#include "rendering/BattleScene.h"
#include "interaction/SettlementCommandController.h"
#include "interaction/SettlementInspectionController.h"
#include "interaction/SettlementObjectPlacementController.h"
#include "world/World.h"
#include "world/generation/SettlementMapGenerator.h"
#include "world/settlements/SettlementMap.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace Paladin
{
    BattleScene::BattleScene(
        const World& world,
        const BattleEncounter& encounter,
        int width,
        int height
    )
    {
        if (!BattleSystem::valid(world, encounter))
        {
            throw std::runtime_error("The encounter is no longer available.");
        }
        const auto player = world.army(encounter.player)->soldiers();
        const auto enemy = world.army(encounter.enemy)->soldiers();
        soldiers_.reserve(player.size() + enemy.size());
        const auto largest = std::max(player.size(), enemy.size());
        int side = std::max(64, int(std::ceil(std::sqrt(double(largest) * 8))));
        std::vector<SettlementTilePosition> lower, upper;
        for (;; side *= 2)
        {
            if (side > 2048)
            {
                throw std::runtime_error(
                    "Insufficient dry deployment space. Use Simulate or "
                    "Retreat."
                );
            }
            SettlementMapGenerationSettings settings;
            settings.localTilesPerWorldTile = side;
            map_ = SettlementMapGenerator{}.generate(
                world.grid(),
                encounter.tile,
                1,
                1,
                world.generationSeed(),
                settings
            );
            lower.clear();
            upper.clear();
            for (int y = 2; y < side / 2 - 4; y += 2)
            {
                for (int x = 2; x < side - 2; x += 2)
                {
                    if (map_->grid().tile({x, y})->terrain == TerrainType::Land)
                    {
                        upper.push_back({x, y});
                    }
                    const int bottom = side - 1 - y;
                    if (map_->grid().tile({x, bottom})->terrain ==
                        TerrainType::Land)
                    {
                        lower.push_back({x, bottom});
                    }
                }
            }
            if (lower.size() >= player.size() && upper.size() >= enemy.size())
            {
                break;
            }
        }
        const auto centerRows = [&](auto& positions, double y)
        {
            std::stable_sort(
                positions.begin(),
                positions.end(),
                [&](const auto& a, const auto& b)
                {
                    const double da =
                        std::abs(a.x - side * .5) + 2 * std::abs(a.y - y);
                    const double db =
                        std::abs(b.x - side * .5) + 2 * std::abs(b.y - y);
                    return da < db;
                }
            );
        };
        centerRows(lower, side * .75);
        centerRows(upper, side * .25);
        const auto deploy =
            [&](std::span<const SoldierId> roster,
                const std::vector<SettlementTilePosition>& positions,
                bool own)
        {
            for (std::size_t i = 0; i < roster.size(); ++i)
            {
                const auto* soldier = world.soldier(roster[i]);
                const auto* home =
                    soldier ? world.settlement(soldier->homeSettlementId())
                            : nullptr;
                const auto* person =
                    home ? home->simulationState().citizens().citizen(
                               soldier->sourceCitizenId()
                           )
                         : nullptr;
                if (!person)
                {
                    throw std::runtime_error(
                        "Missing battle personnel record."
                    );
                }
                const auto p = positions[i];
                map_->naturalFeatures().set(p, NaturalFeatureKind::None);
                soldiers_.push_back(
                    {roster[i],
                     double(p.x),
                     double(p.y),
                     own,
                     person->sex == CitizenSex::Female}
                );
            }
        };
        deploy(player, lower, true);
        deploy(enemy, upper, false);
        metrics_.tilePixels = 2;
        camera_.setPosition(side * .5, side * .5);
        camera_.setZoom(
            std::min(double(width) / side, double(height - 80) / side) / 2
        );
        renderer_.presentation.cloudsEnabled = false;
        layout(height);
    }

    BattleScene::~BattleScene() = default;

    void BattleScene::layout(int height)
    {
        retreat.setBounds({0, float(height - 42), 130, 42});
    }

    void BattleScene::clamp(int width, int height)
    {
        const double fit = std::min(
                               double(width) / map_->grid().width(),
                               double(height - 60) / map_->grid().height()
                           ) /
                           2;
        camera_.setZoom(std::clamp(camera_.zoom(), fit, 24.0));
        camera_.setPosition(
            std::clamp(camera_.tileX(), 0.0, double(map_->grid().width())),
            std::clamp(camera_.tileY(), 0.0, double(map_->grid().height()))
        );
    }

    void BattleScene::update(double seconds, int width, int height)
    {
        const auto* keys = SDL_GetKeyboardState(nullptr);
        const double distance =
            400 * seconds / metrics_.scaledTilePixels(camera_.zoom());
        camera_.setPosition(
            camera_.tileX() +
                (int(keys[SDL_SCANCODE_D]) - int(keys[SDL_SCANCODE_A])) *
                    distance,
            camera_.tileY() +
                (int(keys[SDL_SCANCODE_S]) - int(keys[SDL_SCANCODE_W])) *
                    distance
        );
        clamp(width, height);
    }

    bool BattleScene::handle(const SDL_Event& event, int width, int height)
    {
        if (event.type == SDL_EVENT_MOUSE_MOTION)
        {
            retreat.pointerMoved(event.motion.x, event.motion.y);
        }
        if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST)
        {
            retreat.cancelPress();
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
            event.button.button == SDL_BUTTON_LEFT)
        {
            (void)retreat.pointerPressed(event.button.x, event.button.y);
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
            event.button.button == SDL_BUTTON_LEFT)
        {
            return retreat.pointerReleased(event.button.x, event.button.y);
        }
        if (event.type == SDL_EVENT_MOUSE_WHEEL)
        {
            const double old = metrics_.scaledTilePixels(camera_.zoom());
            const double x = event.wheel.mouse_x - width * .5;
            const double y = event.wheel.mouse_y - height * .5;
            camera_.setZoom(camera_.zoom() * std::pow(1.15, event.wheel.y));
            clamp(width, height);
            const double now = metrics_.scaledTilePixels(camera_.zoom());
            camera_.setPosition(
                camera_.tileX() + x / old - x / now,
                camera_.tileY() + y / old - y / now
            );
            clamp(width, height);
        }
        return false;
    }

    void BattleScene::render(
        Renderer& renderer,
        double hour,
        double sun,
        double seconds
    )
    {
        const SettlementCitizenState empty;
        const SettlementObjectPlacementController placement;
        const SettlementCommandController commands;
        const SettlementInspectionController inspection;
        renderer_.animationSeconds = seconds;
        renderer_.render(
            renderer,
            *map_,
            camera_,
            metrics_,
            placement,
            commands,
            empty,
            inspection,
            1,
            hour,
            sun,
            soldiers_
        );
    }
} // namespace Paladin

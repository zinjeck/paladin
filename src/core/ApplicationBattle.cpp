#include "core/Application.h"
#include "core/SimulationClock.h"
#include "interaction/SettlementPlacementController.h"
#include "rendering/BattleScene.h"
#include "rendering/Renderer.h"
#include "rendering/WorldArmyPresentation.h"
#include "rendering/WorldMapNavigation.h"
#include "rendering/WorldRenderer.h"
#include "simulation/Simulation.h"
#include "ui/CaravanPanel.h"
#include "ui/CityHud.h"
#include "ui/DebugConsole.h"
#include "ui/DiplomacyPanel.h"
#include "ui/EmploymentPanel.h"
#include "ui/FoundingPanel.h"
#include "ui/GrayUiRenderer.h"
#include "ui/LedgerPanel.h"
#include "ui/MilitaryPanel.h"
#include "ui/SimulationSpeedControls.h"
#include "ui/WorldSettlementPanel.h"
#include "world/PlanetAstronomy.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>

namespace Paladin
{
    ArmyId Application::attackTargetAt(float x, float y) const
    {
        if (screen_ != Screen::World || !simulation_ || !selectedWorldArmy_ ||
            battleEncounter_ || !battleResultMessage_.empty() ||
            foundingPanel_->isOpen() ||
            settlementPlacementController_->isActive() ||
            worldSettlementPanel_->choosingDestination() ||
            activeHudContainsPoint(x, y))
        {
            return {};
        }
        const auto& world = simulation_->world();
        const auto* own = world.army(selectedWorldArmy_);
        if (!own || own->ownerRealmId() != simulation_->playerRealmId() ||
            own->engagedOpponent())
        {
            return {};
        }
        const auto& grid = world.grid();
        const double pixels =
            worldRenderer_->globeEnabled
                ? GlobeView::from(
                      *camera_,
                      grid,
                      renderer_->outputWidth(),
                      renderer_->outputHeight()
                  )
                          .radius *
                      6.283185307179586 / grid.width()
                : tileRenderMetrics_->scaledTilePixels(camera_->zoom());
        ArmyId hit;
        double best = std::numeric_limits<double>::max();
        for (const auto& unit : world.armies())
        {
            if (unit.ownerRealmId() == own->ownerRealmId() ||
                unit.garrisoned() || unit.engagedOpponent() ||
                !unit.soldierCount())
            {
                continue;
            }
            const auto p = WorldMapNavigation::annotationPosition(
                *camera_,
                grid,
                renderer_->outputWidth(),
                renderer_->outputHeight(),
                pixels,
                worldRenderer_->globeEnabled,
                unit.visualX() + .5,
                unit.visualY() + .5
            );
            if (!p)
            {
                continue;
            }
            const auto* art =
                worldArmySprite(worldRenderer_->artwork(), world, unit);
            if (!worldArmyHitTest(
                    x,
                    y,
                    p->x,
                    p->y,
                    pixels,
                    art,
                    unit.soldierCount()
                ))
            {
                continue;
            }
            const double distance = std::hypot(p->x - x, p->y - y);
            if (distance < best)
            {
                hit = unit.id();
                best = distance;
            }
        }
        return hit;
    }

    void Application::clearBattle()
    {
        battleScene_.reset();
        battleEncounter_.reset();
        fightButton_.cancelPress();
        simulateButton_.cancelPress();
        encounterRetreat_.cancelPress();
        battleMessage_.clear();
        battleResultMessage_.clear();
        simulateButton_.setText("Simulate");
        SDL_ShowCursor();
    }

    void Application::updateBattleEncounter()
    {
        if (!simulation_ || screen_ == Screen::MainMenu ||
            !battleResultMessage_.empty())
        {
            return;
        }
        if (battleEncounter_)
        {
            if (!BattleSystem::valid(simulation_->world(), *battleEncounter_))
            {
                finishBattle(false);
            }
            return;
        }
        const auto pending = BattleSystem::pendingFor(
            simulation_->world(),
            simulation_->playerRealmId()
        );
        if (!pending)
        {
            return;
        }
        if (screen_ == Screen::City)
        {
            returnToWorldFromSettlement();
        }
        battleEncounter_ = pending;
        preBattlePaused_ = simulationClock_->isPaused();
        preBattleSpeed_ = simulationClock_->speedMultiplier();
        simulationClock_->setPaused(true);
        globePointerDown_ = globeDragging_ = false;
        caravanPointerCaptured_ = militaryPointerCaptured_ =
            simulationControlsCapturedPointer_ = false;
        caravanPanel_->close();
        worldSettlementPanel_->close();
        diplomacyPanel_->close();
        militaryPanel_->close();
        ledgerPanel_->close();
        employmentPanel_->close();
        battleMessage_.clear();
        layoutBattle();
    }

    void Application::layoutBattle()
    {
        const float width =
            std::min(520.F, float(renderer_->outputWidth()) - 24);
        encounterBounds_ = {
            (renderer_->outputWidth() - width) * .5F,
            renderer_->outputHeight() * .5F - 120,
            width,
            240
        };
        const float third = (width - 48) / 3;
        fightButton_.setBounds(
            {encounterBounds_.x + 12, encounterBounds_.y + 181, third, 38}
        );
        simulateButton_.setBounds(
            {encounterBounds_.x + 24 + third,
             encounterBounds_.y + 181,
             third,
             38}
        );
        encounterRetreat_.setBounds(
            {encounterBounds_.x + 36 + 2 * third,
             encounterBounds_.y + 181,
             third,
             38}
        );
        if (battleScene_)
        {
            battleScene_->layout(renderer_->outputHeight());
        }
    }

    void Application::beginBattleScene()
    {
        if (!battleEncounter_ ||
            !BattleSystem::valid(simulation_->world(), *battleEncounter_))
        {
            return;
        }
        try
        {
            auto scene = std::make_unique<BattleScene>(
                simulation_->world(),
                *battleEncounter_,
                renderer_->outputWidth(),
                renderer_->outputHeight()
            );
            battleScene_ = std::move(scene);
            screen_ = Screen::Battle;
            simulationClock_->setPaused(true);
            layoutBattle();
        }
        catch (const std::exception& error)
        {
            battleMessage_ = error.what();
        }
    }

    void Application::finishBattle(bool retreat)
    {
        if (retreat && battleEncounter_ && simulation_)
        {
            BattleSystem::retreat(
                simulation_->world(),
                simulation_->playerRealmId(),
                *battleEncounter_
            );
        }
        clearBattle();
        screen_ = Screen::World;
        globePointerDown_ = globeDragging_ = false;
        if (selectedWorldArmy_ &&
            !simulation_->world().army(selectedWorldArmy_))
        {
            selectedWorldArmy_ = {};
        }
        simulationClock_->setSpeedMultiplier(preBattleSpeed_);
        simulationClock_->setPaused(preBattlePaused_);
    }

    bool Application::handleBattleEvent(const SDL_Event& event)
    {
        if (!battleResultMessage_.empty())
        {
            if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST)
            {
                simulateButton_.cancelPress();
            }
            if (event.type == SDL_EVENT_MOUSE_MOTION)
            {
                simulateButton_.pointerMoved(event.motion.x, event.motion.y);
            }
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                event.button.button == SDL_BUTTON_LEFT)
            {
                (void)simulateButton_.pointerPressed(
                    event.button.x,
                    event.button.y
                );
            }
            if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
                event.button.button == SDL_BUTTON_LEFT &&
                simulateButton_.pointerReleased(event.button.x, event.button.y))
            {
                finishBattle(false);
            }
            return true;
        }
        if (!battleEncounter_ ||
            !BattleSystem::valid(simulation_->world(), *battleEncounter_))
        {
            finishBattle(false);
            return true;
        }
        if (battleScene_)
        {
            if (handleSimulationControlEvent(event))
            {
                return true;
            }
            if (battleScene_->handle(
                    event,
                    renderer_->outputWidth(),
                    renderer_->outputHeight()
                ))
            {
                finishBattle(true);
            }
            return true;
        }
        for (auto* button :
             {&fightButton_, &simulateButton_, &encounterRetreat_})
        {
            if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST)
            {
                button->cancelPress();
            }
            if (event.type == SDL_EVENT_MOUSE_MOTION)
            {
                button->pointerMoved(event.motion.x, event.motion.y);
            }
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                event.button.button == SDL_BUTTON_LEFT)
            {
                (void)button->pointerPressed(event.button.x, event.button.y);
            }
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
            event.button.button == SDL_BUTTON_LEFT)
        {
            const bool fight =
                fightButton_.pointerReleased(event.button.x, event.button.y);
            const bool simulate =
                simulateButton_.pointerReleased(event.button.x, event.button.y);
            const bool retreat = encounterRetreat_.pointerReleased(
                event.button.x,
                event.button.y
            );
            if (fight)
            {
                beginBattleScene();
            }
            else if (retreat)
            {
                finishBattle(true);
            }
            else if (simulate)
            {
                const auto result = BattleSystem::simulate(
                    simulation_->world(),
                    simulation_->playerRealmId(),
                    *battleEncounter_
                );
                if (result.resolved)
                {
                    battleResultMessage_ =
                        "Your losses: " + std::to_string(result.playerLosses) +
                        "    Enemy losses: " +
                        std::to_string(result.enemyLosses);
                    battleMessage_ = result.winner == battleEncounter_->player
                                         ? "Victory"
                                         : "Defeat";
                    battleEncounter_.reset();
                    simulateButton_.setText("Continue");
                    if (!simulation_->world().army(selectedWorldArmy_))
                    {
                        selectedWorldArmy_ = {};
                    }
                }
                else
                {
                    finishBattle(false);
                }
            }
        }
        return true;
    }

    void Application::renderBattleScreen()
    {
        if (!battleScene_)
        {
            return;
        }
        const auto& world = simulation_->world();
        const auto& map = battleScene_->map();
        const double minute = world.time().totalGameMinutes();
        battleScene_->render(
            *renderer_,
            PlanetAstronomy::localMinute(minute, map.planetU) / 60,
            PlanetAstronomy::sunIncidence(
                map.planetU,
                map.planetV,
                minute * 60
            ),
            simulationClock_->presentationSeconds()
        );
        const auto* realm = world.realm(simulation_->playerRealmId());
        cityHud_->setRealmFlag(realm ? realm->flag() : RealmFlag{});
        cityHud_->setCityInformation(
            realm ? std::string(realm->name()) : "Battle",
            world.time().day(),
            world.time().hour(),
            world.time().minute()
        );
        cityHud_->layout(renderer_->outputWidth(), renderer_->outputHeight());
        cityHud_->renderIdentity(*renderer_, *grayUiRenderer_);
        battleScene_->retreat.render(*renderer_, *grayUiRenderer_);
        simulationSpeedControls_->render(*renderer_, *grayUiRenderer_);
    }

    void Application::renderBattleOverlay()
    {
        const auto heading = [&](std::string_view text)
        {
            const BitmapFontRenderer font;
            const float size = std::min(
                4.F,
                (encounterBounds_.width - 40.F) /
                    std::max(1.F, font.measureWidth(text, 1.F))
            );
            grayUiRenderer_->drawLabel(
                *renderer_,
                text,
                encounterBounds_.x +
                    (encounterBounds_.width - font.measureWidth(text, size)) *
                        .5F,
                encounterBounds_.y + 24,
                size
            );
        };
        if (!battleResultMessage_.empty())
        {
            SDL_ShowCursor();
            grayUiRenderer_->drawModalBackdrop(*renderer_);
            grayUiRenderer_->drawPanel(*renderer_, encounterBounds_);
            heading(battleMessage_);
            grayUiRenderer_->drawLabel(
                *renderer_,
                battleResultMessage_,
                encounterBounds_.x + 20,
                encounterBounds_.y + 100,
                2
            );
            simulateButton_.render(*renderer_, *grayUiRenderer_);
            return;
        }
        if (battleEncounter_ && !battleScene_)
        {
            SDL_ShowCursor();
            grayUiRenderer_->drawModalBackdrop(*renderer_);
            grayUiRenderer_->drawPanel(*renderer_, encounterBounds_);
            heading("Initialize battle");
            const auto& world = simulation_->world();
            const auto* player = world.army(battleEncounter_->player);
            const auto* enemy = world.army(battleEncounter_->enemy);
            if (player && enemy)
            {
                grayUiRenderer_->drawLabel(
                    *renderer_,
                    "Your soldiers: " + std::to_string(player->soldierCount()) +
                        "    Enemy: " + std::to_string(enemy->soldierCount()),
                    encounterBounds_.x + 20,
                    encounterBounds_.y + 85,
                    2
                );
            }
            if (!battleMessage_.empty())
            {
                grayUiRenderer_->drawLabel(
                    *renderer_,
                    battleMessage_.substr(0, 58),
                    encounterBounds_.x + 12,
                    encounterBounds_.y + 132,
                    1.25F
                );
            }
            fightButton_.render(*renderer_, *grayUiRenderer_);
            simulateButton_.render(*renderer_, *grayUiRenderer_);
            encounterRetreat_.render(*renderer_, *grayUiRenderer_);
            return;
        }
        float x = 0, y = 0;
        SDL_GetMouseState(&x, &y);
        if (!attackTargetAt(x, y))
        {
            SDL_ShowCursor();
            return;
        }
        SDL_HideCursor();
        // Native-pixel cursor: two crossed blades, dark outline and red steel.
        const RenderColor dark{57, 43, 60, 255}, red{215, 80, 86, 255};
        for (int i = 0; i < 12; ++i)
        {
            renderer_
                ->fillRectangle(x - 12 + i * 2, y - 12 + i * 2, 5, 5, dark);
            renderer_
                ->fillRectangle(x + 12 - i * 2, y - 12 + i * 2, 5, 5, dark);
            renderer_->fillRectangle(x - 11 + i * 2, y - 11 + i * 2, 3, 3, red);
            renderer_->fillRectangle(x + 13 - i * 2, y - 11 + i * 2, 3, 3, red);
        }
        renderer_->fillRectangle(x - 14, y + 7, 12, 3, red);
        renderer_->fillRectangle(x + 5, y + 7, 12, 3, red);
    }
} // namespace Paladin

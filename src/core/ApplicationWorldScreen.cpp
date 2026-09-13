#include "core/Application.h"
#include "core/SimulationClock.h"
#include "interaction/GlobeCameraNavigation.h"
#include "interaction/SettlementPlacementController.h"
#include "rendering/Renderer.h"
#include "rendering/WorldRenderer.h"
#include "simulation/Simulation.h"
#include "ui/BitmapFontRenderer.h"
#include "ui/CityHud.h"
#include "ui/DebugConsole.h"
#include "ui/EmploymentPanel.h"
#include "ui/FoundingPanel.h"
#include "ui/GrayUiRenderer.h"
#include "ui/SimulationSpeedControls.h"
#include "ui/WorldHud.h"
#include "world/World.h"
#include "world/settlements/SettlementMap.h"
#include <SDL3/SDL.h>

namespace Paladin
{
    void Application::layoutWorldScreen()
    {
        worldHud_->layout(renderer_->outputWidth(), renderer_->outputHeight());

        const Realm* playerRealm =
            simulation_->world().realm(simulation_->playerRealmId());

        worldHud_->setCapitalEstablished(
            playerRealm && playerRealm->capitalSettlementId().isValid()
        );

        worldHud_->setSimulationControlsUnlocked(simulationControlsUnlocked_);

        foundingPanel_->layout(
            renderer_->outputWidth(),
            renderer_->outputHeight()
        );
    }

    void Application::updateWorldScreen()
    {
        if (!worldRenderer_->terrainDetailReady())
        {
            return;
        }

        if (!foundingPanel_->isOpen())
        {
            const double frameDeltaSeconds = simulationClock_->frameDeltaSeconds();
            updateCameraMovement(frameDeltaSeconds);
            updateCameraZoom(frameDeltaSeconds);

            if (!debugConsole_->wantsKeyboard() && worldRenderer_->globeEnabled)
            {
                const bool* keyboardState = SDL_GetKeyboardState(nullptr);
                double rollDirection = 0.0;
                if (keyboardState[SDL_SCANCODE_Q])
                {
                    rollDirection += 1.0;
                }
                if (keyboardState[SDL_SCANCODE_E])
                {
                    rollDirection -= 1.0;
                }
                if (rollDirection != 0.0)
                {
                    GlobeCameraNavigation::roll(
                        *camera_,
                        simulation_->world().grid(),
                        renderer_->outputWidth(),
                        renderer_->outputHeight(),
                        rollDirection *
                            cameraNavigationPolicy_.globeRollRadiansPerSecond *
                            frameDeltaSeconds
                    );
                }
            }
        }

        if (settlementPlacementController_->isSelecting())
        {
            float mouseX = 0.0F;
            float mouseY = 0.0F;

            SDL_GetMouseState(&mouseX, &mouseY);

            updateSettlementPlacementHover(
                static_cast<double>(mouseX),
                static_cast<double>(mouseY)
            );
        }
    }

    void Application::renderWorldScreen()
    {
        if (!worldRenderer_->prepareTerrain(*renderer_, simulation_->world()))
        {
            renderer_->fillRectangle(
                0,
                0,
                float(renderer_->outputWidth()),
                float(renderer_->outputHeight()),
                {8, 15, 27, 255}
            );
            const float x = renderer_->outputWidth() * .5F,
                        y = renderer_->outputHeight() * .5F;
            grayUiRenderer_
                ->drawTitle(*renderer_, "PREPARING WORLD", x, y - 40);
            grayUiRenderer_->drawLabel(
                *renderer_,
                "Preparing terrain " +
                    std::to_string(
                        int(worldRenderer_->terrainPreparationProgress() * 100)
                    ) +
                    "%",
                x - 150,
                y + 12,
                2
            );
            return;
        }

        std::optional<WorldPlacementMarker> placementMarker;
        const std::optional<WorldTilePosition> hoveredPosition =
            settlementPlacementController_->hoveredPosition();
        const std::optional<WorldTilePosition> lockedPosition =
            settlementPlacementController_->lockedPosition();
        const bool foundingOriginChosen =
            foundingPanel_->isOpen() &&
            foundingPanel_->mode() == FoundingPanelMode::Founding &&
            !foundingPanel_->identity().realmOriginId.empty();

        if (settlementPlacementController_->isSelecting() && hoveredPosition)
        {
            const RenderColor markerColor =
                settlementPlacementController_->hasValidPlacement(
                    simulation_->world()
                )
                    ? RenderColor{121, 181, 109, 235}
                    : RenderColor{215, 80, 86, 235};
            placementMarker = WorldPlacementMarker{*hoveredPosition, markerColor};
        }
        else if (lockedPosition && !foundingOriginChosen)
        {
            const MapColor selectedColor = foundingPanel_->isOpen()
                                               ? foundingPanel_->selectedColor()
                                               : MapColor{255, 215, 131};
            placementMarker = WorldPlacementMarker{
                *lockedPosition,
                RenderColor{
                    selectedColor.red,
                    selectedColor.green,
                    selectedColor.blue,
                    235
                }
            };
        }

        worldRenderer_->animationSeconds =
            simulationClock_->presentationSeconds();
        worldRenderer_->render(
            *renderer_,
            simulation_->world(),
            *camera_,
            *tileRenderMetrics_,
            {},
            {},
            {},
            placementMarker
        );

        renderWorldManagement();
        worldRenderer_->renderNavigator(
            *renderer_,
            simulation_->world(),
            *camera_,
            *tileRenderMetrics_,
            *grayUiRenderer_
        );

        worldHud_->render(
            *renderer_,
            *grayUiRenderer_,
            settlementPlacementController_->isActive()
        );

        if (simulationControlsUnlocked_)
        {
            simulationSpeedControls_->render(*renderer_, *grayUiRenderer_);
        }

        if (!foundingPanel_->isOpen() &&
            !settlementPlacementController_->isActive())
        {
            renderWorldRealmInspection();
        }
        foundingPanel_->render(*renderer_, *grayUiRenderer_);
    }

    void Application::renderWorldRealmInspection()
    {
        const auto& world = simulation_->world();
        const auto* city = world.settlement(inspectedWorldSettlementId_);
        const auto* realm = city ? world.realm(city->ownerRealmId()) : nullptr;
        if (!city || !realm || employmentPanel_->isOpen())
        {
            return;
        }
        const UiRectangle box{16, 182, 426, 360};
        grayUiRenderer_->drawPanel(*renderer_, box);
        float y = box.y + 16;
        const auto line = [&](const std::string& text, float size = 2.0F)
        {
            const float measured =
                BitmapFontRenderer{}.measureWidth(text, size);
            const float fitted = measured > box.width - 28
                                     ? size * (box.width - 28) / measured
                                     : size;
            grayUiRenderer_->drawLabel(*renderer_, text, box.x + 14, y, fitted);
            y += 26;
        };
        line(std::string(realm->name()));
        line(
            std::string(city->name()) + " - " +
                std::string(settlementKindName(city->kind())),
            1.7F
        );
        line("Population " + std::to_string(city->population()), 1.8F);
        const auto& ruler = realm->ruler;
        line(
            ruler.vacant ? "Ruler: vacant"
                         : "Ruler: " + ruler.name + " (" +
                               std::to_string(int(ruler.age)) + ")",
            1.8F
        );
        line(
            "Dynasty " + std::to_string(ruler.dynasty) + " / Reign " +
                std::to_string(ruler.reign),
            1.7F
        );
        const auto axis =
            [&](const char* first, const char* second, const RulerAxis& value)
        {
            const float left = box.x + 14, width = box.width - 28;
            constexpr float size = 1.4F;
            grayUiRenderer_->drawLabel(*renderer_, first, left, y, size);
            grayUiRenderer_->drawLabel(
                *renderer_,
                second,
                left + width - BitmapFontRenderer{}.measureWidth(second, size),
                y,
                size
            );
            renderer_->fillRectangle(left, y + 17, width, 5, {48, 54, 65, 255});
            renderer_->fillRectangle(
                left + width * .5F,
                y + 15,
                1,
                9,
                {131, 140, 150, 255}
            );
            // The first pole is on the left: a larger first() value moves left.
            if (!ruler.vacant)
            {
                const float marker =
                    left + float(value.opposite()) * (width - 6);
                renderer_->fillRectangle(
                    marker,
                    y + 13,
                    6,
                    13,
                    {255, 215, 131, 255}
                );
            }
            y += 43;
        };
        axis("Militarism", "Pacifism", ruler.personality.militarism);
        axis("Isolationism", "Mercantilism", ruler.personality.isolationism);
        axis(
            "Authoritarianism",
            "Libertarianism",
            ruler.personality.authoritarianism
        );
        axis("Elitism", "Egalitarianism", ruler.personality.elitism);
        line("Click elsewhere or Esc to close", 1.4F);
    }

    void Application::renderWorldManagement()
    {
        if (!simulationControlsUnlocked_ ||
            settlementPlacementController_->isActive())
        {
            return;
        }
        cityHud_->setWorldMode(true);
        employmentPanel_->setWorldMode(true);
        cityHud_->layout(renderer_->outputWidth(), renderer_->outputHeight());
        const auto& world = simulation_->world();
        const auto* realm = world.realm(simulation_->playerRealmId());
        const auto* active =
            world.settlement(simulation_->presentedSettlementId());
        std::uint64_t population = 0;
        for (const auto& settlement : world.settlements())
        {
            if (settlement.ownerRealmId() == simulation_->playerRealmId())
            {
                population += settlement.population();
            }
        }
        cityHud_->setSettlementStatus(true, population);
        cityHud_->setTreasuryGold(realm ? realm->treasury->balance : 0);
        cityHud_->setCityInformation(
            realm ? std::string(realm->name()) : "",
            world.time().day(),
            world.time().hour(),
            world.time().minute()
        );
        cityHud_->setActiveSettlementName(
            active ? std::string(active->name()) : ""
        );
        cityHud_->render(*renderer_, *grayUiRenderer_);
        if (const auto* map = simulation_->settlementMap(
                simulation_->presentedSettlementId()
            );
            map && active)
        {
            employmentPanel_->render(
                *renderer_,
                *grayUiRenderer_,
                *map,
                active->simulationState().citizens(),
                world.time().totalGameMinutes()
            );
        }
    }

} // namespace Paladin

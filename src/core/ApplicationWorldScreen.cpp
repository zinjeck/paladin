#include "core/Application.h"
#include "core/SimulationClock.h"
#include "interaction/GlobeCameraNavigation.h"
#include "interaction/SettlementPlacementController.h"
#include "rendering/Renderer.h"
#include "rendering/WorldRenderer.h"
#include "simulation/Simulation.h"
#include "ui/CityHud.h"
#include "ui/DebugConsole.h"
#include "ui/EmploymentPanel.h"
#include "ui/FoundingPanel.h"
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

        foundingPanel_->render(*renderer_, *grayUiRenderer_);
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

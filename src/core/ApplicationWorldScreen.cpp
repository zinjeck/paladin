#include "core/Application.h"
#include "core/SimulationClock.h"
#include "interaction/GlobeCameraNavigation.h"
#include "interaction/SettlementPlacementController.h"
#include "rendering/Renderer.h"
#include "rendering/WorldRenderer.h"
#include "simulation/Simulation.h"
#include "ui/BitmapFontRenderer.h"
#include "ui/CaravanPanel.h"
#include "ui/CityHud.h"
#include "ui/DebugConsole.h"
#include "ui/DiplomacyPanel.h"
#include "ui/EmploymentPanel.h"
#include "ui/FoundingPanel.h"
#include "ui/GrayUiRenderer.h"
#include "ui/RealmFlagRenderer.h"
#include "ui/SimulationSpeedControls.h"
#include "ui/WorldHud.h"
#include "ui/WorldSettlementPanel.h"
#include "world/World.h"
#include "world/settlements/SettlementMap.h"
#include <SDL3/SDL.h>

namespace Paladin
{
    void Application::layoutWorldScreen()
    {
        worldHud_->layout(renderer_->outputWidth(), renderer_->outputHeight());
        caravanPanel_->layout(renderer_->outputWidth(), renderer_->outputHeight(), simulation_->world(), simulation_->playerRealmId());
        worldSettlementPanel_->layout(renderer_->outputWidth(),renderer_->outputHeight(),simulation_->world(),simulation_->playerRealmId());
        diplomacyPanel_->layout(renderer_->outputWidth(),renderer_->outputHeight(),simulation_->world(),simulation_->playerRealmId());

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
            if (!worldSettlementPanel_->wantsKeyboard()) updateCameraMovement(frameDeltaSeconds);
            updateCameraZoom(frameDeltaSeconds);

            if (!debugConsole_->wantsKeyboard() && !worldSettlementPanel_->wantsKeyboard() && worldRenderer_->globeEnabled)
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
        // Selection is application state, independent of asynchronous terrain
        // preparation. Keep it current even on a loading/projection frame.
        worldRenderer_->selectedRealm = diplomacyPanel_->selection();
        worldRenderer_->selectedArmy = selectedWorldArmy_;
        worldRenderer_->selectedCaravan = caravanPanel_->selection();
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
            caravanPanel_->render(*renderer_, *grayUiRenderer_, simulation_->world());
            diplomacyPanel_->render(*renderer_,*grayUiRenderer_,simulation_->world(),simulation_->playerRealmId());
            worldSettlementPanel_->render(*renderer_,*grayUiRenderer_,simulation_->world(),simulation_->playerRealmId());
        }
        if (!foundingPanel_->isOpen() &&
            !settlementPlacementController_->isActive())
        {
            const auto selected = worldRenderer_->selectedRealm;
            if (selected && selected != simulation_->playerRealmId())
            {
                if (const auto* realm = simulation_->world().realm(selected))
                {
                    drawRealmFlag(
                        *renderer_,
                        realm->flag(),
                        157,
                        float(renderer_->outputHeight()) - 104,
                        6
                    );
                }
            }
        }
        if (settlementPlacementController_->isSelecting() && hoveredPosition &&
            !foundingPanel_->isOpen())
        {
            const auto& policy = simulation_->world().territoryFoundationPolicy();
            const auto kind = foundingPanel_->settlementKind();
            float x = 0, y = 0;
            SDL_GetMouseState(&x, &y);
            worldRenderer_->renderRegionSurvey(*renderer_, *grayUiRenderer_,
                simulation_->world(), *hoveredPosition,
                settlementRegionDimension(policy.settlementRegionWidth, kind),
                settlementRegionDimension(policy.settlementRegionHeight, kind), x, y);
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
        cityHud_->setRealmFlag(realm ? realm->flag() : RealmFlag{});
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
            if (const auto* realm=world.realm(simulation_->playerRealmId()))
                employmentPanel_->setRealmWorkDayHours(realm->workDayHours());
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

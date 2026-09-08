#include "core/Application.h"
#include "core/SimulationClock.h"
#include "interaction/GlobeCameraNavigation.h"
#include "interaction/SettlementPlacementController.h"
#include "rendering/OverlayRenderer.h"
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
#include <array>
#include <span>

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
        std::array<TileOverlayRenderItem, 1> overlays{};
        std::array<TileOutlineRenderItem, 1> outlines{};
        std::size_t overlayCount = 0;
        std::size_t outlineCount = 0;

        const std::optional<WorldTilePosition> hoveredPosition =
            settlementPlacementController_->hoveredPosition();

        const std::optional<WorldTilePosition> lockedPosition =
            settlementPlacementController_->lockedPosition();

        if (settlementPlacementController_->isSelecting() && hoveredPosition)
        {
            const bool validPlacement =
                settlementPlacementController_->hasValidPlacement(
                    simulation_->world()
                );

            const TerritoryFoundationPolicy& territoryPolicy =
                simulation_->world().territoryFoundationPolicy();

            const double regionX =
                static_cast<double>(hoveredPosition->x) -
                static_cast<double>(territoryPolicy.settlementRegionWidth / 2);

            const double regionY =
                static_cast<double>(hoveredPosition->y) -
                static_cast<double>(territoryPolicy.settlementRegionHeight / 2);

            const RenderColor previewColor =
                validPlacement ? RenderColor{121, 181, 109, 220}
                               : RenderColor{215, 80, 86, 230};

            overlays[0] = {
                regionX,
                regionY,
                static_cast<double>(territoryPolicy.settlementRegionWidth),
                static_cast<double>(territoryPolicy.settlementRegionHeight),
                validPlacement ? RenderColor{121, 181, 109, 34}
                               : RenderColor{215, 80, 86, 42}
            };

            outlines[0] = {
                regionX,
                regionY,
                static_cast<double>(territoryPolicy.settlementRegionWidth),
                static_cast<double>(territoryPolicy.settlementRegionHeight),
                2.0F,
                previewColor
            };

            overlayCount = 1;
            outlineCount = 1;
        }
        else if (lockedPosition)
        {
            const MapColor selectedColor = foundingPanel_->isOpen()
                                               ? foundingPanel_->selectedColor()
                                               : MapColor{255, 215, 131};

            const TerritoryFoundationPolicy& territoryPolicy =
                simulation_->world().territoryFoundationPolicy();

            const double regionX =
                static_cast<double>(lockedPosition->x) -
                static_cast<double>(territoryPolicy.settlementRegionWidth / 2);

            const double regionY =
                static_cast<double>(lockedPosition->y) -
                static_cast<double>(territoryPolicy.settlementRegionHeight / 2);

            overlays[0] = {
                regionX,
                regionY,
                static_cast<double>(territoryPolicy.settlementRegionWidth),
                static_cast<double>(territoryPolicy.settlementRegionHeight),
                RenderColor{
                    selectedColor.red,
                    selectedColor.green,
                    selectedColor.blue,
                    38
                }
            };

            outlines[0] = {
                regionX,
                regionY,
                static_cast<double>(territoryPolicy.settlementRegionWidth),
                static_cast<double>(territoryPolicy.settlementRegionHeight),
                2.0F,
                RenderColor{
                    selectedColor.red,
                    selectedColor.green,
                    selectedColor.blue,
                    235
                }
            };

            overlayCount = 1;
            outlineCount = 1;
        }

        worldRenderer_->animationSeconds =
            simulationClock_->presentationSeconds();
        worldRenderer_->render(
            *renderer_,
            simulation_->world(),
            *camera_,
            *tileRenderMetrics_,
            {},
            std::span<const TileOverlayRenderItem>(
                overlays.data(),
                overlayCount
            ),
            std::span<const TileOutlineRenderItem>(
                outlines.data(),
                outlineCount
            )
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

#include "core/Application.h"
#include "interaction/SettlementCommandController.h"
#include "interaction/SettlementInspectionController.h"
#include "interaction/SettlementObjectPlacementController.h"
#include "platform/Window.h"
#include "rendering/Camera2D.h"
#include "rendering/CityRenderer.h"
#include "rendering/Renderer.h"
#include "simulation/Simulation.h"
#include "ui/CityHud.h"
#include "ui/EmploymentPanel.h"
#include "ui/SettlementInspectionPanel.h"
#include "world/World.h"
#include "world/settlements/SettlementMap.h"
#include <SDL3/SDL.h>
#include <cmath>

namespace Paladin
{
    bool Application::handleCityRenameEvent(
        const SDL_Event& event,
        SettlementMap& map
    )
    {
        if (event.type == SDL_EVENT_TEXT_INPUT)
        {
            settlementInspectionPanel_->appendText(event.text.text);
            return true;
        }
        if (event.type == SDL_EVENT_KEY_DOWN)
        {
            if (event.key.scancode == SDL_SCANCODE_BACKSPACE)
            {
                settlementInspectionPanel_->backspace();
            }
            else if (
                event.key.scancode == SDL_SCANCODE_RETURN ||
                event.key.scancode == SDL_SCANCODE_KP_ENTER
            )
            {
                settlementInspectionPanel_->finishRename(map, true);
            }
            else if (event.key.scancode == SDL_SCANCODE_ESCAPE)
            {
                settlementInspectionPanel_->finishRename(map, false);
            }
            if (!settlementInspectionPanel_->editingName())
            {
                SDL_StopTextInput(window_->nativeHandle());
            }
            return true;
        }

        return false;
    }

    bool Application::handleCityEmploymentEvent(
        const SDL_Event& event,
        SettlementMap& map,
        SettlementCitizenState& citizens
    )
    {
        if (event.type == SDL_EVENT_MOUSE_MOTION &&
            employmentPanel_->pointerMoved(event.motion.x, event.motion.y))
        {
            return true;
        }
        if ((event.type == SDL_EVENT_KEY_DOWN &&
             event.key.scancode == SDL_SCANCODE_ESCAPE) ||
            (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
             event.button.button == SDL_BUTTON_RIGHT))
        {
            employmentPanel_->close();
            employmentCapturedPointer_ = false;
            return true;
        }
        if (event.type == SDL_EVENT_MOUSE_WHEEL &&
            employmentPanel_
                ->containsPoint(event.wheel.mouse_x, event.wheel.mouse_y))
        {
            employmentPanel_->scroll(event.wheel.y);
            return true;
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
            event.button.button == SDL_BUTTON_LEFT)
        {
            employmentCapturedPointer_ = employmentPanel_->pointerPressed(
                event.button.x,
                event.button.y
            );
            if (employmentCapturedPointer_)
            {
                return true;
            }
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
            event.button.button == SDL_BUTTON_LEFT &&
            employmentCapturedPointer_)
        {
            employmentPanel_->pointerReleased(
                event.button.x,
                event.button.y,
                map,
                citizens,
                simulation_->world().time().totalGameMinutes()
            );
            if (const auto change = employmentPanel_->takeWorkDayChange())
            {
                simulation_->changeWorkDay(
                    activeCitySettlementId_,
                    change->realm,
                    change->delta
                );
            }
            if (const auto admission = employmentPanel_->takeAdmission())
            {
                simulation_->admitImmigrants(
                    activeCitySettlementId_,
                    *admission
                );
            }
            employmentCapturedPointer_ = false;
            if (const auto* workplace = map.employment().workplace(
                    employmentPanel_->takeFocusedWorkplace()
                ))
            {
                camera_->setPosition(
                    workplace->footprint.topLeft.x +
                        workplace->footprint.width * 0.5,
                    workplace->footprint.topLeft.y +
                        workplace->footprint.height * 0.5
                );
                clampCameraToWorld();
                settlementInspectionController_->selectWorkplace(
                    workplace->objectId,
                    workplace->constructionId
                );
                settlementInspectionPanel_->clearLayout();
            }
            return true;
        }

        return false;
    }

    void Application::handleCityPointerMotion(const SDL_Event& event)
    {
        settlementInspectionPanel_->pointerMoved(
            event.motion.x,
            event.motion.y
        );
        cityHud_->pointerMoved(event.motion.x, event.motion.y);

        settlementObjectPlacementController_->pointerMoved(
            activeHudContainsPoint(event.motion.x, event.motion.y)
                ? std::nullopt
                : cityTileAtScreen(event.motion.x, event.motion.y)
        );
        settlementCommandController_->pointerMoved(
            activeHudContainsPoint(event.motion.x, event.motion.y)
                ? std::nullopt
                : cityTileAtScreen(event.motion.x, event.motion.y)
        );
    }

    void Application::handleCityPointerPressed(const SDL_Event& event)
    {
        const bool inspectionPanelCapturedPointer =
            settlementInspectionPanel_->pointerPressed(
                event.button.x,
                event.button.y
            );

        cityHudCapturedPointer_ =
            inspectionPanelCapturedPointer ||
            cityHud_->pointerPressed(event.button.x, event.button.y);

        if (cityHudCapturedPointer_ && !inspectionPanelCapturedPointer &&
            !cityHud_->roofControlAt(event.button.x, event.button.y))
        {
            settlementInspectionController_->clear();
        }

        SettlementMap* settlementMap =
            simulation_->settlementMap(activeCitySettlementId_);

        if (!cityHudCapturedPointer_ &&
            !activeHudContainsPoint(event.button.x, event.button.y) &&
            settlementMap)
        {
            const auto tile = cityTileAtScreen(event.button.x, event.button.y);
            if (settlementCommandController_->isActive())
            {
                settlementCommandController_->pointerPressed(tile);
            }
            else if (settlementObjectPlacementController_->isActive())
            {
                const SettlementPlacementCommitResult result =
                    settlementObjectPlacementController_->pointerPressed(
                        tile,
                        *settlementMap
                    );

                if (result == SettlementPlacementCommitResult::CompletedObject)
                {
                    Settlement* settlement = simulation_->world().settlement(
                        activeCitySettlementId_
                    );

                    if (settlement)
                    {
                        settlement->simulationState()
                            .citizens()
                            .placeUnpositionedCitizens(*settlementMap);
                    }
                }
            }
            else if (tile)
            {
                const Settlement* settlement =
                    simulation_->world().settlement(activeCitySettlementId_);

                if (settlement)
                {
                    static_cast<void>(settlementInspectionController_->selectAt(
                        *tile,
                        settlementMap->objectState(),
                        settlement->simulationState().citizens(),
                        event.button.x <
                            static_cast<float>(renderer_->outputWidth()) * 0.5F,
                        &settlementMap->logistics
                    ));
                }
            }
            else
            {
                settlementInspectionController_->clear();
            }
        }
    }

    void Application::handleCityPointerReleased(const SDL_Event& event)
    {
        auto* currentMap = simulation_->settlementMap(activeCitySettlementId_);
        auto* currentSettlement =
            simulation_->world().settlement(activeCitySettlementId_);

        if (currentMap && currentSettlement)
        {
            settlementInspectionPanel_->pointerReleased(
                event.button.x,
                event.button.y,
                *currentMap,
                currentSettlement->simulationState().citizens(),
                simulation_->world().time().totalGameMinutes()
            );
            if (settlementInspectionPanel_->editingName())
            {
                SDL_StartTextInput(window_->nativeHandle());
            }
            else
            {
                SDL_StopTextInput(window_->nativeHandle());
            }
            if (const auto id =
                    settlementInspectionPanel_->takeCitizenNavigation())
            {
                if (const auto* citizen =
                        currentSettlement->simulationState().citizens().citizen(
                            id
                        ))
                {
                    settlementInspectionController_->selectCitizen(id);
                    settlementInspectionPanel_->clearLayout();
                    camera_->setPosition(
                        citizen->visualX() + .5,
                        citizen->visualY() + .5
                    );
                    return;
                }
            }
        }
        const CityHudAction action =
            cityHud_->pointerReleased(event.button.x, event.button.y);
        if (handleReportAction(action))
        {
            cityHudCapturedPointer_ = false;
            return;
        }

        if (action == CityHudAction::Population ||
            action == CityHudAction::Laws ||
            action == CityHudAction::Employment ||
            action == CityHudAction::Technology ||
            action == CityHudAction::Military ||
            action == CityHudAction::Economy)
        {
            const auto section =
                action == CityHudAction::Population   ? "Population"
                : action == CityHudAction::Laws       ? "Laws"
                : action == CityHudAction::Technology ? "Technology"
                : action == CityHudAction::Military   ? "Military"
                : action == CityHudAction::Economy    ? "Economy"
                                                      : "Employment";
            employmentPanel_->toggle(section);
            settlementInspectionController_->clear();
            settlementInspectionPanel_->clearLayout();
            SDL_StopTextInput(window_->nativeHandle());
            settlementObjectPlacementController_->cancelPlacement();
            settlementCommandController_->cancel();
        }
        else if (action == CityHudAction::ToggleRoofs)
        {
            cityRenderer_->presentation.roofsVisible =
                !cityRenderer_->presentation.roofsVisible;
            cityHud_->setRoofsVisible(cityRenderer_->presentation.roofsVisible);
        }
        else if (action == CityHudAction::ToggleEnvironmentArt)
        {
            SceneSpriteLibrary::setEnvironmentArtEnabled(
                !SceneSpriteLibrary::environmentArtEnabled()
            );
        }
        else if (action == CityHudAction::Back)
        {
            returnToWorldFromCity();
        }
        else if (action == CityHudAction::BeginObjectPlacement)
        {
            settlementInspectionController_->clear();
            settlementCommandController_->cancel();
            static_cast<void>(
                settlementObjectPlacementController_->beginPlacement(
                    cityHud_->selectedObjectTypeId()
                )
            );
        }
        else if (action == CityHudAction::BeginCommand)
        {
            settlementInspectionController_->clear();
            settlementObjectPlacementController_->cancelPlacement();
            static_cast<void>(settlementCommandController_->begin(
                cityHud_->selectedCommandTypeId()
            ));
        }
        else if (!cityHudCapturedPointer_)
        {
            SettlementMap* settlementMap =
                simulation_->settlementMap(activeCitySettlementId_);

            if (settlementMap)
            {
                const auto tile =
                    cityTileAtScreen(event.button.x, event.button.y);
                Settlement* settlement =
                    simulation_->world().settlement(activeCitySettlementId_);
                if (settlementCommandController_->isActive() && settlement)
                {
                    static_cast<void>(
                        settlementCommandController_->pointerReleased(
                            tile,
                            *settlementMap,
                            settlement->simulationState().citizens()
                        )
                    );
                }
                else if (settlementObjectPlacementController_->isActive())
                {
                    static_cast<void>(
                        settlementObjectPlacementController_
                            ->pointerReleased(tile, *settlementMap)
                    );
                }
            }
        }

        cityHudCapturedPointer_ = false;
    }

    void Application::handleCityEvent(const SDL_Event& event)
    {
        synchronizeCityStatus();
        auto* currentMap = simulation_->settlementMap(activeCitySettlementId_);
        auto* currentSettlement =
            simulation_->world().settlement(activeCitySettlementId_);
        if (currentMap && currentSettlement)
        {
            if (settlementInspectionPanel_->editingName() &&
                handleCityRenameEvent(event, *currentMap))
            {
                return;
            }
            if (employmentPanel_->isOpen() &&
                handleCityEmploymentEvent(
                    event,
                    *currentMap,
                    currentSettlement->simulationState().citizens()
                ))
            {
                return;
            }
        }
        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
            event.key.scancode == SDL_SCANCODE_F7)
        {
            camera_->setZoom(
                std::max(8.0, std::round(camera_->zoom() / 8.0) * 8.0)
            );
            return;
        }
        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
            event.key.scancode == SDL_SCANCODE_F6)
        {
            cityRenderer_->reloadArt();
            return;
        }
        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
            event.key.scancode == SDL_SCANCODE_O)
        {
            cityRenderer_->presentation.roofsVisible =
                !cityRenderer_->presentation.roofsVisible;
            cityHud_->setRoofsVisible(cityRenderer_->presentation.roofsVisible);
            return;
        }
        if (event.type == SDL_EVENT_MOUSE_MOTION)
        {
            handleCityPointerMotion(event);
        }


        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
            settlementObjectPlacementController_->isActive() &&
            event.key.scancode == SDL_SCANCODE_F)
        {
            settlementObjectPlacementController_->rotatePlacement();
            return;
        }
        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
            settlementObjectPlacementController_->isActive() &&
            (event.key.scancode == SDL_SCANCODE_E ||
             event.key.scancode == SDL_SCANCODE_R))
        {
            settlementObjectPlacementController_->rotateDoor(
                event.key.scancode == SDL_SCANCODE_E ? -1 : 1
            );
            return;
        }

        if (event.type == SDL_EVENT_KEY_DOWN &&
            event.key.scancode == SDL_SCANCODE_ESCAPE)
        {
            returnToWorldFromCity();
            return;
        }

        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
            event.button.button == SDL_BUTTON_RIGHT)
        {
            if (!settlementObjectPlacementController_->stepBack())
            {
                cityHud_->closeCategoryMenus();
                settlementCommandController_->cancel();
                settlementInspectionController_->clear();
            }
            return;
        }


        handleCameraZoomEvent(event);
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
            event.button.button == SDL_BUTTON_LEFT)
        {
            handleCityPointerPressed(event);
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
            event.button.button == SDL_BUTTON_LEFT)
        {
            handleCityPointerReleased(event);
        }
    }

} // namespace Paladin

#include "core/Application.h"
#include "interaction/SettlementInspectionController.h"
#include "interaction/SettlementPlacementController.h"
#include "platform/Window.h"
#include "rendering/Camera2D.h"
#include "rendering/Renderer.h"
#include "rendering/SceneSpriteLibrary.h"
#include "rendering/TileRenderMetrics.h"
#include "simulation/Simulation.h"
#include "ui/CityHud.h"
#include "ui/EmploymentPanel.h"
#include "ui/FoundingPanel.h"
#include "ui/SettlementInspectionPanel.h"
#include "ui/WorldHud.h"
#include "world/World.h"
#include "world/settlements/SettlementMap.h"
#include <SDL3/SDL.h>

namespace Paladin
{
    void Application::handleFoundingEvent(const SDL_Event& event)
    {
        if (event.type == SDL_EVENT_MOUSE_MOTION)
        {
            foundingPanel_->pointerMoved(event.motion.x, event.motion.y);
        }

        if (event.type == SDL_EVENT_TEXT_INPUT)
        {
            foundingPanel_->appendText(event.text.text);
        }

        if (event.type == SDL_EVENT_KEY_DOWN)
        {
            if (event.key.scancode == SDL_SCANCODE_ESCAPE)
            {
                if (!foundingPanel_->closeTopLayer())
                {
                    cancelFoundingFlow();
                }
            }
            else if (event.key.scancode == SDL_SCANCODE_BACKSPACE)
            {
                foundingPanel_->backspace();
            }
            else if (event.key.scancode == SDL_SCANCODE_TAB)
            {
                foundingPanel_->focusNextField();
            }
            else if (
                event.key.scancode == SDL_SCANCODE_RETURN ||
                event.key.scancode == SDL_SCANCODE_KP_ENTER
            )
            {
                const FoundingPanelAction action = foundingPanel_->submit();

                if (action == FoundingPanelAction::Confirm)
                {
                    confirmFoundingFlow();
                }
            }
        }

        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
            event.button.button == SDL_BUTTON_RIGHT)
        {
            if (!foundingPanel_->closeTopLayer())
            {
                cancelFoundingFlow();
            }
        }

        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
            event.button.button == SDL_BUTTON_LEFT)
        {
            foundingPanel_->pointerPressed(event.button.x, event.button.y);
        }

        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
            event.button.button == SDL_BUTTON_LEFT)
        {
            const FoundingPanelAction action =
                foundingPanel_->pointerReleased(event.button.x, event.button.y);

            if (action == FoundingPanelAction::Cancel)
            {
                cancelFoundingFlow();
            }
            else if (action == FoundingPanelAction::Confirm)
            {
                confirmFoundingFlow();
            }
        }

        return;
    }

    void Application::handleWorldPointerPressed(const SDL_Event& event)
    {
        const bool hudCapturedPointer =
            worldHud_->pointerPressed(event.button.x, event.button.y);

        if (!hudCapturedPointer &&
            settlementPlacementController_->isSelecting())
        {
            updateSettlementPlacementHover(
                static_cast<double>(event.button.x),
                static_cast<double>(event.button.y)
            );

            const bool locked =
                settlementPlacementController_->lockHoveredSelection(
                    simulation_->world()
                );

            if (locked)
            {
                if (movingCapital_)
                {
                    if (simulation_->movePlayerCapital(
                            *settlementPlacementController_->lockedPosition()
                        ))
                    {
                        settlementPlacementController_->cancelSelection();
                        movingCapital_ = false;
                    }
                }
                else
                {
                    if (foundingAdditionalSettlement_)
                    {
                        foundingPanel_->openForSettlement();
                    }
                    else
                    {
                        foundingPanel_->open();
                    }
                    SDL_StartTextInput(window_->nativeHandle());
                }
            }
        }
    }

    void Application::handleWorldPointerReleased(const SDL_Event& event)
    {
        const WorldHudAction action =
            worldHud_->pointerReleased(event.button.x, event.button.y);

        if (action == WorldHudAction::SelectRegion)
        {
            movingCapital_ = false;
            settlementPlacementController_->beginSelection(
                simulation_->playerRealmId(),
                foundingAdditionalSettlement_
            );
        }
        else if (action == WorldHudAction::MoveCapital)
        {
            movingCapital_ = true;
            settlementPlacementController_->beginSelection(
                simulation_->playerRealmId()
            );
        }
        else if (
            action == WorldHudAction::RenameCapital ||
            action == WorldHudAction::EditRealm
        )
        {
            const World& world = simulation_->world();
            const Realm* realm = world.realm(simulation_->playerRealmId());
            const Settlement* capital =
                realm ? world.settlement(realm->capitalSettlementId())
                      : nullptr;
            const Culture* culture =
                realm ? world.culture(realm->primaryCultureId()) : nullptr;

            if (realm && capital && culture)
            {
                if (action == WorldHudAction::RenameCapital)
                {
                    foundingPanel_->openForCapitalRename(capital->name());
                }
                else
                {
                    foundingPanel_->openForRealmEdit(
                        {std::string(realm->name()),
                         std::string(culture->name()),
                         std::string(capital->name()),
                         realm->mapColor(),
                         std::string(realm->startingOriginId()),
                         realm->flag()}
                    );
                }

                SDL_StartTextInput(window_->nativeHandle());
            }
        }
        else if (action == WorldHudAction::Play)
        {
            enterPlayerCapitalCity();
        }
        else if (action == WorldHudAction::Back)
        {
            endWorldSession();
        }
    }

    void Application::handleWorldEvent(const SDL_Event& event)
    {
        if (handleWorldManagement(event))
        {
            return;
        }
        if (foundingPanel_->isOpen())
        {
            handleFoundingEvent(event);
            return;
        }


        if (event.type == SDL_EVENT_MOUSE_MOTION)
        {
            worldHud_->pointerMoved(event.motion.x, event.motion.y);
        }

        if (event.type == SDL_EVENT_KEY_DOWN &&
            event.key.scancode == SDL_SCANCODE_ESCAPE)
        {
            settlementPlacementController_->cancelSelection();
            movingCapital_ = false;
            foundingAdditionalSettlement_ = false;
            worldHud_->setAdditionalSelection(false);
            employmentPanel_->close();
        }


        handleCameraZoomEvent(event);


        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
            event.button.button == SDL_BUTTON_RIGHT)
        {
            settlementPlacementController_->cancelSelection();
            movingCapital_ = false;
            foundingAdditionalSettlement_ = false;
            worldHud_->setAdditionalSelection(false);
        }


        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
            event.button.button == SDL_BUTTON_LEFT)
        {
            handleWorldPointerPressed(event);
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
            event.button.button == SDL_BUTTON_LEFT)
        {
            handleWorldPointerReleased(event);
        }
    }

    bool Application::handleWorldManagement(const SDL_Event& event)
    {
        if (screen_ != Screen::World || !simulationControlsUnlocked_ ||
            foundingPanel_->isOpen() ||
            settlementPlacementController_->isActive())
        {
            return false;
        }
        auto* settlement = simulation_->world().settlement(
            simulation_->presentedSettlementId()
        );
        auto* map =
            simulation_->settlementMap(simulation_->presentedSettlementId());
        if (!settlement || !map)
        {
            return false;
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
            event.button.button == SDL_BUTTON_RIGHT &&
            employmentPanel_->isOpen())
        {
            employmentPanel_->close();
            employmentCapturedPointer_ = false;
            return true;
        }
        if (event.type == SDL_EVENT_MOUSE_MOTION)
        {
            cityHud_->pointerMoved(event.motion.x, event.motion.y);
            if (employmentPanel_->pointerMoved(event.motion.x, event.motion.y))
            {
                return true;
            }
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
            cityHudCapturedPointer_ =
                cityHud_->pointerPressed(event.button.x, event.button.y);
            return cityHudCapturedPointer_;
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
            event.button.button == SDL_BUTTON_LEFT)
        {
            if (employmentCapturedPointer_)
            {
                employmentPanel_->pointerReleased(
                    event.button.x,
                    event.button.y,
                    *map,
                    settlement->simulationState().citizens(),
                    simulation_->world().time().totalGameMinutes()
                );
                employmentCapturedPointer_ = false;
                if (employmentPanel_->takeFoundSettlement())
                {
                    foundingAdditionalSettlement_ = true;
                    movingCapital_ = false;
                    worldHud_->setAdditionalSelection(true);
                    settlementPlacementController_->beginSelection(
                        simulation_->playerRealmId(),
                        true
                    );
                }
                return true;
            }
            if (cityHudCapturedPointer_)
            {
                const auto action =
                    cityHud_->pointerReleased(event.button.x, event.button.y);
                cityHudCapturedPointer_ = false;
                if (handleReportAction(action))
                {
                    return true;
                }
                if (action == CityHudAction::ToggleEnvironmentArt)
                {
                    SceneSpriteLibrary::setEnvironmentArtEnabled(
                        !SceneSpriteLibrary::environmentArtEnabled()
                    );
                    return true;
                }
                if (action != CityHudAction::None)
                {
                    const auto* section =
                        action == CityHudAction::Laws         ? "Laws"
                        : action == CityHudAction::Employment ? "Employment"
                        : action == CityHudAction::Technology ? "Technology"
                        : action == CityHudAction::Military   ? "Military"
                        : action == CityHudAction::Economy    ? "Economy"
                                                              : "Population";
                    employmentPanel_->toggle(section);
                }
                return true;
            }
            // Selecting an owned settlement changes the active context;
            // double-click enters it.
            if (!activeHudContainsPoint(event.button.x, event.button.y))
            {
                const double pixels =
                    tileRenderMetrics_->scaledTilePixels(camera_->zoom());
                const double x =
                    camera_->tileX() +
                    (event.button.x - renderer_->outputWidth() * .5) / pixels;
                const double y =
                    camera_->tileY() +
                    (event.button.y - renderer_->outputHeight() * .5) / pixels;
                SettlementId nearest;
                double best = std::max(1.5, 8 / pixels);
                for (const auto& city : simulation_->world().settlements())
                {
                    if (city.ownerRealmId() != simulation_->playerRealmId())
                    {
                        continue;
                    }
                    const double distance = std::hypot(
                        x - city.position().x - .5,
                        y - city.position().y - .5
                    );
                    if (distance < best)
                    {
                        best = distance;
                        nearest = city.id();
                    }
                }
                if (nearest && simulation_->setPresentedSettlement(nearest))
                {
                    if (event.button.clicks >= 2)
                    {
                        enterPlayerCapitalCity();
                    }
                    return true;
                }
            }
        }
        if (event.type == SDL_EVENT_MOUSE_WHEEL &&
            employmentPanel_
                ->containsPoint(event.wheel.mouse_x, event.wheel.mouse_y))
        {
            return true;
        }
        return false;
    }

} // namespace Paladin

#include "core/Application.h"
#include "debug/ConsoleCommand.h"
#include "ui/DebugConsole.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include <iomanip>
#include <sstream>

#include "core/SimulationClock.h"
#include "interaction/SettlementPlacementController.h"
#include "interaction/SettlementObjectPlacementController.h"
#include "interaction/SettlementCommandController.h"
#include "interaction/SettlementInspectionController.h"
#include "platform/Window.h"
#include "rendering/Camera2D.h"
#include "rendering/CityRenderer.h"
#include "rendering/OverlayRenderer.h"
#include "rendering/Renderer.h"
#include "rendering/TileRenderMetrics.h"
#include "rendering/WorldRenderer.h"
#include "simulation/Simulation.h"
#include "ui/GrayUiRenderer.h"
#include "ui/CityHud.h"
#include "ui/EmploymentPanel.h"
#include "ui/FoundingPanel.h"
#include "ui/MainMenu.h"
#include "ui/SimulationSpeedControls.h"
#include "ui/SettlementInspectionPanel.h"
#include "ui/WorldHud.h"
#include "world/World.h"
#include "world/settlements/SettlementMap.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <span>

namespace Paladin
{
    Application::Application()
    {
        if (!SDL_Init(SDL_INIT_VIDEO))
        {
            SDL_Log(
                "SDL_Init failed: %s",
                SDL_GetError()
            );

            return;
        }

        sdlInitialized_ = true;

        window_ = std::make_unique<Window>(
            "Paladin",
            1280,
            720
        );

        if (!window_->isValid())
        {
            window_.reset();
            return;
        }

        renderer_ =
            std::make_unique<Renderer>(
                window_->nativeHandle()
            );

        if (!renderer_->isValid())
        {
            renderer_.reset();
            return;
        }

        simulationClock_ =
            std::make_unique<SimulationClock>(20.0);

        grayUiRenderer_ =
            std::make_unique<GrayUiRenderer>();

        mainMenu_ =
            std::make_unique<MainMenu>();

        worldHud_ =
            std::make_unique<WorldHud>();

        employmentPanel_ = std::make_unique<EmploymentPanel>();
        debugConsole_ = std::make_unique<DebugConsole>();
        cityHud_ =
            std::make_unique<CityHud>();

        simulationSpeedControls_ =
            std::make_unique<SimulationSpeedControls>();

        foundingPanel_ =
            std::make_unique<FoundingPanel>();

        settlementInspectionController_ =
            std::make_unique<SettlementInspectionController>();

        settlementInspectionPanel_ =
            std::make_unique<SettlementInspectionPanel>();
    }

    Application::~Application()
    {
        if (window_)
        {
            SDL_StopTextInput(window_->nativeHandle());
        }

        tileRenderMetrics_.reset();
        cityCameras_.clear();
        cityRenderer_.reset();
        worldRenderer_.reset();
        settlementPlacementController_.reset();
        settlementObjectPlacementController_.reset();
        settlementInspectionPanel_.reset();
        settlementInspectionController_.reset();
        camera_.reset();
        savedWorldCamera_.reset();
        simulation_.reset();

        worldHud_.reset();
        debugConsole_.reset();
        employmentPanel_.reset();
        cityHud_.reset();
        simulationSpeedControls_.reset();
        foundingPanel_.reset();
        mainMenu_.reset();
        grayUiRenderer_.reset();
        simulationClock_.reset();
        renderer_.reset();
        window_.reset();

        if (sdlInitialized_)
        {
            SDL_Quit();
        }
    }

    int Application::run()
    {
        if (
            !sdlInitialized_ ||
            !window_ ||
            !renderer_ ||
            !simulationClock_ ||
            !grayUiRenderer_ ||
            !mainMenu_ ||
            !worldHud_ ||
            !cityHud_ ||
            !simulationSpeedControls_ ||
            !foundingPanel_ ||
            !settlementInspectionController_ ||
            !settlementInspectionPanel_
        )
        {
            return 1;
        }

        bool running = true;

        while (running)
        {
            simulationClock_->beginFrame();

            if (screen_ == Screen::MainMenu)
            {
                mainMenu_->layout(
                    renderer_->outputWidth(),
                    renderer_->outputHeight()
                );
            }
            else if (screen_ == Screen::World)
            {
                worldHud_->layout(
                    renderer_->outputWidth(),
                    renderer_->outputHeight()
                );

                const Polity* playerPolity = simulation_->world().polity(
                    simulation_->playerPolityId()
                );

                worldHud_->setCapitalEstablished(
                    playerPolity &&
                    playerPolity->capitalSettlementId().isValid()
                );

                worldHud_->setSimulationControlsUnlocked(
                    simulationControlsUnlocked_
                );

                foundingPanel_->layout(
                    renderer_->outputWidth(),
                    renderer_->outputHeight()
                );
            }
            else
            {
                cityHud_->layout(
                    renderer_->outputWidth(),
                    renderer_->outputHeight()
                );
            }

            const bool simulationControlsVisible =
                screen_ == Screen::City ||
                (
                    screen_ == Screen::World &&
                    simulationControlsUnlocked_
                );

            if (simulationControlsVisible)
            {
                simulationSpeedControls_->layout(
                    renderer_->outputWidth()
                );

                simulationSpeedControls_->setPlaybackState(
                    simulationClock_->isPaused(),
                    simulationClock_->speedMultiplier()
                );
            }

            debugConsole_->layout(
                renderer_->outputWidth(),
                renderer_->outputHeight()
            );
            SDL_Event event;

            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_EVENT_QUIT)
                {
                    running = false;
                    continue;
                }

                if (screen_ != Screen::MainMenu && simulation_)
                {
                    const bool wasText = debugConsole_->wantsText();
                    const bool consumed = debugConsole_->handle(event);
                    if (wasText != debugConsole_->wantsText())
                    {
                        if (debugConsole_->wantsText())
                        {
                            SDL_StartTextInput(window_->nativeHandle());
                        }
                        else
                        {
                            SDL_StopTextInput(window_->nativeHandle());
                        }
                    }
                    if (consumed)
                    {
                        if (event.type == SDL_EVENT_KEY_DOWN &&
                            event.key.scancode == SDL_SCANCODE_GRAVE)
                        {
                            if (auto* map = simulation_->settlementMap(
                                    activeCitySettlementId_
                                ))
                            {
                                settlementInspectionPanel_->finishRename(
                                    *map,
                                    false
                                );
                            }
                            settlementObjectPlacementController_
                                ->cancelPlacement();
                            settlementCommandController_->cancel();
                        }
                        const auto command = debugConsole_->takeCommand();
                        if (!command.empty())
                        {
                            executeConsoleCommand(command);
                        }
                        continue;
                    }
                }
                if (screen_ == Screen::MainMenu)
                {
                    if (event.type == SDL_EVENT_MOUSE_MOTION)
                    {
                        mainMenu_->pointerMoved(
                            event.motion.x,
                            event.motion.y
                        );
                    }

                    if (
                        event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                        event.button.button == SDL_BUTTON_LEFT
                    )
                    {
                        mainMenu_->pointerPressed(
                            event.button.x,
                            event.button.y
                        );
                    }

                    if (
                        event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
                        event.button.button == SDL_BUTTON_LEFT
                    )
                    {
                        const MainMenuAction action =
                            mainMenu_->pointerReleased(
                                event.button.x,
                                event.button.y
                            );

                        if (action == MainMenuAction::Play)
                        {
                            startWorldSession();
                        }
                        else if (action == MainMenuAction::Exit)
                        {
                            running = false;
                        }

                        // Tutorial intentionally has no action yet.
                    }

                    continue;
                }

                if (simulationControlsVisible)
                {
                    if (event.type == SDL_EVENT_MOUSE_MOTION)
                    {
                        simulationSpeedControls_->pointerMoved(
                            event.motion.x,
                            event.motion.y
                        );
                    }

                    if (
                        event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                        event.button.button == SDL_BUTTON_LEFT
                    )
                    {
                        simulationControlsCapturedPointer_ =
                            simulationSpeedControls_->pointerPressed(
                                event.button.x,
                                event.button.y
                            );

                        if (simulationControlsCapturedPointer_)
                        {
                            continue;
                        }
                    }

                    if (
                        event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
                        event.button.button == SDL_BUTTON_LEFT
                    )
                    {
                        const SimulationSpeedControlAction action =
                            simulationSpeedControls_->pointerReleased(
                                event.button.x,
                                event.button.y
                            );

                        const bool captured =
                            simulationControlsCapturedPointer_;

                        simulationControlsCapturedPointer_ = false;

                        if (action == SimulationSpeedControlAction::Pause)
                        {
                            simulationClock_->setPaused(true);
                        }
                        else if (
                            action == SimulationSpeedControlAction::Normal
                        )
                        {
                            simulationClock_->setSpeedMultiplier(1.0);
                            simulationClock_->setPaused(false);
                        }
                        else if (
                            action == SimulationSpeedControlAction::Double
                        )
                        {
                            simulationClock_->setSpeedMultiplier(2.0);
                            simulationClock_->setPaused(false);
                        }
                        else if (
                            action == SimulationSpeedControlAction::Fast
                        )
                        {
                            const double nextFastSpeed =
                                !simulationClock_->isPaused() &&
                                std::abs(
                                    simulationClock_->speedMultiplier() - 3.0
                                ) < 0.001
                                    ? 5.0
                                    : 3.0;

                            simulationClock_->setSpeedMultiplier(
                                nextFastSpeed
                            );
                            simulationClock_->setPaused(false);
                        }

                        if (
                            action != SimulationSpeedControlAction::None
                        )
                        {
                            simulationSpeedControls_->setPlaybackState(
                                simulationClock_->isPaused(),
                                simulationClock_->speedMultiplier()
                            );
                        }

                        if (captured)
                        {
                            continue;
                        }
                    }
                }

                if (screen_ == Screen::City)
                {
                    SettlementMap* currentMap = simulation_->settlementMap(activeCitySettlementId_);
                    Settlement* currentSettlement = simulation_->world().settlement(activeCitySettlementId_);
                    if (currentMap && currentSettlement)
                    {
                        auto& citizens = currentSettlement->simulationState().citizens();
                        currentMap->employment().synchronize(currentMap->objectState(), citizens);
                        currentMap->employment().record(simulation_->world().time().totalGameMinutes(), citizens);
                        cityHud_->setSettlementStatus(currentMap->objectState().hasCityKeep(), citizens.citizens().size());
                        if (settlementInspectionPanel_->editingName())
                        {
                            if (event.type == SDL_EVENT_TEXT_INPUT)
                            {
                                settlementInspectionPanel_->appendText(event.text.text);
                                continue;
                            }
                            if (event.type == SDL_EVENT_KEY_DOWN)
                            {
                                if (event.key.scancode == SDL_SCANCODE_BACKSPACE)
                                    settlementInspectionPanel_->backspace();
                                else if (event.key.scancode == SDL_SCANCODE_RETURN || event.key.scancode == SDL_SCANCODE_KP_ENTER)
                                    settlementInspectionPanel_->finishRename(*currentMap, true);
                                else if (event.key.scancode == SDL_SCANCODE_ESCAPE)
                                    settlementInspectionPanel_->finishRename(*currentMap, false);
                                if (!settlementInspectionPanel_->editingName()) SDL_StopTextInput(window_->nativeHandle());
                                continue;
                            }
                        }
                        if (employmentPanel_->isOpen())
                        {
                            if ((event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE)
                                || (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_RIGHT))
                            {
                                employmentPanel_->close();
                                employmentCapturedPointer_ = false;
                                continue;
                            }
                            if (event.type == SDL_EVENT_MOUSE_WHEEL
                                && employmentPanel_->containsPoint(event.wheel.mouse_x, event.wheel.mouse_y))
                            {
                                employmentPanel_->scroll(event.wheel.y);
                                continue;
                            }
                            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT)
                            {
                                employmentCapturedPointer_ = employmentPanel_->pointerPressed(event.button.x, event.button.y);
                                if (employmentCapturedPointer_) continue;
                            }
                            if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT
                                && employmentCapturedPointer_)
                            {
                                employmentPanel_->pointerReleased(event.button.x, event.button.y, *currentMap, citizens,
                                    simulation_->world().time().totalGameMinutes());
                                employmentCapturedPointer_ = false;
                                if (const auto* workplace = currentMap->employment().workplace(employmentPanel_->takeFocusedWorkplace()))
                                {
                                    camera_->setPosition(workplace->footprint.topLeft.x + workplace->footprint.width * 0.5,
                                        workplace->footprint.topLeft.y + workplace->footprint.height * 0.5);
                                    clampCameraToWorld();
                                    settlementInspectionController_->selectWorkplace(workplace->objectId, workplace->constructionId);
                                    settlementInspectionPanel_->clearLayout();
                                }
                                continue;
                            }
                        }
                    }
                    if (event.type == SDL_EVENT_MOUSE_MOTION)
                    {
                        settlementInspectionPanel_->pointerMoved(event.motion.x, event.motion.y);
                        cityHud_->pointerMoved(
                            event.motion.x,
                            event.motion.y
                        );

                        settlementObjectPlacementController_->pointerMoved(
                            activeHudContainsPoint(
                                event.motion.x,
                                event.motion.y
                            )
                                ? std::nullopt
                                : cityTileAtScreen(
                                    event.motion.x,
                                    event.motion.y
                                )
                        );
                        settlementCommandController_->pointerMoved(
                            activeHudContainsPoint(
                                event.motion.x,
                                event.motion.y
                            )
                                ? std::nullopt
                                : cityTileAtScreen(
                                    event.motion.x,
                                    event.motion.y
                                )
                        );
                    }

                    if (
                        event.type == SDL_EVENT_KEY_DOWN &&
                        event.key.scancode == SDL_SCANCODE_ESCAPE
                    )
                    {
                        returnToWorldFromCity();
                        continue;
                    }

                    if (
                        event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                        event.button.button == SDL_BUTTON_RIGHT
                    )
                    {
                        settlementObjectPlacementController_
                            ->cancelPlacement();
                        settlementCommandController_->cancel();
                        settlementInspectionController_->clear();
                        continue;
                    }

                    if (
                        event.type == SDL_EVENT_KEY_DOWN &&
                        !event.key.repeat &&
                        (
                            event.key.scancode == SDL_SCANCODE_EQUALS ||
                            event.key.scancode == SDL_SCANCODE_KP_PLUS ||
                            event.key.scancode == SDL_SCANCODE_MINUS ||
                            event.key.scancode == SDL_SCANCODE_KP_MINUS
                        )
                    )
                    {
                        float mouseX = 0.0F;
                        float mouseY = 0.0F;
                        SDL_GetMouseState(&mouseX, &mouseY);

                        const bool zoomingIn =
                            event.key.scancode == SDL_SCANCODE_EQUALS ||
                            event.key.scancode == SDL_SCANCODE_KP_PLUS;

                        applyCameraZoom(
                            zoomingIn ? 1.15 : 0.85,
                            static_cast<double>(mouseX),
                            static_cast<double>(mouseY)
                        );
                    }

                    if (event.type == SDL_EVENT_MOUSE_WHEEL)
                    {
                        double wheelDelta =
                            static_cast<double>(event.wheel.y);

                        if (
                            event.wheel.direction ==
                            SDL_MOUSEWHEEL_FLIPPED
                        )
                        {
                            wheelDelta = -wheelDelta;
                        }

                        const double multiplier = wheelDelta > 0.0
                            ? std::pow(1.15, wheelDelta)
                            : std::pow(0.85, -wheelDelta);

                        applyCameraZoom(
                            multiplier,
                            static_cast<double>(event.wheel.mouse_x),
                            static_cast<double>(event.wheel.mouse_y)
                        );
                    }

                    if (
                        event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                        event.button.button == SDL_BUTTON_LEFT
                    )
                    {
                        const bool inspectionPanelCapturedPointer =
                            settlementInspectionPanel_->pointerPressed(
                                event.button.x,
                                event.button.y
                            );

                        cityHudCapturedPointer_ =
                            inspectionPanelCapturedPointer ||
                            cityHud_->pointerPressed(
                                event.button.x,
                                event.button.y
                            );

                        if (
                            cityHudCapturedPointer_ &&
                            !inspectionPanelCapturedPointer
                        )
                        {
                            settlementInspectionController_->clear();
                        }

                        SettlementMap* settlementMap =
                            simulation_->settlementMap(
                                activeCitySettlementId_
                            );

                        if (
                            !cityHudCapturedPointer_ &&
                            !activeHudContainsPoint(
                                event.button.x,
                                event.button.y
                            ) &&
                            settlementMap
                        )
                        {
                            const auto tile = cityTileAtScreen(
                                event.button.x,
                                event.button.y
                            );
                            if (settlementCommandController_->isActive())
                            {
                                settlementCommandController_->pointerPressed(
                                    tile
                                );
                            }
                            else if (
                                settlementObjectPlacementController_
                                    ->isActive()
                            )
                            {
                                const SettlementPlacementCommitResult result =
                                    settlementObjectPlacementController_
                                        ->pointerPressed(tile, *settlementMap)
                                ;

                                if (
                                    result ==
                                        SettlementPlacementCommitResult::
                                            CompletedObject
                                )
                                {
                                    Settlement* settlement =
                                        simulation_->world().settlement(
                                            activeCitySettlementId_
                                        );

                                    if (settlement)
                                    {
                                        settlement->simulationState()
                                            .citizens()
                                            .placeUnpositionedCitizens(
                                                *settlementMap
                                            );
                                    }
                                }
                            }
                            else if (tile)
                            {
                                const Settlement* settlement =
                                    simulation_->world().settlement(
                                        activeCitySettlementId_
                                    );

                                if (settlement)
                                {
                                    static_cast<void>(
                                        settlementInspectionController_
                                            ->selectAt(
                                                *tile,
                                                settlementMap->objectState(),
                                                settlement->simulationState()
                                                    .citizens(),
                                                event.button.x <
                                                    static_cast<float>(
                                                        renderer_
                                                            ->outputWidth()
                                                    ) * 0.5F
                                            )
                                    );
                                }
                            }
                            else
                            {
                                settlementInspectionController_->clear();
                            }
                        }
                    }

                    if (
                        event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
                        event.button.button == SDL_BUTTON_LEFT
                    )
                    {
                        if (currentMap && currentSettlement)
                        {
                            settlementInspectionPanel_->pointerReleased(event.button.x, event.button.y,
                                *currentMap, currentSettlement->simulationState().citizens(),
                                simulation_->world().time().totalGameMinutes());
                            if (settlementInspectionPanel_->editingName()) SDL_StartTextInput(window_->nativeHandle());
                            else SDL_StopTextInput(window_->nativeHandle());
                        }
                        const CityHudAction action =
                            cityHud_->pointerReleased(
                                event.button.x,
                                event.button.y
                            );

                        if (action == CityHudAction::Employment || action == CityHudAction::Technology
                            || action == CityHudAction::Military || action == CityHudAction::Economy)
                        {
                            const auto section = action == CityHudAction::Technology ? "Technology"
                                : action == CityHudAction::Military ? "Military"
                                : action == CityHudAction::Economy ? "Economy" : "Employment";
                            employmentPanel_->toggle(section);
                            settlementInspectionController_->clear();
                            settlementInspectionPanel_->clearLayout();
                            SDL_StopTextInput(window_->nativeHandle());
                            settlementObjectPlacementController_->cancelPlacement();
                            settlementCommandController_->cancel();
                        }
                        else if (action == CityHudAction::Back)
                        {
                            returnToWorldFromCity();
                        }
                        else if (
                            action ==
                                CityHudAction::BeginObjectPlacement
                        )
                        {
                            settlementInspectionController_->clear();
                            settlementCommandController_->cancel();
                            static_cast<void>(
                                settlementObjectPlacementController_
                                    ->beginPlacement(
                                        cityHud_->selectedObjectTypeId()
                                    )
                            );
                        }
                        else if (action == CityHudAction::BeginCommand)
                        {
                            settlementInspectionController_->clear();
                            settlementObjectPlacementController_
                                ->cancelPlacement();
                            static_cast<void>(
                                settlementCommandController_->begin(
                                    cityHud_->selectedCommandTypeId()
                                )
                            );
                        }
                        else if (!cityHudCapturedPointer_)
                        {
                            SettlementMap* settlementMap =
                                simulation_->settlementMap(
                                    activeCitySettlementId_
                                );

                            if (settlementMap)
                            {
                                const auto tile = cityTileAtScreen(
                                    event.button.x,
                                    event.button.y
                                );
                                Settlement* settlement =
                                    simulation_->world().settlement(
                                        activeCitySettlementId_
                                    );
                                if (
                                    settlementCommandController_->isActive() &&
                                    settlement
                                )
                                {
                                    static_cast<void>(
                                        settlementCommandController_
                                            ->pointerReleased(
                                                tile,
                                                *settlementMap,
                                                settlement->simulationState()
                                                    .citizens()
                                            )
                                    );
                                }
                                else if (
                                    settlementObjectPlacementController_
                                        ->isActive()
                                )
                                {
                                    static_cast<void>(
                                        settlementObjectPlacementController_
                                            ->pointerReleased(
                                                tile,
                                                *settlementMap
                                            )
                                    );
                                }
                            }
                        }

                        cityHudCapturedPointer_ = false;
                    }

                    continue;
                }

                if (foundingPanel_->isOpen())
                {
                    if (event.type == SDL_EVENT_MOUSE_MOTION)
                    {
                        foundingPanel_->pointerMoved(
                            event.motion.x,
                            event.motion.y
                        );
                    }

                    if (event.type == SDL_EVENT_TEXT_INPUT)
                    {
                        foundingPanel_->appendText(
                            event.text.text
                        );
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
                        else if (
                            event.key.scancode
                            == SDL_SCANCODE_BACKSPACE
                        )
                        {
                            foundingPanel_->backspace();
                        }
                        else if (
                            event.key.scancode == SDL_SCANCODE_TAB
                        )
                        {
                            foundingPanel_->focusNextField();
                        }
                        else if (
                            event.key.scancode == SDL_SCANCODE_RETURN ||
                            event.key.scancode == SDL_SCANCODE_KP_ENTER
                        )
                        {
                            const FoundingPanelAction action =
                                foundingPanel_->submit();

                            if (
                                action == FoundingPanelAction::Confirm
                            )
                            {
                                confirmFoundingFlow();
                            }
                        }
                    }

                    if (
                        event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                        event.button.button == SDL_BUTTON_RIGHT
                    )
                    {
                        if (!foundingPanel_->closeTopLayer())
                        {
                            cancelFoundingFlow();
                        }
                    }

                    if (
                        event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                        event.button.button == SDL_BUTTON_LEFT
                    )
                    {
                        foundingPanel_->pointerPressed(
                            event.button.x,
                            event.button.y
                        );
                    }

                    if (
                        event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
                        event.button.button == SDL_BUTTON_LEFT
                    )
                    {
                        const FoundingPanelAction action =
                            foundingPanel_->pointerReleased(
                                event.button.x,
                                event.button.y
                            );

                        if (action == FoundingPanelAction::Cancel)
                        {
                            cancelFoundingFlow();
                        }
                        else if (
                            action == FoundingPanelAction::Confirm
                        )
                        {
                            confirmFoundingFlow();
                        }
                    }

                    continue;
                }

                if (event.type == SDL_EVENT_MOUSE_MOTION)
                {
                    worldHud_->pointerMoved(
                        event.motion.x,
                        event.motion.y
                    );
                }

                if (
                    event.type == SDL_EVENT_KEY_DOWN &&
                    event.key.scancode == SDL_SCANCODE_ESCAPE
                )
                {
                    settlementPlacementController_
                        ->cancelSelection();
                    movingCapital_ = false;
                }

                if (
                    event.type == SDL_EVENT_KEY_DOWN &&
                    !event.key.repeat &&
                    (
                        event.key.scancode == SDL_SCANCODE_EQUALS ||
                        event.key.scancode == SDL_SCANCODE_KP_PLUS ||
                        event.key.scancode == SDL_SCANCODE_MINUS ||
                        event.key.scancode == SDL_SCANCODE_KP_MINUS
                    )
                )
                {
                    float mouseX = 0.0F;
                    float mouseY = 0.0F;
                    SDL_GetMouseState(&mouseX, &mouseY);

                    const bool zoomingIn =
                        event.key.scancode == SDL_SCANCODE_EQUALS ||
                        event.key.scancode == SDL_SCANCODE_KP_PLUS;

                    applyCameraZoom(
                        zoomingIn ? 1.15 : 0.85,
                        static_cast<double>(mouseX),
                        static_cast<double>(mouseY)
                    );
                }

                if (event.type == SDL_EVENT_MOUSE_WHEEL)
                {
                    double wheelDelta =
                        static_cast<double>(event.wheel.y);

                    if (
                        event.wheel.direction
                        == SDL_MOUSEWHEEL_FLIPPED
                    )
                    {
                        wheelDelta = -wheelDelta;
                    }

                    constexpr double zoomInPerWheelStep = 1.15;
                    constexpr double zoomOutPerWheelStep = 0.85;

                    const double multiplier =
                        wheelDelta > 0.0
                            ? std::pow(
                                zoomInPerWheelStep,
                                wheelDelta
                            )
                            : std::pow(
                                zoomOutPerWheelStep,
                                -wheelDelta
                            );

                    applyCameraZoom(
                        multiplier,
                        static_cast<double>(event.wheel.mouse_x),
                        static_cast<double>(event.wheel.mouse_y)
                    );
                }

                if (
                    event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                    event.button.button == SDL_BUTTON_RIGHT
                )
                {
                    settlementPlacementController_
                        ->cancelSelection();
                    movingCapital_ = false;
                }

                if (
                    event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                    event.button.button == SDL_BUTTON_LEFT
                )
                {
                    const bool hudCapturedPointer =
                        worldHud_->pointerPressed(
                            event.button.x,
                            event.button.y
                        );

                    if (
                        !hudCapturedPointer &&
                        settlementPlacementController_->isSelecting()
                    )
                    {
                        updateSettlementPlacementHover(
                            static_cast<double>(event.button.x),
                            static_cast<double>(event.button.y)
                        );

                        const bool locked =
                            settlementPlacementController_
                                ->lockHoveredSelection(
                                    simulation_->world()
                                );

                        if (locked)
                        {
                            if (movingCapital_)
                            {
                                if (simulation_->movePlayerCapital(
                                    *settlementPlacementController_
                                        ->lockedPosition()
                                ))
                                {
                                    settlementPlacementController_
                                        ->cancelSelection();
                                    movingCapital_ = false;
                                }
                            }
                            else
                            {
                                foundingPanel_->open();
                                SDL_StartTextInput(
                                    window_->nativeHandle()
                                );
                            }
                        }
                    }
                }

                if (
                    event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
                    event.button.button == SDL_BUTTON_LEFT
                )
                {
                    const WorldHudAction action =
                        worldHud_->pointerReleased(
                            event.button.x,
                            event.button.y
                        );

                    if (action == WorldHudAction::SelectRegion)
                    {
                        movingCapital_ = false;
                        settlementPlacementController_
                            ->beginSelection();
                    }
                    else if (action == WorldHudAction::MoveCapital)
                    {
                        movingCapital_ = true;
                        settlementPlacementController_->beginSelection();
                    }
                    else if (
                        action == WorldHudAction::RenameCapital ||
                        action == WorldHudAction::EditPolity
                    )
                    {
                        const World& world = simulation_->world();
                        const Polity* polity = world.polity(
                            simulation_->playerPolityId()
                        );
                        const Settlement* capital = polity
                            ? world.settlement(polity->capitalSettlementId())
                            : nullptr;
                        const Culture* culture = polity
                            ? world.culture(polity->primaryCultureId())
                            : nullptr;

                        if (polity && capital && culture)
                        {
                            if (action == WorldHudAction::RenameCapital)
                            {
                                foundingPanel_->openForCapitalRename(
                                    capital->name()
                                );
                            }
                            else
                            {
                                foundingPanel_->openForPolityEdit({
                                    std::string(polity->name()),
                                    std::string(culture->name()),
                                    std::string(capital->name()),
                                    polity->mapColor(),
                                    std::string(polity->startingOriginId()),
                                    polity->flag()
                                });
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
            }

            if (!running)
            {
                break;
            }

            if (screen_ == Screen::MainMenu)
            {
                while (simulationClock_->shouldTick())
                {
                    simulationClock_->consumeTick();
                }

                renderer_->beginFrame();
                mainMenu_->render(
                    *renderer_,
                    *grayUiRenderer_
                );
                if (screen_ != Screen::MainMenu && simulation_)
                {
                    renderDebug();
                }
                renderer_->endFrame();
                continue;
            }

            if (screen_ == Screen::City)
            {
                if (!settlementInspectionPanel_->editingName())
                {
                    updateCameraMovement(simulationClock_->frameDeltaSeconds());
                    updateCameraZoom(simulationClock_->frameDeltaSeconds());
                }

                if (settlementObjectPlacementController_->isActive())
                {
                    float mouseX = 0.0F;
                    float mouseY = 0.0F;
                    SDL_GetMouseState(&mouseX, &mouseY);

                    settlementObjectPlacementController_->pointerMoved(
                        cityHud_->containsInteractivePoint(mouseX, mouseY)
                            ? std::nullopt
                            : cityTileAtScreen(mouseX, mouseY)
                    );
                }

                if (settlementCommandController_->isActive())
                {
                    float mouseX = 0.0F;
                    float mouseY = 0.0F;
                    SDL_GetMouseState(&mouseX, &mouseY);
                    settlementCommandController_->pointerMoved(
                        cityHud_->containsInteractivePoint(mouseX, mouseY)
                            ? std::nullopt
                            : cityTileAtScreen(mouseX, mouseY)
                    );
                }

                while (simulationClock_->shouldTick())
                {
                    simulation_->tick(
                        simulationClock_->fixedDeltaSeconds()
                    );

                    simulationClock_->consumeTick();
                }

                renderer_->beginFrame();

                SettlementMap* settlementMap =
                    simulation_->settlementMap(activeCitySettlementId_);
                if (settlementMap)
                {
                    auto* owner = simulation_->world().settlement(activeCitySettlementId_);
                    if (owner)
                    {
                        auto& citizens = owner->simulationState().citizens();
                        settlementMap->employment().synchronize(settlementMap->objectState(), citizens);
                        settlementMap->employment().record(simulation_->world().time().totalGameMinutes(), citizens);
                        cityHud_->setSettlementStatus(settlementMap->objectState().hasCityKeep(), citizens.citizens().size());
                    }
                }

                if (settlementMap)
                {
                    const Settlement* renderedSettlement =
                        simulation_->world().settlement(
                            activeCitySettlementId_
                        );
                    if (renderedSettlement)
                    {
                        cityRenderer_->render(
                            *renderer_,
                            *settlementMap,
                            *camera_,
                            *tileRenderMetrics_,
                            *settlementObjectPlacementController_,
                            *settlementCommandController_,
                            renderedSettlement->simulationState().citizens()
                        );

                        settlementInspectionPanel_->render(
                            *renderer_,
                            *grayUiRenderer_,
                            *settlementInspectionController_,
                            *settlementMap,
                            renderedSettlement->simulationState().citizens(),
                            *camera_,
                            *tileRenderMetrics_
                        );
                    }
                    else
                    {
                        settlementInspectionPanel_->clearLayout();
                    }
                }

                const Settlement* citySettlement =
                    simulation_->world().settlement(
                        activeCitySettlementId_
                    );

                const WorldTime& worldTime =
                    simulation_->world().time();

                cityHud_->setCityInformation(
                    citySettlement
                        ? std::string(citySettlement->name())
                        : std::string(),
                    worldTime.day(),
                    worldTime.hour(),
                    worldTime.minute()
                );

                cityHud_->setGoodsAmounts(
                    citySettlement ? citySettlement->simulationState().stockpile().amount("stone") : 0,
                    citySettlement ? citySettlement->simulationState().stockpile().amount("lumber") : 0
                );
                cityHud_->render(
                    *renderer_,
                    *grayUiRenderer_
                );
                if (settlementMap)
                {
                    cityRenderer_->renderMinimap(
                        *renderer_, *settlementMap, *camera_,
                        *tileRenderMetrics_, cityHud_->minimapBounds()
                    );
                }

                simulationSpeedControls_->render(
                    *renderer_,
                    *grayUiRenderer_
                );
                if (settlementMap && citySettlement)
                    employmentPanel_->render(*renderer_, *grayUiRenderer_, *settlementMap,
                        citySettlement->simulationState().citizens(), worldTime.totalGameMinutes());

                if (screen_ != Screen::MainMenu && simulation_)
                {
                    renderDebug();
                }
                renderer_->endFrame();
                continue;
            }

            if (!foundingPanel_->isOpen())
            {
                updateCameraMovement(
                    simulationClock_->frameDeltaSeconds()
                );

                updateCameraZoom(
                    simulationClock_->frameDeltaSeconds()
                );
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

            while (simulationClock_->shouldTick())
            {
                simulation_->tick(
                    simulationClock_->fixedDeltaSeconds()
                );

                simulationClock_->consumeTick();
            }

            renderer_->beginFrame();

            std::array<TileOverlayRenderItem, 1> overlays{};
            std::array<TileOutlineRenderItem, 1> outlines{};
            std::size_t overlayCount = 0;
            std::size_t outlineCount = 0;

            const std::optional<WorldTilePosition> hoveredPosition =
                settlementPlacementController_->hoveredPosition();

            const std::optional<WorldTilePosition> lockedPosition =
                settlementPlacementController_->lockedPosition();

            if (
                settlementPlacementController_->isSelecting() &&
                hoveredPosition
            )
            {
                const bool validPlacement =
                    settlementPlacementController_
                        ->hasValidPlacement(
                            simulation_->world()
                        );

                const TerritoryFoundationPolicy& territoryPolicy =
                    simulation_->world()
                        .territoryFoundationPolicy();

                const double regionX =
                    static_cast<double>(hoveredPosition->x)
                    - static_cast<double>(
                        territoryPolicy.settlementRegionWidth / 2
                    );

                const double regionY =
                    static_cast<double>(hoveredPosition->y)
                    - static_cast<double>(
                        territoryPolicy.settlementRegionHeight / 2
                    );

                const RenderColor previewColor = validPlacement
                    ? RenderColor{72, 220, 112, 220}
                    : RenderColor{232, 70, 70, 230};

                overlays[0] = {
                    regionX,
                    regionY,
                    static_cast<double>(
                        territoryPolicy.settlementRegionWidth
                    ),
                    static_cast<double>(
                        territoryPolicy.settlementRegionHeight
                    ),
                    validPlacement
                        ? RenderColor{72, 220, 112, 34}
                        : RenderColor{232, 70, 70, 42}
                };

                outlines[0] = {
                    regionX,
                    regionY,
                    static_cast<double>(
                        territoryPolicy.settlementRegionWidth
                    ),
                    static_cast<double>(
                        territoryPolicy.settlementRegionHeight
                    ),
                    2.0F,
                    previewColor
                };

                overlayCount = 1;
                outlineCount = 1;
            }
            else if (lockedPosition)
            {
                const MapColor selectedColor =
                    foundingPanel_->isOpen()
                        ? foundingPanel_->selectedColor()
                        : MapColor{238, 190, 64};

                const TerritoryFoundationPolicy& territoryPolicy =
                    simulation_->world()
                        .territoryFoundationPolicy();

                const double regionX =
                    static_cast<double>(lockedPosition->x)
                    - static_cast<double>(
                        territoryPolicy.settlementRegionWidth / 2
                    );

                const double regionY =
                    static_cast<double>(lockedPosition->y)
                    - static_cast<double>(
                        territoryPolicy.settlementRegionHeight / 2
                    );

                overlays[0] = {
                    regionX,
                    regionY,
                    static_cast<double>(
                        territoryPolicy.settlementRegionWidth
                    ),
                    static_cast<double>(
                        territoryPolicy.settlementRegionHeight
                    ),
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
                    static_cast<double>(
                        territoryPolicy.settlementRegionWidth
                    ),
                    static_cast<double>(
                        territoryPolicy.settlementRegionHeight
                    ),
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

            worldHud_->render(
                *renderer_,
                *grayUiRenderer_,
                settlementPlacementController_->isActive()
            );

            if (simulationControlsUnlocked_)
            {
                simulationSpeedControls_->render(
                    *renderer_,
                    *grayUiRenderer_
                );
            }

            foundingPanel_->render(
                *renderer_,
                *grayUiRenderer_
            );

            if (screen_ != Screen::MainMenu && simulation_)
            {
                renderDebug();
            }
            renderer_->endFrame();
        }

        return 0;
    }

    void Application::startWorldSession()
    {
        simulation_ =
            std::make_unique<Simulation>();

        camera_ =
            std::make_unique<Camera2D>(
                static_cast<double>(
                    simulation_->world().grid().width()
                ) * 0.5,
                static_cast<double>(
                    simulation_->world().grid().height()
                ) * 0.5
            );

        settlementPlacementController_ =
            std::make_unique<SettlementPlacementController>();

        settlementObjectPlacementController_ =
            std::make_unique<SettlementObjectPlacementController>();

        settlementCommandController_ =
            std::make_unique<SettlementCommandController>();

        worldRenderer_ =
            std::make_unique<WorldRenderer>();

        cityRenderer_ =
            std::make_unique<CityRenderer>();

        tileRenderMetrics_ =
            std::make_unique<TileRenderMetrics>();

        tileRenderMetrics_->tilePixels = 4.0;

        edgeScrollDwellSeconds_ = 0.0;
        movingCapital_ = false;
        savedWorldCamera_.reset();
        activeCitySettlementId_ = {};
        cityHudCapturedPointer_ = false;
        simulationControlsUnlocked_ = false;
        simulationControlsCapturedPointer_ = false;
        settlementInspectionController_->clear();
        settlementInspectionPanel_->clearLayout();
        simulationClock_->reset();
        screen_ = Screen::World;
    }

    void Application::endWorldSession()
    {
        debugConsole_->reset();
        cachedStats_.clear();
        nextStatsRefresh_ = 0;
        foundingPanel_->close();
        SDL_StopTextInput(window_->nativeHandle());

        tileRenderMetrics_.reset();
        cityCameras_.clear();
        cityRenderer_.reset();
        worldRenderer_.reset();
        settlementPlacementController_.reset();
        settlementObjectPlacementController_.reset();
        settlementCommandController_.reset();
        settlementInspectionController_->clear();
        settlementInspectionPanel_->clearLayout();
        camera_.reset();
        savedWorldCamera_.reset();
        simulation_.reset();

        edgeScrollDwellSeconds_ = 0.0;
        movingCapital_ = false;
        activeCitySettlementId_ = {};
        cityHudCapturedPointer_ = false;
        simulationControlsUnlocked_ = false;
        simulationControlsCapturedPointer_ = false;
        simulationClock_->reset();
        screen_ = Screen::MainMenu;

        mainMenu_->layout(
            renderer_->outputWidth(),
            renderer_->outputHeight()
        );
    }

    void Application::enterPlayerCapitalCity()
    {
        if (
            screen_ != Screen::World ||
            !simulation_ ||
            !camera_ ||
            !tileRenderMetrics_
        )
        {
            return;
        }

        const Polity* polity = simulation_->world().polity(
            simulation_->playerPolityId()
        );

        if (!polity || !polity->capitalSettlementId().isValid())
        {
            return;
        }

        const SettlementId capitalId =
            simulation_->presentedSettlementId()
                ? simulation_->presentedSettlementId()
                : polity->capitalSettlementId();

        if (
            !simulation_->prepareSettlementMap(capitalId) ||
            !simulation_->setPresentedSettlement(capitalId) ||
            !simulation_->setDetailedSimulationSettlement(capitalId)
        )
        {
            return;
        }

        const SettlementMap* settlementMap =
            simulation_->settlementMap(capitalId);

        if (!settlementMap)
        {
            static_cast<void>(
                simulation_->clearDetailedSimulationSettlement()
            );
            return;
        }

        savedWorldCamera_ =
            std::make_unique<Camera2D>(*camera_);

        camera_ = std::make_unique<Camera2D>(
            static_cast<double>(settlementMap->grid().width()) * 0.5,
            static_cast<double>(settlementMap->grid().height()) * 0.5
        );

        tileRenderMetrics_->tilePixels = 2.0;
        simulation_->world()
            .settlement(capitalId)
            ->simulationState()
            .citizens()
            .placeUnpositionedCitizens(*settlementMap);
        if (!cityRenderer_)
        {
            cityRenderer_ = std::make_unique<CityRenderer>();
        }
        for (const auto& saved : cityCameras_)
        {
            if (saved.first == capitalId)
            {
                *camera_ = *saved.second;
            }
        }
        activeCitySettlementId_ = capitalId;
        edgeScrollDwellSeconds_ = 0.0;
        settlementPlacementController_->cancelSelection();
        settlementObjectPlacementController_->cancelPlacement();
        settlementCommandController_->cancel();
        settlementInspectionController_->clear();
        settlementInspectionPanel_->clearLayout();
        cityHudCapturedPointer_ = false;
        simulationControlsUnlocked_ = true;
        simulationControlsCapturedPointer_ = false;
        movingCapital_ = false;
        employmentPanel_->close();
        employmentCapturedPointer_ = false;
        cityHud_->setSettlementStatus(settlementMap->objectState().hasCityKeep(),
            simulation_->world().settlement(capitalId)->simulationState().citizens().citizens().size());
        screen_ = Screen::City;

        simulationSpeedControls_->layout(renderer_->outputWidth());
        simulationSpeedControls_->setPlaybackState(
            simulationClock_->isPaused(),
            simulationClock_->speedMultiplier()
        );

        clampCameraToWorld();
    }

    void Application::returnToWorldFromCity()
    {
        employmentPanel_->close();
        SDL_StopTextInput(window_->nativeHandle());
        if (screen_ != Screen::City || !simulation_)
        {
            return;
        }

        settlementObjectPlacementController_->cancelPlacement();
        settlementCommandController_->cancel();
        settlementInspectionController_->clear();
        settlementInspectionPanel_->clearLayout();
        cityHudCapturedPointer_ = false;
        simulationControlsCapturedPointer_ = false;

        if (!simulation_->clearDetailedSimulationSettlement())
        {
            return;
        }

        auto saved = std::find_if(
            cityCameras_.begin(),
            cityCameras_.end(),
            [&](const auto& entry)
            { return entry.first == activeCitySettlementId_; }
        );
        if (saved == cityCameras_.end())
        {
            cityCameras_.push_back(
                {activeCitySettlementId_, std::make_unique<Camera2D>(*camera_)}
            );
        }
        else
        {
            *saved->second = *camera_;
        }
        if (savedWorldCamera_)
        {
            camera_ = std::move(savedWorldCamera_);
        }
        else
        {
            camera_ = std::make_unique<Camera2D>(
                static_cast<double>(
                    simulation_->world().grid().width()
                ) * 0.5,
                static_cast<double>(
                    simulation_->world().grid().height()
                ) * 0.5
            );
        }

        tileRenderMetrics_->tilePixels = 4.0;
        activeCitySettlementId_ = {};
        edgeScrollDwellSeconds_ = 0.0;
        screen_ = Screen::World;

        worldHud_->setSimulationControlsUnlocked(true);
        worldHud_->layout(
            renderer_->outputWidth(),
            renderer_->outputHeight()
        );

        clampCameraToWorld();
    }

    void Application::cancelFoundingFlow()
    {
        foundingPanel_->close();
        SDL_StopTextInput(window_->nativeHandle());
        settlementPlacementController_->cancelSelection();
        movingCapital_ = false;
    }

    void Application::confirmFoundingFlow()
    {
        if (
            !foundingPanel_->isOpen() ||
            !foundingPanel_->canConfirm()
        )
        {
            return;
        }

        const FoundingPanelMode mode = foundingPanel_->mode();
        const FoundingIdentity identity = foundingPanel_->identity();

        bool completed = false;
        SettlementId settlementId;

        if (mode == FoundingPanelMode::Founding)
        {
            const std::optional<WorldTilePosition> lockedPosition =
                settlementPlacementController_->lockedPosition();

            if (!lockedPosition)
            {
                cancelFoundingFlow();
                return;
            }

            settlementId = simulation_->foundPlayerCapital(
                *lockedPosition,
                identity
            );
            completed = settlementId.isValid();
        }
        else if (mode == FoundingPanelMode::RenameCapital)
        {
            completed = simulation_->renamePlayerCapital(
                identity.capitalName
            );
        }
        else
        {
            completed = simulation_->editPlayerPolity(identity);
        }

        if (!completed) return;

        foundingPanel_->close();
        SDL_StopTextInput(window_->nativeHandle());
        settlementPlacementController_->cancelSelection();

        if (settlementId.isValid())
        {
            SDL_Log(
                "Founded player capital %llu.",
                static_cast<unsigned long long>(settlementId.value())
            );
        }
    }

    void Application::updateCameraMovement(
        double frameDeltaSeconds
    )
    {
        if (debugConsole_->wantsKeyboard())
        {
            return;
        }
        const bool* keyboardState =
            SDL_GetKeyboardState(nullptr);

        double directionX = 0.0;
        double directionY = 0.0;

        if (keyboardState[SDL_SCANCODE_A])
        {
            directionX -= 1.0;
        }

        if (keyboardState[SDL_SCANCODE_D])
        {
            directionX += 1.0;
        }

        if (keyboardState[SDL_SCANCODE_W])
        {
            directionY -= 1.0;
        }

        if (keyboardState[SDL_SCANCODE_S])
        {
            directionY += 1.0;
        }

        const double baseTilePixels =
            tileRenderMetrics_
                ? std::max(tileRenderMetrics_->tilePixels, 0.001)
                : 1.0;

        double panSpeedTilesPerSecondAtZoomOne =
            cameraNavigationPolicy_
                .keyboardPanSpeedScreenPixelsPerSecond
            / baseTilePixels;

        const bool keyboardMoving =
            directionX != 0.0 || directionY != 0.0;

        if (keyboardMoving)
        {
            edgeScrollDwellSeconds_ = 0.0;
        }
        else
        {
            float mouseX = 0.0F;
            float mouseY = 0.0F;

            const SDL_MouseButtonFlags mouseButtons =
                SDL_GetMouseState(&mouseX, &mouseY);

            const SDL_WindowFlags windowFlags =
                SDL_GetWindowFlags(window_->nativeHandle());

            const float viewportWidth =
                static_cast<float>(renderer_->outputWidth());

            const float viewportHeight =
                static_cast<float>(renderer_->outputHeight());

            const bool edgeScrollEligible =
                (
                    windowFlags & SDL_WINDOW_INPUT_FOCUS
                ) != 0 &&
                (
                    windowFlags & SDL_WINDOW_MOUSE_FOCUS
                ) != 0 &&
                mouseButtons == 0 &&
                mouseX >= 0.0F &&
                mouseY >= 0.0F &&
                mouseX < viewportWidth &&
                mouseY < viewportHeight &&
                !activeHudContainsPoint(
                    mouseX,
                    mouseY
                );

            if (edgeScrollEligible)
            {
                const float activationWidth = std::min(
                    cameraNavigationPolicy_
                        .edgeActivationWidthPixels,
                    std::min(viewportWidth, viewportHeight) * 0.5F
                );

                const auto edgeAxis = [this, activationWidth](
                    float position,
                    float extent
                )
                {
                    double intensity = 0.0;

                    if (position < activationWidth)
                    {
                        intensity = -(
                            1.0
                            - static_cast<double>(position)
                                / activationWidth
                        );
                    }
                    else if (position > extent - activationWidth)
                    {
                        intensity =
                            1.0
                            - static_cast<double>(extent - position)
                                / activationWidth;
                    }

                    return std::copysign(
                        std::pow(
                            std::abs(intensity),
                            cameraNavigationPolicy_
                                .edgeResponseExponent
                        ),
                        intensity
                    );
                };

                directionX = edgeAxis(mouseX, viewportWidth);
                directionY = edgeAxis(mouseY, viewportHeight);
            }

            if (directionX != 0.0 || directionY != 0.0)
            {
                edgeScrollDwellSeconds_ += frameDeltaSeconds;

                if (
                    edgeScrollDwellSeconds_ <
                    cameraNavigationPolicy_
                        .edgeActivationDelaySeconds
                )
                {
                    return;
                }

                panSpeedTilesPerSecondAtZoomOne =
                    cameraNavigationPolicy_
                        .edgePanSpeedScreenPixelsPerSecond
                    / baseTilePixels;
            }
            else
            {
                edgeScrollDwellSeconds_ = 0.0;
                return;
            }
        }

        double directionLength =
            std::hypot(directionX, directionY);

        if (directionLength == 0.0)
        {
            return;
        }

        if (directionLength > 1.0)
        {
            directionX /= directionLength;
            directionY /= directionLength;
            directionLength = 1.0;
        }

        const double panSpeedTilesPerSecond =
            panSpeedTilesPerSecondAtZoomOne
            / camera_->zoom();

        camera_->move(
            directionX
                * panSpeedTilesPerSecond
                * frameDeltaSeconds,
            directionY
                * panSpeedTilesPerSecond
                * frameDeltaSeconds
        );

        clampCameraToWorld();
    }

    void Application::updateCameraZoom(
        double frameDeltaSeconds
    )
    {
        if (debugConsole_->wantsKeyboard())
        {
            return;
        }
        const bool* keyboardState =
            SDL_GetKeyboardState(nullptr);

        double zoomDirection = 0.0;

        if (
            keyboardState[SDL_SCANCODE_EQUALS] ||
            keyboardState[SDL_SCANCODE_KP_PLUS]
        )
        {
            zoomDirection += 1.0;
        }

        if (
            keyboardState[SDL_SCANCODE_MINUS] ||
            keyboardState[SDL_SCANCODE_KP_MINUS]
        )
        {
            zoomDirection -= 1.0;
        }

        if (zoomDirection == 0.0)
        {
            return;
        }

        float mouseX = 0.0F;
        float mouseY = 0.0F;

        SDL_GetMouseState(&mouseX, &mouseY);

        constexpr double keyboardZoomFactorPerSecond = 2.0;

        applyCameraZoom(
            std::pow(
                keyboardZoomFactorPerSecond,
                zoomDirection * frameDeltaSeconds
            ),
            static_cast<double>(mouseX),
            static_cast<double>(mouseY)
        );
    }

    void Application::updateSettlementPlacementHover(
        double screenX,
        double screenY
    )
    {
        const double tilePixels =
            tileRenderMetrics_->scaledTilePixels(
                camera_->zoom()
            );

        const double viewportWidth =
            static_cast<double>(renderer_->outputWidth());

        const double viewportHeight =
            static_cast<double>(renderer_->outputHeight());

        if (
            tilePixels <= 0.0 ||
            screenX < 0.0 ||
            screenY < 0.0 ||
            screenX >= viewportWidth ||
            screenY >= viewportHeight
        )
        {
            settlementPlacementController_->setHoveredPosition(
                std::nullopt
            );

            return;
        }

        const double worldTileX =
            camera_->tileX()
            + (screenX - viewportWidth * 0.5)
                / tilePixels;

        const double worldTileY =
            camera_->tileY()
            + (screenY - viewportHeight * 0.5)
                / tilePixels;

        const WorldTilePosition position{
            static_cast<std::int32_t>(std::floor(worldTileX)),
            static_cast<std::int32_t>(std::floor(worldTileY))
        };

        if (
            !simulation_->world().grid().isValidPosition({
                position.x,
                position.y
            })
        )
        {
            settlementPlacementController_->setHoveredPosition(
                std::nullopt
            );

            return;
        }

        settlementPlacementController_->setHoveredPosition(position);
    }

    void Application::applyCameraZoom(
        double multiplier,
        double screenX,
        double screenY
    )
    {
        if (multiplier <= 0.0 || multiplier == 1.0)
        {
            return;
        }

        const double viewportWidth =
            static_cast<double>(renderer_->outputWidth());

        const double viewportHeight =
            static_cast<double>(renderer_->outputHeight());

        if (viewportWidth <= 0.0 || viewportHeight <= 0.0)
        {
            return;
        }

        const double screenOffsetX =
            screenX - viewportWidth * 0.5;

        const double screenOffsetY =
            screenY - viewportHeight * 0.5;

        const double oldTilePixels =
            tileRenderMetrics_->scaledTilePixels(
                camera_->zoom()
            );

        if (oldTilePixels <= 0.0)
        {
            return;
        }

        const double worldTileXUnderCursor =
            camera_->tileX()
            + screenOffsetX / oldTilePixels;

        const double worldTileYUnderCursor =
            camera_->tileY()
            + screenOffsetY / oldTilePixels;

        camera_->multiplyZoom(multiplier);

        const double newTilePixels =
            tileRenderMetrics_->scaledTilePixels(
                camera_->zoom()
            );

        camera_->setPosition(
            worldTileXUnderCursor
                - screenOffsetX / newTilePixels,
            worldTileYUnderCursor
                - screenOffsetY / newTilePixels
        );

        clampCameraToWorld();
    }


    void Application::clampCameraToWorld() noexcept
    {
        if (
            !camera_ ||
            !simulation_ ||
            !renderer_ ||
            !tileRenderMetrics_
        )
        {
            return;
        }

        const double tilePixels =
            tileRenderMetrics_->scaledTilePixels(camera_->zoom());

        if (tilePixels <= 0.0)
        {
            return;
        }

        std::int32_t gridWidth = simulation_->world().grid().width();
        std::int32_t gridHeight = simulation_->world().grid().height();

        if (screen_ == Screen::City)
        {
            const SettlementMap* settlementMap =
                simulation_->settlementMap(activeCitySettlementId_);

            if (!settlementMap)
            {
                return;
            }

            gridWidth = settlementMap->grid().width();
            gridHeight = settlementMap->grid().height();
        }

        const double worldWidth = static_cast<double>(gridWidth);
        const double worldHeight = static_cast<double>(gridHeight);

        const double halfVisibleWidth =
            static_cast<double>(renderer_->outputWidth())
            / (2.0 * tilePixels);

        const double halfVisibleHeight =
            static_cast<double>(renderer_->outputHeight())
            / (2.0 * tilePixels);

        const auto clampAxis = [](
            double position,
            double worldSize,
            double halfVisibleSize
        ) noexcept
        {
            if (halfVisibleSize * 2.0 >= worldSize)
            {
                return worldSize * 0.5;
            }

            return std::clamp(
                position,
                halfVisibleSize,
                worldSize - halfVisibleSize
            );
        };

        camera_->setPosition(
            clampAxis(
                camera_->tileX(),
                worldWidth,
                halfVisibleWidth
            ),
            clampAxis(
                camera_->tileY(),
                worldHeight,
                halfVisibleHeight
            )
        );
    }

    bool Application::activeHudContainsPoint(
        float x,
        float y
    ) const noexcept
    {
        if (debugConsole_->contains(x, y))
        {
            return true;
        }
        if (
            (
                screen_ == Screen::City ||
                (
                    screen_ == Screen::World &&
                    simulationControlsUnlocked_
                )
            ) &&
            simulationSpeedControls_->containsInteractivePoint(x, y)
        )
        {
            return true;
        }

        if (screen_ == Screen::City)
        {
            return
                cityHud_->containsInteractivePoint(x, y) ||
                employmentPanel_->containsPoint(x, y) ||
                settlementInspectionPanel_->containsPoint(x, y);
        }

        return worldHud_->containsInteractivePoint(x, y);
    }

    std::optional<SettlementTilePosition> Application::cityTileAtScreen(
        double screenX,
        double screenY
    ) const noexcept
    {
        if (
            screen_ != Screen::City ||
            !camera_ ||
            !renderer_ ||
            !tileRenderMetrics_
        )
        {
            return std::nullopt;
        }

        const SettlementMap* settlementMap = simulation_->settlementMap(
            activeCitySettlementId_
        );

        if (!settlementMap)
        {
            return std::nullopt;
        }

        const double tilePixels =
            tileRenderMetrics_->scaledTilePixels(camera_->zoom());

        if (tilePixels <= 0.0)
        {
            return std::nullopt;
        }

        const double tileX =
            camera_->tileX()
            + (
                screenX
                - static_cast<double>(renderer_->outputWidth()) * 0.5
            ) / tilePixels;

        const double tileY =
            camera_->tileY()
            + (
                screenY
                - static_cast<double>(renderer_->outputHeight()) * 0.5
            ) / tilePixels;

        const SettlementTilePosition position{
            static_cast<std::int32_t>(std::floor(tileX)),
            static_cast<std::int32_t>(std::floor(tileY))
        };

        return settlementMap->grid().isValidPosition(position)
            ? std::optional<SettlementTilePosition>(position)
            : std::nullopt;
    }
}

namespace Paladin
{
void Application::executeConsoleCommand(std::string_view text)
{
    const auto command = parseConsoleCommand(text);
    if (command.kind == ConsoleCommandKind::Empty)
    {
        return;
    }
    if (command.kind == ConsoleCommandKind::Stats)
    {
        debugConsole_->showStats();
        nextStatsRefresh_ = 0;
        return;
    }
    if (command.kind == ConsoleCommandKind::Invalid)
    {
        debugConsole_->print(command.error);
        return;
    }
    // Presented settlement is the active player context, independent of
    // view/detailed tier.
    auto id = simulation_->presentedSettlementId();
    if (!id)
    {
        if (const auto* polity =
                simulation_->world().polity(simulation_->playerPolityId()))
        {
            id = polity->capitalSettlementId();
        }
    }
    auto* settlement = simulation_->world().settlement(id);
    if (!settlement ||
        settlement->ownerPolityId() != simulation_->playerPolityId())
    {
        debugConsole_->print(
            "No active player settlement. Found a settlement first."
        );
        return;
    }
    try
    {
        if (!settlement->simulationState().spawnCitizens(command.count))
        {
            debugConsole_->print("Unable to spawn citizens.");
            return;
        }
        if (auto* map = simulation_->settlementMap(id))
        {
            map->employment().record(
                simulation_->world().time().totalGameMinutes(),
                settlement->simulationState().citizens()
            );
        }
        debugConsole_->print(
            "Successfully spawned " + std::to_string(command.count) +
            (command.count == 1 ? " citizen" : " citizens") +
            " in settlement #" + std::to_string(id.value()) + "."
        );
        nextStatsRefresh_ = 0;
    }
    catch (const std::bad_alloc&)
    {
        debugConsole_->print("Unable to spawn citizens: insufficient memory.");
        return;
    }
    if (auto* map = simulation_->settlementMap(id))
    {
        try
        {
            settlement->simulationState().citizens().placeUnpositionedCitizens(
                *map
            );
        }
        catch (const std::bad_alloc&)
        {
            debugConsole_->print(
                "Citizens created; map placement deferred due "
                "to memory pressure."
            );
        }
    }
}
void Application::renderDebug()
{
    if (debugConsole_->statsVisible() && SDL_GetTicks() >= nextStatsRefresh_)
    {
        nextStatsRefresh_ = SDL_GetTicks() + 200;
        const auto& world = simulation_->world();
        std::ostringstream s;
        s << std::fixed << std::setprecision(3);
        s << "World time: " << world.time().totalGameMinutes()
          << " minutes\nClock: "
          << (simulationClock_->isPaused() ? "Paused" : "Running")
          << " | Speed: " << simulationClock_->speedMultiplier()
          << "x\nTick: " << simulation_->tickCount()
          << " | Backlog: " << simulationClock_->backlogTicks()
          << "\nBacklog limit hits: " << simulationClock_->limitHits
          << " | Discarded: " << simulationClock_->discardedSeconds << " s"
          << "\nFrame: " << simulationClock_->frameDeltaSeconds() * 1000
          << " ms | FPS: "
          << (simulationClock_->frameDeltaSeconds() > 0
                  ? 1 / simulationClock_->frameDeltaSeconds()
                  : 0)
          << "\nTiming ms: last / avg / p95 / max (120 samples)\nTotal: "
          << simulation_->tickTiming.text()
          << "\nCitizens: " << simulation_->citizenTiming.text()
          << "\nAggregate: " << simulation_->aggregateTiming.text() << "\n"
          << simulation_->systemTimingText() << "Minutes/tick: "
          << simulation_->gameMinutesPerTick(
                 simulationClock_->fixedDeltaSeconds()
             )
          << "\nSlow ticks >= 50 ms: " << simulation_->tickTiming.slow
          << "\nScene: " << (screen_ == Screen::City ? "City" : "World")
          << " | Seed: " << world.generationSeed()
          << "\nSettlements: " << world.settlementCount() << " | Detailed: #"
          << simulation_->detailedSimulationSettlementId().value();
        auto id = simulation_->presentedSettlementId();
        if (!id)
        {
            if (const auto* p = world.polity(simulation_->playerPolityId()))
            {
                id = p->capitalSettlementId();
            }
        }
        if (const auto* settlement = world.settlement(id))
        {
            const auto& state = settlement->simulationState();
            const auto& citizens = state.citizens();
            const auto* map = simulation_->settlementMap(id);
            std::size_t moving = 0, working = 0, paths = 0, invalidJobs = 0;
            for (const auto& c : citizens.citizens())
            {
                moving += !c.path.empty();
                working += c.activity == CitizenActivity::AtWork;
                paths += c.path.size();
                invalidJobs +=
                    c.workplaceId &&
                    (!map || !map->employment().workplace(c.workplaceId));
            }
            s << "\nActive settlement: #" << id.value() << " | Owner: #"
              << settlement->ownerPolityId().value()
              << "\nTier: " << int(state.simulationTier())
              << " | Pending minutes: " << state.pendingSimulationMinutes()
              << "\nPopulation: " << state.population().residents()
              << " | Citizens: " << citizens.citizens().size()
              << "\nMoving: " << moving << " | At work: " << working
              << " | Path steps: " << paths
              << "\nValidation - orphaned jobs: " << invalidJobs;
            if (map)
            {
                s << "\nLocal seed: " << map->generationSeed()
                  << " | Size: " << map->grid().width() << " x "
                  << map->grid().height() << "\nObjects: "
                  << map->objectState().completedObjects().size()
                  << " | Construction: "
                  << map->objectState().constructionSites().size()
                  << "\nCommands: " << map->commandState().commands().size()
                  << " | Workplaces: " << map->employment().workplaces().size()
                  << "\nUnemployed: " << map->employment().unemployed(citizens);
            }
            const auto& nav = citizens.navigationDiagnostics();
            s << "\nNavigation requests: " << nav.requests
              << " | Failed: " << nav.failures
              << "\nExpanded: " << nav.expandedNodes
              << " | Candidates: " << nav.candidates
              << " | Cost: " << nav.lastCost
              << "\nNavigation ms: " << nav.timing.text();
            s << "\nResources (secured):";
            for (const auto& entry : state.stockpile().entries())
            {
                s << "\n" << entry.resourceId << ": " << entry.amount;
            }
            s << "\nLoose/physical resource conservation: not implemented";
        }
        float mx = 0, my = 0;
        SDL_GetMouseState(&mx, &my);
        const auto pixels =
            tileRenderMetrics_->scaledTilePixels(camera_->zoom());
        const int x = int(std::floor(
            camera_->tileX() + (mx - renderer_->outputWidth() * .5) / pixels
        ));
        const int y = int(std::floor(
            camera_->tileY() + (my - renderer_->outputHeight() * .5) / pixels
        ));
        const WorldTile* tile = nullptr;
        if (screen_ == Screen::City)
        {
            if (const auto* map =
                    simulation_->settlementMap(activeCitySettlementId_))
            {
                tile = map->grid().tile({x, y});
            }
        }
        else
        {
            tile = world.grid().tile({x, y});
        }
        s << "\nCursor tile: " << x << ", " << y
          << " | Zoom: " << camera_->zoom();
        if (tile)
        {
            constexpr const char* terrains[] = {"Land", "Water", "Mountain"};
            constexpr const char* biomes[] = {
                "Plain",
                "Forest",
                "Jungle",
                "Desert",
                "Tundra",
                "Taiga",
                "Ocean"
            };
            s << "\nTerrain: " << terrains[int(tile->terrain)]
              << " | Biome: " << biomes[int(tile->biome)]
              << "\nElevation: " << tile->elevation.value()
              << " | Temperature: " << tile->temperature.value()
              << "\nPrecipitation: " << tile->rainfall.value();
        }
        if (screen_ == Screen::City)
        {
            if (const auto* map =
                    simulation_->settlementMap(activeCitySettlementId_))
            {
                const auto* object =
                    map->objectState().completedObjectAt({x, y});
                const auto* site =
                    map->objectState().constructionSiteAt({x, y});
                s << "\nObject: " << (object ? object->objectTypeId : "none")
                  << " | Site: " << (site ? site->objectTypeId : "none");
                if (site)
                {
                    s << "\nConstruction progress: "
                      << site->progressPermille / 10.0 << "%";
                }
                const auto* owner = world.settlement(activeCitySettlementId_);
                if (owner)
                {
                    const auto& citizens = owner->simulationState().citizens();
                    s << "\nWalkable: "
                      << (citizens.navigationDiagnostics()
                                  .walkable(*map, {x, y})
                              ? "Yes"
                              : "No");
                    if (const auto* citizen = citizens.citizenAt({x, y}))
                    {
                        s << "\nCitizen: #" << citizen->id.value() << " "
                          << citizen->name << " | Age: " << citizen->ageYears
                          << " | Job: #" << citizen->workplaceId.value();
                    }
                }
                if (const auto* road = SettlementObjectCatalog::definition(
                        SettlementObjectTypes::Road
                    ))
                {
                    s << "\nRoad placeable: "
                      << (map->objectState()
                                  .canPlace(map->grid(), *road, {{x, y}, 1, 1})
                              ? "Yes"
                              : "No");
                }
            }
        }
        s << "\nFertility, animals, ground piles, production: not implemented";
        cachedStats_ = s.str();
    }
    debugConsole_->layout(renderer_->outputWidth(), renderer_->outputHeight());
    debugConsole_->render(*renderer_, cachedStats_);
}
} // namespace Paladin

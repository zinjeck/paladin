#include "core/Application.h"

#include "core/SimulationClock.h"
#include "interaction/SettlementPlacementController.h"
#include "platform/Window.h"
#include "rendering/Camera2D.h"
#include "rendering/OverlayRenderer.h"
#include "rendering/Renderer.h"
#include "rendering/TileRenderMetrics.h"
#include "rendering/WorldRenderer.h"
#include "simulation/Simulation.h"
#include "ui/GrayUiRenderer.h"
#include "ui/FoundingPanel.h"
#include "ui/MainMenu.h"
#include "ui/WorldHud.h"
#include "world/World.h"

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

        foundingPanel_ =
            std::make_unique<FoundingPanel>();
    }

    Application::~Application()
    {
        if (window_)
        {
            SDL_StopTextInput(window_->nativeHandle());
        }

        tileRenderMetrics_.reset();
        worldRenderer_.reset();
        settlementPlacementController_.reset();
        camera_.reset();
        simulation_.reset();

        worldHud_.reset();
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
            !foundingPanel_
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
            else
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

                foundingPanel_->layout(
                    renderer_->outputWidth(),
                    renderer_->outputHeight()
                );
            }

            SDL_Event event;

            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_EVENT_QUIT)
                {
                    running = false;
                    continue;
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

            const std::optional<WorldPosition> hoveredPosition =
                settlementPlacementController_->hoveredPosition();

            const std::optional<WorldPosition> lockedPosition =
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

            foundingPanel_->render(
                *renderer_,
                *grayUiRenderer_
            );

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

        worldRenderer_ =
            std::make_unique<WorldRenderer>();

        tileRenderMetrics_ =
            std::make_unique<TileRenderMetrics>();

        edgeScrollDwellSeconds_ = 0.0;
        movingCapital_ = false;
        simulationClock_->reset();
        screen_ = Screen::World;
    }

    void Application::endWorldSession()
    {
        foundingPanel_->close();
        SDL_StopTextInput(window_->nativeHandle());

        tileRenderMetrics_.reset();
        worldRenderer_.reset();
        settlementPlacementController_.reset();
        camera_.reset();
        simulation_.reset();

        edgeScrollDwellSeconds_ = 0.0;
        movingCapital_ = false;
        simulationClock_->reset();
        screen_ = Screen::MainMenu;

        mainMenu_->layout(
            renderer_->outputWidth(),
            renderer_->outputHeight()
        );
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
            const std::optional<WorldPosition> lockedPosition =
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

        double panSpeedTilesPerSecondAtZoomOne =
            cameraNavigationPolicy_
                .keyboardPanSpeedTilesPerSecondAtZoomOne;

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
                !worldHud_->containsInteractivePoint(
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
                        .edgePanSpeedTilesPerSecondAtZoomOne;
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

        const WorldPosition position{
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

        const double worldWidth = static_cast<double>(
            simulation_->world().grid().width()
        );

        const double worldHeight = static_cast<double>(
            simulation_->world().grid().height()
        );

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
}

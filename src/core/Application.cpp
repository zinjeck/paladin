#include "core/Application.h"

#include "core/SimulationClock.h"
#include "platform/Window.h"
#include "rendering/Renderer.h"
#include "simulation/Simulation.h"
#include "rendering/Camera2D.h"
#include "rendering/TileRenderMetrics.h"
#include "rendering/WorldRenderer.h"

#include "world/World.h"
#include <SDL3/SDL.h>

#include <cmath>
#include <memory>

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

        simulation_ =
            std::make_unique<Simulation>();

        camera_ =
            std::make_unique<Camera2D>(
                128.0,
                128.0
            );
        
        worldRenderer_ =
            std::make_unique<WorldRenderer>();
        
        tileRenderMetrics_ =
            std::make_unique<TileRenderMetrics>();
    }

    Application::~Application()
    {
        tileRenderMetrics_.reset();
        worldRenderer_.reset();
        camera_.reset();

        simulation_.reset();
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
            !simulation_ ||
            !camera_ ||
            !worldRenderer_ ||
            !tileRenderMetrics_
        )
        {
            return 1;
        }

        bool running = true;

        while (running)
        {
            simulationClock_->beginFrame();

            SDL_Event event;

            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_EVENT_QUIT)
                {
                    running = false;
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
            }

            updateCameraMovement(
                simulationClock_->frameDeltaSeconds()
            );

            updateCameraZoom(
                simulationClock_->frameDeltaSeconds()
            );

            while (simulationClock_->shouldTick())
            {
                simulation_->tick(
                    simulationClock_->fixedDeltaSeconds()
                );

                simulationClock_->consumeTick();
            }

            renderer_->beginFrame();

            worldRenderer_->render(
                *renderer_,
                simulation_->world(),
                *camera_,
                *tileRenderMetrics_
            );

            renderer_->endFrame();
        }

        return 0;
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

        const double directionLength =
            std::hypot(directionX, directionY);

        if (directionLength == 0.0)
        {
            return;
        }

        directionX /= directionLength;
        directionY /= directionLength;

        constexpr double panSpeedTilesPerSecondAtZoomOne =
            162.5;

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
    }
}

#include "core/Application.h"

#include "core/SimulationClock.h"
#include "platform/Window.h"
#include "rendering/Renderer.h"
#include "simulation/Simulation.h"
#include "rendering/Camera2D.h"
#include "rendering/TileRenderMetrics.h"
#include "rendering/WorldGridRenderer.h"

#include "world/World.h"
#include <SDL3/SDL.h>

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
        
        worldGridRenderer_ =
            std::make_unique<WorldGridRenderer>();
        
        tileRenderMetrics_ =
            std::make_unique<TileRenderMetrics>();
    }

    Application::~Application()
    {
        tileRenderMetrics_.reset();
        worldGridRenderer_.reset();
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
            !simulation_
            !camera_ ||
            !worldGridRenderer_ ||
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
            }

            while (simulationClock_->shouldTick())
            {
                simulation_->tick(
                    simulationClock_->fixedDeltaSeconds()
                );

                simulationClock_->consumeTick();
            }

            renderer_->beginFrame();

            worldGridRenderer_->render(
                *renderer_,
                simulation_->world().grid(),
                *camera_,
                *tileRenderMetrics_
            )

            renderer_->endFrame();
        }

        return 0;
    }
}
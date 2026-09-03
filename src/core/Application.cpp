#include "core/Application.h"

#include "platform/Window.h"

#include <SDL3/SDL.h>

namespace Paladin
{
    Application::Application()
    {
        if (!SDL_Init(SDL_INIT_VIDEO))
        {
            SDL_Log("SDL_Init failed: %s", SDL_GetError());
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
        }
    }

    Application::~Application()
    {
        window_.reset();

        if (sdlInitialized_)
        {
            SDL_Quit();
        }
    }

    int Application::run()
    {
        if (!sdlInitialized_ || !window_)
        {
            return 1;
        }

        bool running = true;

        while (running)
        {
            SDL_Event event;

            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_EVENT_QUIT)
                {
                    running = false;
                }
            }
        }

        return 0;
    }
}
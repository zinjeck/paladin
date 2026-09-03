#include "platform/Window.h"

#include <SDL3/SDL.h>

namespace Paladin
{
    Window::Window(
        const char* title,
        int width,
        int height
    )
    {
        window_ = SDL_CreateWindow(
            title,
            width,
            height,
            SDL_WINDOW_RESIZABLE
        );

        if (!window_)
        {
            SDL_Log(
                "SDL_CreateWindow failed: %s",
                SDL_GetError()
            );
        }
    }

    Window::~Window()
    {
        if (window_)
        {
            SDL_DestroyWindow(window_);
        }
    }

    bool Window::isValid() const noexcept
    {
        return window_ != nullptr;
    }

    SDL_Window* Window::nativeHandle() const noexcept
    {
        return window_;
    }
}
#pragma once

struct SDL_Window;

namespace Paladin
{
    class Window
    {
    public:
        Window(
            const char* title,
            int width,
            int height
        );

        ~Window();

        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;

        bool isValid() const noexcept;

        SDL_Window* nativeHandle() const noexcept;

    private:
        SDL_Window* window_ = nullptr;
    };
}
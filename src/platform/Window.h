#pragma once

struct SDL_Window;

namespace Paladin
{
    enum class WindowMode
    {
        Windowed,
        WindowedFullscreen,
        Fullscreen
    };

    constexpr const char* windowModeName(WindowMode mode) noexcept
    {
        switch (mode)
        {
        case WindowMode::Windowed:
            return "Windowed";
        case WindowMode::Fullscreen:
            return "Fullscreen";
        default:
            return "Windowed Fullscreen";
        }
    }

    class Window
    {
    public:
        Window(const char* title, int width, int height);

        ~Window();

        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;

        bool isValid() const noexcept;

        SDL_Window* nativeHandle() const noexcept;
        [[nodiscard]] bool setMode(WindowMode mode, bool persist = true);
        [[nodiscard]] WindowMode mode() const noexcept
        {
            return mode_;
        }


    private:
        SDL_Window* window_ = nullptr;
        WindowMode mode_ = WindowMode::Windowed;
        int windowedX_ = 0;
        int windowedY_ = 0;
        int windowedWidth_ = 1280;
        int windowedHeight_ = 720;
        bool headless_ = false;
    };
} // namespace Paladin
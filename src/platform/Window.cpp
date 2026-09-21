#include "platform/Window.h"

#include <SDL3/SDL.h>
#include <fstream>
#include <string>

namespace Paladin
{
    namespace
    {
        std::string displaySettingsPath()
        {
            char* directory = SDL_GetPrefPath("Paladin", "Paladin");
            if (!directory)
            {
                return {};
            }
            std::string path = std::string(directory) + "display.cfg";
            SDL_free(directory);
            return path;
        }

        WindowMode loadWindowMode()
        {
            std::ifstream file(displaySettingsPath());
            int value = int(WindowMode::WindowedFullscreen);
            if (file >> value && value >= 0 && value <= 2)
            {
                return WindowMode(value);
            }
            return WindowMode::WindowedFullscreen;
        }
    } // namespace

    Window::Window(const char* title, int width, int height)
    {
        windowedWidth_ = width;
        windowedHeight_ = height;
        window_ = SDL_CreateWindow(title, width, height, SDL_WINDOW_RESIZABLE);

        if (!window_)
        {
            SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
            return;
        }
        SDL_GetWindowPosition(window_, &windowedX_, &windowedY_);
        const char* driver = SDL_GetCurrentVideoDriver();
        headless_ = driver && (std::string_view(driver) == "dummy" ||
                               std::string_view(driver) == "offscreen");
        // Headless render fixtures must retain their requested dimensions and
        // must never read or overwrite the player's display preference.
        if (!headless_)
        {
            static_cast<void>(setMode(loadWindowMode(), false));
        }
    }

    bool Window::setMode(WindowMode requested, bool persist)
    {
        if (!window_)
        {
            return false;
        }
        if (requested == mode_)
        {
            return true;
        }
        const auto previous = mode_;
        SDL_DisplayMode previousExclusive{};
        const auto* exclusive = SDL_GetWindowFullscreenMode(window_);
        const bool hadExclusive = exclusive != nullptr;
        if (exclusive)
        {
            previousExclusive = *exclusive;
        }
        if (previous == WindowMode::Windowed)
        {
            SDL_GetWindowPosition(window_, &windowedX_, &windowedY_);
            SDL_GetWindowSize(window_, &windowedWidth_, &windowedHeight_);
        }
        SDL_DisplayMode displayMode{};
        if (requested == WindowMode::Fullscreen)
        {
            const auto display = SDL_GetDisplayForWindow(window_);
            const auto* desktop = SDL_GetDesktopDisplayMode(display);
            if (!desktop || !SDL_GetClosestFullscreenDisplayMode(
                                display,
                                desktop->w,
                                desktop->h,
                                desktop->refresh_rate,
                                true,
                                &displayMode
                            ))
            {
                return false;
            }
        }
        const auto apply = [&](WindowMode target, const SDL_DisplayMode* mode)
        {
            if (!SDL_SetWindowFullscreen(window_, false) ||
                !SDL_SetWindowFullscreenMode(window_, mode))
            {
                return false;
            }
            if (target != WindowMode::Windowed &&
                !SDL_SetWindowFullscreen(window_, true))
            {
                return false;
            }
            if (target == WindowMode::Windowed)
            {
                SDL_SetWindowSize(window_, windowedWidth_, windowedHeight_);
                SDL_SetWindowPosition(window_, windowedX_, windowedY_);
            }
            return SDL_SyncWindow(window_);
        };
        if (!apply(
                requested,
                requested == WindowMode::Fullscreen ? &displayMode : nullptr
            ))
        {
            const std::string error = SDL_GetError();
            static_cast<void>(
                apply(previous, hadExclusive ? &previousExclusive : nullptr)
            );
            SDL_SetError("Unable to change window mode: %s", error.c_str());
            return false;
        }
        mode_ = requested;
        if (persist && !headless_)
        {
            std::ofstream file(displaySettingsPath(), std::ios::trunc);
            if (file)
            {
                file << int(mode_) << '\n';
            }
        }
        return true;
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
} // namespace Paladin
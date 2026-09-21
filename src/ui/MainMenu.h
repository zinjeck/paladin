#pragma once

#include "platform/Window.h"
#include "ui/UiButton.h"
#include <array>
#include <string>

namespace Paladin
{
    class GrayUiRenderer;
    class Renderer;

    enum class MainMenuAction
    {
        None,
        Play,
        Tutorial,
        Exit,
        ChangeWindowMode
    };

    class MainMenu
    {
    public:
        MainMenu();

        void layout(int viewportWidth, int viewportHeight) noexcept;
        void pointerMoved(float x, float y) noexcept;

        void pointerPressed(float x, float y) noexcept;

        [[nodiscard]]
        MainMenuAction pointerReleased(float x, float y) noexcept;

        void render(Renderer& renderer, const GrayUiRenderer& uiRenderer) const;
        void setWindowMode(WindowMode mode);
        [[nodiscard]] WindowMode requestedWindowMode() const noexcept
        {
            return requestedWindowMode_;
        }
        [[nodiscard]] bool closeTopLayer() noexcept;
        void setDisplayError(std::string message)
        {
            displayError_ = std::move(message);
        }


    private:
        UiButton playButton_;
        UiButton tutorialButton_;
        UiButton exitButton_;
        UiButton settingsButton_{"Settings"};
        UiButton displayTab_{"Display"};
        UiButton closeSettingsButton_{"Back"};
        UiButton windowModeButton_{"Windowed Fullscreen"};
        std::array<UiButton, 3> windowModeOptions_{
            UiButton{"Windowed"},
            UiButton{"Windowed Fullscreen"},
            UiButton{"Fullscreen"}
        };
        UiRectangle settingsBounds_;
        WindowMode requestedWindowMode_ = WindowMode::WindowedFullscreen;
        WindowMode activeWindowMode_ = WindowMode::WindowedFullscreen;
        bool settingsOpen_ = false;
        bool dropdownOpen_ = false;
        std::string displayError_;
    };
} // namespace Paladin

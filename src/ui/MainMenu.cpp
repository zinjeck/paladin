#include "ui/MainMenu.h"

#include "rendering/Renderer.h"
#include "ui/GrayUiRenderer.h"
#include <algorithm>

namespace Paladin
{
    MainMenu::MainMenu()
        : playButton_("Play"), tutorialButton_("Tutorial"), exitButton_("Exit")
    {
        displayTab_.setSelected(true);
    }

    void MainMenu::setWindowMode(WindowMode mode)
    {
        activeWindowMode_ = mode;
        requestedWindowMode_ = mode;
        windowModeButton_.setText(windowModeName(mode));
        for (std::size_t i = 0; i < windowModeOptions_.size(); ++i)
        {
            windowModeOptions_[i].setSelected(WindowMode(i) == mode);
        }
    }

    bool MainMenu::closeTopLayer() noexcept
    {
        if (dropdownOpen_)
        {
            dropdownOpen_ = false;
            return true;
        }
        if (settingsOpen_)
        {
            settingsOpen_ = false;
            return true;
        }
        return false;
    }

    void MainMenu::layout(int viewportWidth, int viewportHeight) noexcept
    {
        constexpr float buttonWidth = 220, buttonHeight = 44, pitch = 60;
        const float left = (float(viewportWidth) - buttonWidth) * .5F;
        // Preserve the established Play hit region when adding Settings.
        const float top = (float(viewportHeight) - 164) * .5F;
        playButton_.setBounds({left, top, buttonWidth, buttonHeight});
        tutorialButton_.setBounds(
            {left, top + pitch, buttonWidth, buttonHeight}
        );
        settingsButton_.setBounds(
            {left, top + pitch * 2, buttonWidth, buttonHeight}
        );
        exitButton_.setBounds(
            {left, top + pitch * 3, buttonWidth, buttonHeight}
        );
        const float width = std::min(650.F, float(viewportWidth) - 32);
        settingsBounds_ = {
            (float(viewportWidth) - width) * .5F,
            std::max(28.F, (float(viewportHeight) - 400) * .5F),
            width,
            400
        };
        const auto& b = settingsBounds_;
        displayTab_.setBounds({b.x + 20, b.y + 58, 144, 42});
        windowModeButton_.setBounds({b.x + 210, b.y + 135, width - 235, 44});
        closeSettingsButton_.setBounds(
            {b.x + 20, b.y + b.height - 64, 140, 44}
        );
        for (std::size_t i = 0; i < windowModeOptions_.size(); ++i)
        {
            windowModeOptions_[i].setBounds(
                {b.x + 210, b.y + 181 + float(i) * 44, width - 235, 44}
            );
        }
    }

    void MainMenu::pointerMoved(float x, float y) noexcept
    {
        if (settingsOpen_)
        {
            closeSettingsButton_.pointerMoved(x, y);
            windowModeButton_.pointerMoved(x, y);
            if (dropdownOpen_)
            {
                for (auto& button : windowModeOptions_)
                {
                    button.pointerMoved(x, y);
                }
            }
            return;
        }
        for (auto* button :
             {&playButton_, &tutorialButton_, &settingsButton_, &exitButton_})
        {
            button->pointerMoved(x, y);
        }
    }

    void MainMenu::pointerPressed(float x, float y) noexcept
    {
        if (settingsOpen_)
        {
            static_cast<void>(closeSettingsButton_.pointerPressed(x, y));
            static_cast<void>(windowModeButton_.pointerPressed(x, y));
            if (dropdownOpen_)
            {
                bool inDropdown = windowModeButton_.containsPoint(x, y);
                for (auto& button : windowModeOptions_)
                {
                    inDropdown = button.pointerPressed(x, y) || inDropdown;
                }
                if (!inDropdown)
                {
                    dropdownOpen_ = false;
                }
            }
            return;
        }
        for (auto* button :
             {&playButton_, &tutorialButton_, &settingsButton_, &exitButton_})
        {
            static_cast<void>(button->pointerPressed(x, y));
        }
    }

    MainMenuAction MainMenu::pointerReleased(float x, float y) noexcept
    {
        if (settingsOpen_)
        {
            if (closeSettingsButton_.pointerReleased(x, y))
            {
                settingsOpen_ = dropdownOpen_ = false;
                return MainMenuAction::None;
            }
            if (dropdownOpen_)
            {
                for (std::size_t i = 0; i < windowModeOptions_.size(); ++i)
                {
                    if (windowModeOptions_[i].pointerReleased(x, y))
                    {
                        requestedWindowMode_ = WindowMode(i);
                        dropdownOpen_ = false;
                        displayError_.clear();
                        return MainMenuAction::ChangeWindowMode;
                    }
                }
            }
            if (windowModeButton_.pointerReleased(x, y))
            {
                dropdownOpen_ = !dropdownOpen_;
            }
            return MainMenuAction::None;
        }
        const bool play = playButton_.pointerReleased(x, y);
        const bool tutorial = tutorialButton_.pointerReleased(x, y);
        const bool settings = settingsButton_.pointerReleased(x, y);
        const bool exit = exitButton_.pointerReleased(x, y);
        if (settings)
        {
            settingsOpen_ = true;
            displayError_.clear();
        }
        if (play)
        {
            return MainMenuAction::Play;
        }
        if (tutorial)
        {
            return MainMenuAction::Tutorial;
        }
        if (exit)
        {
            return MainMenuAction::Exit;
        }
        return MainMenuAction::None;
    }

    void MainMenu::render(Renderer& renderer, const GrayUiRenderer& ui) const
    {
        ui.drawMainMenuBackground(renderer);
        ui.drawTitle(
            renderer,
            "PALADIN",
            float(renderer.outputWidth()) * .5F,
            105
        );
        for (const auto* button :
             {&playButton_, &tutorialButton_, &settingsButton_, &exitButton_})
        {
            button->render(renderer, ui);
        }
        if (!settingsOpen_)
        {
            return;
        }
        ui.drawModalBackdrop(renderer);
        ui.drawPanel(renderer, settingsBounds_);
        const auto& b = settingsBounds_;
        ui.drawLabel(renderer, "SETTINGS", b.x + 24, b.y + 21, 3);
        displayTab_.render(renderer, ui);
        ui.drawLabel(renderer, "Window Mode:", b.x + 24, b.y + 151, 2);
        windowModeButton_.render(renderer, ui);
        ui.drawLabel(renderer, "v", b.x + b.width - 39, b.y + 153, 1.5F);
        closeSettingsButton_.render(renderer, ui);
        if (!displayError_.empty())
        {
            ui.drawLabel(
                renderer,
                displayError_,
                b.x + 24,
                b.y + 315,
                1.4F,
                {215, 140, 120, 255}
            );
        }
        if (dropdownOpen_)
        {
            for (const auto& button : windowModeOptions_)
            {
                button.render(renderer, ui);
            }
        }
    }
} // namespace Paladin

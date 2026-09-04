#pragma once

#include "ui/UiButton.h"

namespace Paladin
{
    class GrayUiRenderer;
    class Renderer;

    enum class MainMenuAction
    {
        None,
        Play,
        Tutorial,
        Exit
    };

    class MainMenu
    {
    public:
        MainMenu();

        void layout(int viewportWidth, int viewportHeight) noexcept;
        void pointerMoved(float x, float y) noexcept;

        void pointerPressed(float x, float y) noexcept;

        [[nodiscard]]
        MainMenuAction pointerReleased(
            float x,
            float y
        ) noexcept;

        void render(
            Renderer& renderer,
            const GrayUiRenderer& uiRenderer
        ) const;

    private:
        UiButton playButton_;
        UiButton tutorialButton_;
        UiButton exitButton_;
    };
}

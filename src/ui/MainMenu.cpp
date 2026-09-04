#include "ui/MainMenu.h"

#include "rendering/Renderer.h"
#include "ui/GrayUiRenderer.h"

namespace Paladin
{
    MainMenu::MainMenu()
        : playButton_("Play"),
          tutorialButton_("Tutorial"),
          exitButton_("Exit")
    {
    }

    void MainMenu::layout(
        int viewportWidth,
        int viewportHeight
    ) noexcept
    {
        constexpr float buttonWidth = 220.0F;
        constexpr float buttonHeight = 44.0F;
        constexpr float buttonGap = 16.0F;

        constexpr float totalHeight =
            buttonHeight * 3.0F
            + buttonGap * 2.0F;

        const float left =
            (
                static_cast<float>(viewportWidth)
                - buttonWidth
            ) * 0.5F;

        const float top =
            (
                static_cast<float>(viewportHeight)
                - totalHeight
            ) * 0.5F;

        playButton_.setBounds({
            left,
            top,
            buttonWidth,
            buttonHeight
        });

        tutorialButton_.setBounds({
            left,
            top + buttonHeight + buttonGap,
            buttonWidth,
            buttonHeight
        });

        exitButton_.setBounds({
            left,
            top + (buttonHeight + buttonGap) * 2.0F,
            buttonWidth,
            buttonHeight
        });
    }

    void MainMenu::pointerMoved(float x, float y) noexcept
    {
        playButton_.pointerMoved(x, y);
        tutorialButton_.pointerMoved(x, y);
        exitButton_.pointerMoved(x, y);
    }

    void MainMenu::pointerPressed(float x, float y) noexcept
    {
        const bool playPressed =
            playButton_.pointerPressed(x, y);

        const bool tutorialPressed =
            tutorialButton_.pointerPressed(x, y);

        const bool exitPressed =
            exitButton_.pointerPressed(x, y);

        if (!playPressed)
        {
            playButton_.cancelPress();
        }

        if (!tutorialPressed)
        {
            tutorialButton_.cancelPress();
        }

        if (!exitPressed)
        {
            exitButton_.cancelPress();
        }
    }

    MainMenuAction MainMenu::pointerReleased(
        float x,
        float y
    ) noexcept
    {
        const bool playClicked =
            playButton_.pointerReleased(x, y);

        const bool tutorialClicked =
            tutorialButton_.pointerReleased(x, y);

        const bool exitClicked =
            exitButton_.pointerReleased(x, y);

        if (playClicked)
        {
            return MainMenuAction::Play;
        }

        if (tutorialClicked)
        {
            return MainMenuAction::Tutorial;
        }

        if (exitClicked)
        {
            return MainMenuAction::Exit;
        }

        return MainMenuAction::None;
    }

    void MainMenu::render(
        Renderer& renderer,
        const GrayUiRenderer& uiRenderer
    ) const
    {
        uiRenderer.drawMainMenuBackground(renderer);

        uiRenderer.drawTitle(
            renderer,
            "PALADIN",
            static_cast<float>(renderer.outputWidth()) * 0.5F,
            105.0F
        );

        playButton_.render(renderer, uiRenderer);
        tutorialButton_.render(renderer, uiRenderer);
        exitButton_.render(renderer, uiRenderer);
    }
}

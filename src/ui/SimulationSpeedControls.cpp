#include "ui/SimulationSpeedControls.h"

#include "rendering/Renderer.h"
#include "ui/GrayUiRenderer.h"

#include <cmath>

namespace Paladin
{
    namespace
    {
        constexpr float buttonSide = 36.0F;

        bool nearlyEqual(double left, double right) noexcept
        {
            return std::abs(left - right) < 0.001;
        }
    }


    SimulationSpeedControls::SimulationSpeedControls()
        : buttons_{
              UiButton(""),
              UiButton("1x"),
              UiButton("2x"),
              UiButton("3x")
          }
    {
    }


    void SimulationSpeedControls::layout(int viewportWidth) noexcept
    {
        const float rowWidth =
            buttonSide * static_cast<float>(ButtonCount);

        const float rowX =
            (static_cast<float>(viewportWidth) - rowWidth) * 0.5F;

        for (std::size_t index = 0; index < buttons_.size(); ++index)
        {
            bounds_[index] = {
                rowX + static_cast<float>(index) * buttonSide,
                8.0F,
                buttonSide,
                buttonSide
            };

            buttons_[index].setBounds(bounds_[index]);
        }
    }


    void SimulationSpeedControls::setPlaybackState(
        bool paused,
        double speedMultiplier
    )
    {
        const bool fastFive = !paused && nearlyEqual(speedMultiplier, 5.0);
        buttons_[3].setText(fastFive ? "5x" : "3x");

        buttons_[0].setSelected(paused);
        buttons_[1].setSelected(
            !paused && nearlyEqual(speedMultiplier, 1.0)
        );
        buttons_[2].setSelected(
            !paused && nearlyEqual(speedMultiplier, 2.0)
        );
        buttons_[3].setSelected(
            !paused &&
            (
                nearlyEqual(speedMultiplier, 3.0) ||
                nearlyEqual(speedMultiplier, 5.0)
            )
        );
    }


    void SimulationSpeedControls::pointerMoved(
        float x,
        float y
    ) noexcept
    {
        for (UiButton& button : buttons_)
        {
            button.pointerMoved(x, y);
        }
    }


    bool SimulationSpeedControls::pointerPressed(
        float x,
        float y
    ) noexcept
    {
        bool captured = false;

        for (UiButton& button : buttons_)
        {
            captured = button.pointerPressed(x, y) || captured;
        }

        return captured;
    }


    bool SimulationSpeedControls::containsInteractivePoint(
        float x,
        float y
    ) const noexcept
    {
        for (const UiButton& button : buttons_)
        {
            if (button.containsPoint(x, y))
            {
                return true;
            }
        }

        return false;
    }


    SimulationSpeedControlAction
    SimulationSpeedControls::pointerReleased(
        float x,
        float y
    ) noexcept
    {
        std::array<bool, ButtonCount> clicked{};

        for (std::size_t index = 0; index < buttons_.size(); ++index)
        {
            clicked[index] = buttons_[index].pointerReleased(x, y);
        }

        if (clicked[0])
        {
            return SimulationSpeedControlAction::Pause;
        }

        if (clicked[1])
        {
            return SimulationSpeedControlAction::Normal;
        }

        if (clicked[2])
        {
            return SimulationSpeedControlAction::Double;
        }

        if (clicked[3])
        {
            return SimulationSpeedControlAction::Fast;
        }

        return SimulationSpeedControlAction::None;
    }


    void SimulationSpeedControls::render(
        Renderer& renderer,
        const GrayUiRenderer& uiRenderer
    ) const
    {
        for (const UiButton& button : buttons_)
        {
            button.render(renderer, uiRenderer);
        }

        const UiRectangle& pauseBounds = bounds_[0];
        constexpr float pauseBarWidth = 4.0F;
        constexpr float pauseBarHeight = 16.0F;
        constexpr float pauseBarGap = 4.0F;

        const float firstBarX =
            pauseBounds.x
            + (pauseBounds.width - pauseBarWidth * 2.0F - pauseBarGap)
                * 0.5F;

        const float barY =
            pauseBounds.y + (pauseBounds.height - pauseBarHeight) * 0.5F;

        renderer.fillRectangle(
            firstBarX,
            barY,
            pauseBarWidth,
            pauseBarHeight,
            {242, 242, 244, 255}
        );

        renderer.fillRectangle(
            firstBarX + pauseBarWidth + pauseBarGap,
            barY,
            pauseBarWidth,
            pauseBarHeight,
            {242, 242, 244, 255}
        );
    }
}

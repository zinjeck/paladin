#include "ui/SimulationSpeedControls.h"

#include "rendering/Renderer.h"
#include "ui/GrayUiRenderer.h"

#include <cmath>

namespace Paladin
{
namespace
{
constexpr float buttonSide = SimulationSpeedControls::ButtonSide;

bool nearlyEqual(double left, double right) noexcept
{
    return std::abs(left - right) < 0.001;
}
} // namespace

SimulationSpeedControls::SimulationSpeedControls() :
    buttons_{UiButton(""), UiButton(""), UiButton(""), UiButton("")}
{
    buttons_[0].setSkinId("time-pause");
    buttons_[1].setSkinId("time-normal");
    buttons_[2].setSkinId("time-double");
    buttons_[3].setSkinId("time-fast");
}

void SimulationSpeedControls::layout(int viewportWidth) noexcept
{
    for (std::size_t index = 0; index < buttons_.size(); ++index)
    {
        bounds_[index] = {
            static_cast<float>(viewportWidth) -
                static_cast<float>(index == 0 ? 1 : 5 - index) * buttonSide,
            0.0F,
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
    buttons_[0].setSelected(paused);
    buttons_[1].setSelected(!paused && nearlyEqual(speedMultiplier, 1.0));
    buttons_[2].setSelected(!paused && nearlyEqual(speedMultiplier, 2.0));
    buttons_[3].setSelected(
        !paused &&
        (nearlyEqual(speedMultiplier, 3.0) || nearlyEqual(speedMultiplier, 5.0))
    );
}

void SimulationSpeedControls::pointerMoved(float x, float y) noexcept
{
    for (UiButton& button : buttons_)
    {
        button.pointerMoved(x, y);
    }
}

bool SimulationSpeedControls::pointerPressed(float x, float y) noexcept
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

SimulationSpeedControlAction SimulationSpeedControls::pointerReleased(
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

    // Pixel-stepped right arrows: one, two, then three speed tiers.
    for (std::size_t level = 1; level < buttons_.size(); ++level)
    {
        const auto& box = bounds_[level];
        constexpr float arrowWidth = 10.0F;
        constexpr float spacing = 2.0F;
        const float width = float(level) * (arrowWidth + spacing) - spacing;
        const float left = box.x + (box.width - width) * 0.5F;
        for (std::size_t arrow = 0; arrow < level; ++arrow)
        {
            for (int column = 0; column < 5; ++column)
            {
                const float height = float(5 - column) * 4.0F;
                renderer.fillRectangle(
                    left + float(arrow) * (arrowWidth + spacing) +
                        column * 2.0F,
                    box.y + (box.height - height) * 0.5F,
                    2.0F,
                    height,
                    {242, 242, 244, 255}
                );
            }
        }
    }

    const UiRectangle& pauseBounds = bounds_[0];
    constexpr float pauseBarWidth = 4.0F;
    constexpr float pauseBarHeight = 16.0F;
    constexpr float pauseBarGap = 4.0F;

    const float firstBarX =
        pauseBounds.x +
        (pauseBounds.width - pauseBarWidth * 2.0F - pauseBarGap) * 0.5F;

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
} // namespace Paladin

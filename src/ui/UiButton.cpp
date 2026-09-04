#include "ui/UiButton.h"

#include "ui/GrayUiRenderer.h"

#include <utility>

namespace Paladin
{
    UiButton::UiButton(std::string text)
        : text_(std::move(text))
    {
    }

    void UiButton::setText(std::string text)
    {
        text_ = std::move(text);
    }

    void UiButton::setBounds(UiRectangle bounds) noexcept
    {
        bounds_ = bounds;
    }

    void UiButton::setSelected(bool selected) noexcept
    {
        selected_ = selected;
    }

    void UiButton::setEnabled(bool enabled) noexcept
    {
        enabled_ = enabled;

        if (!enabled_)
        {
            hovered_ = false;
            pressed_ = false;
        }
    }

    void UiButton::pointerMoved(float x, float y) noexcept
    {
        hovered_ = enabled_ && bounds_.contains(x, y);
    }

    bool UiButton::containsPoint(float x, float y) const noexcept
    {
        return enabled_ && bounds_.contains(x, y);
    }

    bool UiButton::pointerPressed(float x, float y) noexcept
    {
        pressed_ = enabled_ && bounds_.contains(x, y);
        return pressed_;
    }

    bool UiButton::pointerReleased(float x, float y) noexcept
    {
        const bool clicked =
            enabled_ && pressed_ && bounds_.contains(x, y);

        pressed_ = false;
        return clicked;
    }

    void UiButton::cancelPress() noexcept
    {
        pressed_ = false;
    }

    void UiButton::render(
        Renderer& renderer,
        const GrayUiRenderer& uiRenderer
    ) const
    {
        uiRenderer.drawButton(
            renderer,
            bounds_,
            text_,
            hovered_,
            pressed_,
            selected_,
            enabled_
        );
    }
}

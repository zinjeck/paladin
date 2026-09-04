#include "ui/UiButton.h"

#include "ui/GrayUiRenderer.h"

#include <utility>

namespace Paladin
{
    UiButton::UiButton(std::string text)
        : text_(std::move(text))
    {
    }

    void UiButton::setBounds(UiRectangle bounds) noexcept
    {
        bounds_ = bounds;
    }

    void UiButton::setSelected(bool selected) noexcept
    {
        selected_ = selected;
    }

    void UiButton::pointerMoved(float x, float y) noexcept
    {
        hovered_ = bounds_.contains(x, y);
    }

    bool UiButton::pointerPressed(float x, float y) noexcept
    {
        pressed_ = bounds_.contains(x, y);
        return pressed_;
    }

    bool UiButton::pointerReleased(float x, float y) noexcept
    {
        const bool clicked =
            pressed_ && bounds_.contains(x, y);

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
            selected_
        );
    }
}

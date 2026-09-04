#pragma once

#include "ui/UiTypes.h"

#include <string>

namespace Paladin
{
    class GrayUiRenderer;
    class Renderer;

    class UiButton
    {
    public:
        explicit UiButton(std::string text);

        void setBounds(UiRectangle bounds) noexcept;
        void setSelected(bool selected) noexcept;

        void pointerMoved(float x, float y) noexcept;

        [[nodiscard]]
        bool pointerPressed(float x, float y) noexcept;

        [[nodiscard]]
        bool pointerReleased(float x, float y) noexcept;

        void cancelPress() noexcept;

        void render(
            Renderer& renderer,
            const GrayUiRenderer& uiRenderer
        ) const;

    private:
        std::string text_;
        UiRectangle bounds_;
        bool hovered_ = false;
        bool pressed_ = false;
        bool selected_ = false;
    };
}

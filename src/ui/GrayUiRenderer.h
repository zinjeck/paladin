#pragma once

#include "rendering/Renderer.h"
#include "ui/BitmapFontRenderer.h"
#include "ui/UiTypes.h"

#include <string_view>

namespace Paladin
{
    class GrayUiRenderer
    {
    public:
        void drawMainMenuBackground(
            Renderer& renderer
        ) const;

        void drawTitle(
            Renderer& renderer,
            std::string_view text,
            float centerX,
            float y
        ) const;

        void drawButton(
            Renderer& renderer,
            const UiRectangle& bounds,
            std::string_view text,
            bool hovered,
            bool pressed,
            bool selected,
            bool enabled
        ) const;

        void drawModalBackdrop(
            Renderer& renderer
        ) const;

        void drawPanel(
            Renderer& renderer,
            const UiRectangle& bounds
        ) const;

        void drawLabel(
            Renderer& renderer,
            std::string_view text,
            float x,
            float y,
            float pixelSize = 3.0F,
            RenderColor color = {242, 242, 244, 255}
        ) const;

        void drawTextField(
            Renderer& renderer,
            const UiRectangle& bounds,
            std::string_view text,
            std::string_view placeholder,
            bool focused
        ) const;

        void drawColorSwatch(
            Renderer& renderer,
            const UiRectangle& bounds,
            RenderColor color,
            bool hovered,
            bool selected
        ) const;

        void drawChoiceCard(
            Renderer& renderer,
            const UiRectangle& bounds,
            std::string_view text,
            bool hovered,
            bool pressed,
            bool selected
        ) const;

    private:
        BitmapFontRenderer fontRenderer_;
    };
}

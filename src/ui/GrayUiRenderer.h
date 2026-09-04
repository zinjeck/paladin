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
            bool selected
        ) const;

    private:
        BitmapFontRenderer fontRenderer_;
    };
}

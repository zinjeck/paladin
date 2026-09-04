#pragma once

#include "rendering/Renderer.h"

#include <string_view>

namespace Paladin
{
    class BitmapFontRenderer
    {
    public:
        [[nodiscard]]
        float measureWidth(
            std::string_view text,
            float pixelSize
        ) const noexcept;

        void drawText(
            Renderer& renderer,
            std::string_view text,
            float x,
            float y,
            float pixelSize,
            RenderColor color
        ) const;
    };
}

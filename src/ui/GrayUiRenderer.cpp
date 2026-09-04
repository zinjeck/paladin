#include "ui/GrayUiRenderer.h"

namespace Paladin
{
    void GrayUiRenderer::drawMainMenuBackground(
        Renderer& renderer
    ) const
    {
        renderer.fillRectangle(
            0.0F,
            0.0F,
            static_cast<float>(renderer.outputWidth()),
            static_cast<float>(renderer.outputHeight()),
            {4, 9, 28, 255}
        );
    }

    void GrayUiRenderer::drawTitle(
        Renderer& renderer,
        std::string_view text,
        float centerX,
        float y
    ) const
    {
        constexpr float pixelSize = 10.0F;

        const float width =
            fontRenderer_.measureWidth(text, pixelSize);

        const float x = centerX - width * 0.5F;

        fontRenderer_.drawText(
            renderer,
            text,
            x + 3.0F,
            y + 3.0F,
            pixelSize,
            {0, 0, 0, 190}
        );

        fontRenderer_.drawText(
            renderer,
            text,
            x,
            y,
            pixelSize,
            {245, 245, 245, 255}
        );
    }

    void GrayUiRenderer::drawButton(
        Renderer& renderer,
        const UiRectangle& bounds,
        std::string_view text,
        bool hovered,
        bool pressed,
        bool selected
    ) const
    {
        RenderColor fillColor{78, 78, 82, 255};

        if (selected)
        {
            fillColor = {92, 102, 112, 255};
        }

        if (hovered)
        {
            fillColor = {104, 104, 110, 255};
        }

        if (pressed)
        {
            fillColor = {58, 58, 62, 255};
        }

        renderer.fillRectangle(
            bounds.x,
            bounds.y,
            bounds.width,
            bounds.height,
            {150, 150, 156, 255}
        );

        renderer.fillRectangle(
            bounds.x + 2.0F,
            bounds.y + 2.0F,
            bounds.width - 4.0F,
            bounds.height - 4.0F,
            fillColor
        );

        constexpr float textPixelSize = 3.0F;

        const float textWidth =
            fontRenderer_.measureWidth(
                text,
                textPixelSize
            );

        const float textHeight = 7.0F * textPixelSize;

        fontRenderer_.drawText(
            renderer,
            text,
            bounds.x + (bounds.width - textWidth) * 0.5F,
            bounds.y + (bounds.height - textHeight) * 0.5F,
            textPixelSize,
            {242, 242, 244, 255}
        );
    }
}

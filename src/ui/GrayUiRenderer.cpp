#include "ui/GrayUiRenderer.h"

#include <algorithm>

namespace Paladin
{
void GrayUiRenderer::drawMainMenuBackground(Renderer& renderer) const
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

    const float width = fontRenderer_.measureWidth(text, pixelSize);

    const float x = centerX - width * 0.5F;

    fontRenderer_.drawText(
        renderer,
        text,
        x + 3.0F,
        y + 3.0F,
        pixelSize,
        {0, 0, 0, 190}
    );

    fontRenderer_
        .drawText(renderer, text, x, y, pixelSize, {245, 245, 245, 255});
}

void GrayUiRenderer::drawButton(
    Renderer& renderer,
    const UiRectangle& bounds,
    std::string_view text,
    bool hovered,
    bool pressed,
    bool selected,
    bool enabled,
    std::string_view skinId
) const
{
    RenderColor fillColor{78, 78, 82, 255};

    if (!enabled)
    {
        fillColor = {56, 56, 59, 85};
    }
    else if (selected)
    {
        fillColor = {92, 102, 112, 255};
    }

    if (enabled && hovered)
    {
        fillColor = {104, 104, 110, 255};
    }

    if (enabled && pressed)
    {
        fillColor = {58, 58, 62, 255};
    }

    if (!drawButtonSprite(
            renderer,
            bounds,
            skinId.empty() ? text : skinId,
            hovered,
            pressed,
            selected,
            enabled
        ))
    {
        renderer.fillRectangle(
            bounds.x,
            bounds.y,
            bounds.width,
            bounds.height,
            {150, 150, 156, static_cast<std::uint8_t>(enabled ? 255 : 100)}
        );

        renderer.fillRectangle(
            bounds.x + 2.0F,
            bounds.y + 2.0F,
            bounds.width - 4.0F,
            bounds.height - 4.0F,
            fillColor
        );
    }

    constexpr float preferredTextPixelSize = 3.0F;
    const float horizontalTextPadding = text.size() == 1 ? 8.0F : 20.0F;

    const float textWidthAtPreferredSize =
        fontRenderer_.measureWidth(text, preferredTextPixelSize);

    const float availableTextWidth =
        std::max(1.0F, bounds.width - horizontalTextPadding);

    const float textPixelSize = textWidthAtPreferredSize > availableTextWidth
                                    ? preferredTextPixelSize *
                                          availableTextWidth /
                                          textWidthAtPreferredSize
                                    : preferredTextPixelSize;

    const float textWidth = fontRenderer_.measureWidth(text, textPixelSize);

    const float textHeight = 7.0F * textPixelSize;

    fontRenderer_.drawText(
        renderer,
        text,
        bounds.x + (bounds.width - textWidth) * 0.5F,
        bounds.y + (bounds.height - textHeight) * 0.5F,
        textPixelSize,
        enabled ? RenderColor{242, 242, 244, 255}
                : RenderColor{142, 142, 146, 255}
    );
}

void GrayUiRenderer::drawModalBackdrop(Renderer& renderer) const
{
    renderer.fillRectangle(
        0.0F,
        0.0F,
        static_cast<float>(renderer.outputWidth()),
        static_cast<float>(renderer.outputHeight()),
        {0, 0, 0, 150}
    );
}

void GrayUiRenderer::drawPanel(
    Renderer& renderer,
    const UiRectangle& bounds
) const
{
    renderer.fillRectangle(
        bounds.x,
        bounds.y,
        bounds.width,
        bounds.height,
        {166, 166, 172, 255}
    );

    renderer.fillRectangle(
        bounds.x + 3.0F,
        bounds.y + 3.0F,
        bounds.width - 6.0F,
        bounds.height - 6.0F,
        {64, 64, 69, 248}
    );
}

void GrayUiRenderer::drawLabel(
    Renderer& renderer,
    std::string_view text,
    float x,
    float y,
    float pixelSize,
    RenderColor color
) const
{
    fontRenderer_.drawText(renderer, text, x, y, pixelSize, color);
}

void GrayUiRenderer::drawTextField(
    Renderer& renderer,
    const UiRectangle& bounds,
    std::string_view text,
    std::string_view placeholder,
    bool focused
) const
{
    renderer.fillRectangle(
        bounds.x,
        bounds.y,
        bounds.width,
        bounds.height,
        focused ? RenderColor{222, 222, 226, 255}
                : RenderColor{132, 132, 138, 255}
    );

    renderer.fillRectangle(
        bounds.x + 2.0F,
        bounds.y + 2.0F,
        bounds.width - 4.0F,
        bounds.height - 4.0F,
        {43, 43, 47, 255}
    );

    const std::string_view displayedText = text.empty() ? placeholder : text;

    constexpr float preferredPixelSize = 3.0F;
    constexpr float horizontalPadding = 20.0F;

    const float widthAtPreferredSize =
        fontRenderer_.measureWidth(displayedText, preferredPixelSize);

    const float availableWidth =
        std::max(1.0F, bounds.width - horizontalPadding);

    const float pixelSize =
        widthAtPreferredSize > availableWidth
            ? preferredPixelSize * availableWidth / widthAtPreferredSize
            : preferredPixelSize;

    const float textY = bounds.y + (bounds.height - 7.0F * pixelSize) * 0.5F;

    fontRenderer_.drawText(
        renderer,
        displayedText,
        bounds.x + 10.0F,
        textY,
        pixelSize,
        text.empty() ? RenderColor{142, 142, 148, 255}
                     : RenderColor{244, 244, 246, 255}
    );

    if (focused && !text.empty())
    {
        const float caretX = bounds.x + 10.0F +
                             fontRenderer_.measureWidth(text, pixelSize) +
                             pixelSize;

        renderer.fillRectangle(
            std::min(caretX, bounds.x + bounds.width - 7.0F),
            textY,
            2.0F,
            7.0F * pixelSize,
            {244, 244, 246, 255}
        );
    }
}

void GrayUiRenderer::drawColorSwatch(
    Renderer& renderer,
    const UiRectangle& bounds,
    RenderColor color,
    bool hovered,
    bool selected
) const
{
    const RenderColor borderColor = selected  ? RenderColor{250, 250, 252, 255}
                                    : hovered ? RenderColor{194, 194, 200, 255}
                                              : RenderColor{112, 112, 118, 255};

    const float borderWidth = selected ? 4.0F : 2.0F;

    renderer.fillRectangle(
        bounds.x,
        bounds.y,
        bounds.width,
        bounds.height,
        borderColor
    );

    renderer.fillRectangle(
        bounds.x + borderWidth,
        bounds.y + borderWidth,
        bounds.width - borderWidth * 2.0F,
        bounds.height - borderWidth * 2.0F,
        color
    );
    drawButtonSprite(
        renderer,
        bounds,
        "color-swatch",
        hovered,
        false,
        selected,
        true
    );
}

void GrayUiRenderer::drawChoiceCard(
    Renderer& renderer,
    const UiRectangle& bounds,
    std::string_view text,
    bool hovered,
    bool pressed,
    bool selected
) const
{
    RenderColor borderColor{132, 132, 138, 255};
    RenderColor fillColor{72, 72, 77, 255};

    if (selected)
    {
        borderColor = {238, 238, 242, 255};
        fillColor = {91, 91, 98, 255};
    }
    else if (hovered)
    {
        borderColor = {190, 190, 196, 255};
        fillColor = {84, 84, 90, 255};
    }

    if (pressed)
    {
        fillColor = {55, 55, 60, 255};
    }

    const float borderWidth = selected ? 4.0F : 2.0F;

    if (!drawButtonSprite(
            renderer,
            bounds,
            "choice-card",
            hovered,
            pressed,
            selected,
            true
        ))
    {
        renderer.fillRectangle(
            bounds.x,
            bounds.y,
            bounds.width,
            bounds.height,
            borderColor
        );

        renderer.fillRectangle(
            bounds.x + borderWidth,
            bounds.y + borderWidth,
            bounds.width - borderWidth * 2.0F,
            bounds.height - borderWidth * 2.0F,
            fillColor
        );
    }
    constexpr float labelAreaHeight = 46.0F;

    renderer.fillRectangle(
        bounds.x + borderWidth,
        bounds.y + bounds.height - labelAreaHeight - borderWidth,
        bounds.width - borderWidth * 2.0F,
        labelAreaHeight,
        {48, 48, 53, 220}
    );

    constexpr float preferredPixelSize = 3.0F;
    const float textWidth =
        fontRenderer_.measureWidth(text, preferredPixelSize);

    fontRenderer_.drawText(
        renderer,
        text,
        bounds.x + (bounds.width - textWidth) * 0.5F,
        bounds.y + bounds.height - labelAreaHeight + 12.0F,
        preferredPixelSize,
        {244, 244, 246, 255}
    );
}
} // namespace Paladin

namespace Paladin
{
void GrayUiRenderer::setButtonSkin(std::string id, ButtonSpriteSkin skin)
{
    if (skin.atlas)
        buttonSkins_.insert_or_assign(std::move(id), std::move(skin));
}
void GrayUiRenderer::clearButtonSkins()
{
    buttonSkins_.clear();
}
bool GrayUiRenderer::drawButtonSprite(
    Renderer& renderer,
    const UiRectangle& bounds,
    std::string_view id,
    bool hovered,
    bool pressed,
    bool selected,
    bool enabled
) const
{
    auto found = buttonSkins_.find(std::string(id));
    if (found == buttonSkins_.end())
        found = buttonSkins_.find("default");
    if (found == buttonSkins_.end() || !found->second.atlas)
        return false;
    const auto& skin = found->second;
    const auto state = !enabled  ? ButtonVisualState::Disabled
                       : pressed ? ButtonVisualState::Pressed
                       : selected && hovered
                           ? ButtonVisualState::SelectedHovered
                       : selected ? ButtonVisualState::Selected
                       : hovered  ? ButtonVisualState::Hovered
                                  : ButtonVisualState::Normal;
    auto frame = skin.frames[std::size_t(state)];
    const bool fallback = frame.width <= 0 || frame.height <= 0;
    if (fallback)
        frame = skin.frames[0];
    if (frame.width <= 0 || frame.height <= 0)
        frame = {0, 0, float(skin.atlas->width()), float(skin.atlas->height())};
    const float border = std::clamp(
        skin.sliceBorder,
        0.0F,
        std::min(frame.width, frame.height) * .5F
    );
    const float dx = std::min(border, bounds.width * .5F);
    const float dy = std::min(border, bounds.height * .5F);
    const std::array sx{
        frame.x,
        frame.x + border,
        frame.x + frame.width - border,
        frame.x + frame.width
    };
    const std::array sy{
        frame.y,
        frame.y + border,
        frame.y + frame.height - border,
        frame.y + frame.height
    };
    const std::array tx{
        bounds.x,
        bounds.x + dx,
        bounds.x + bounds.width - dx,
        bounds.x + bounds.width
    };
    const std::array ty{
        bounds.y,
        bounds.y + dy,
        bounds.y + bounds.height - dy,
        bounds.y + bounds.height
    };
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 3; ++x)
            if (sx[x + 1] > sx[x] && sy[y + 1] > sy[y])
                renderer.drawTexture(
                    *skin.atlas,
                    sx[x],
                    sy[y],
                    sx[x + 1] - sx[x],
                    sy[y + 1] - sy[y],
                    tx[x],
                    ty[y],
                    tx[x + 1] - tx[x],
                    ty[y + 1] - ty[y]
                );
    if (!enabled && fallback)
        renderer.fillRectangle(
            bounds.x,
            bounds.y,
            bounds.width,
            bounds.height,
            {32, 32, 36, 150}
        );
    return true;
}
} // namespace Paladin

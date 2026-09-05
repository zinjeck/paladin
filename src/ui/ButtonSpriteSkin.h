#pragma once
#include "rendering/Texture.h"
#include "ui/UiTypes.h"
#include <array>
#include <memory>

namespace Paladin
{
    enum class ButtonVisualState
    {
        Normal,
        Hovered,
        Pressed,
        Selected,
        SelectedHovered,
        Disabled
    };
    struct ButtonSpriteSkin
    {
        std::shared_ptr<Texture> atlas;
        std::array<UiRectangle, 6> frames{};
        // Optional nine-slice border in source pixels. Zero stretches one
        // sprite.
        float sliceBorder = 0;
    };
} // namespace Paladin

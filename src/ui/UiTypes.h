#pragma once

namespace Paladin
{
    struct UiRectangle
    {
        float x = 0.0F;
        float y = 0.0F;
        float width = 0.0F;
        float height = 0.0F;

        [[nodiscard]]
        bool contains(
            float pointX,
            float pointY
        ) const noexcept
        {
            return
                pointX >= x &&
                pointY >= y &&
                pointX < x + width &&
                pointY < y + height;
        }
    };
}

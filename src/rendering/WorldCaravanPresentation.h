#pragma once
#include "rendering/WorldObjectPresentation.h"
#include "rendering/WorldPresentation.h"
#include "ui/UiTypes.h"
#include "world/WorldShipment.h"
#include <algorithm>
#include <array>
namespace Paladin
{
    inline constexpr std::array<const char*, 14> WorldCaravanRows{
        {"      hhhh      ",
         "    hhiiiihh    ",
         "   hiwwwwwih   ",
         "  hiwwwwwwwih  ",
         "  hiwwwwwwwih  ",
         "  hiwwwwwwwih  ",
         "  hhwwwwwiihh  ",
         "  hdhhhhhdhdh  ",
         "  dbbbbbbbbd   ",
         " ddbbbbbb bdd  ",
         " ddhdddddhddd  ",
         " ddhdddddhddd  ",
         "  dd     dd    ",
         "               "}
    };
    inline bool worldCaravanVisible(
        const WorldShipment& caravan,
        double pixels
    ) noexcept
    {
        return caravan.active() && !caravan.path.empty() &&
               worldArmyVisibility(pixels) > .001F;
    }
    inline float worldCaravanPixelStep(double pixels) noexcept
    {
        return float(worldMovingObjectHeight(pixels) / 24.);
    }
    inline UiRectangle worldCaravanBounds(
        double x,
        double y,
        double pixels
    ) noexcept
    {
        const float step = worldCaravanPixelStep(pixels);
        const float padding = std::max(4.F, step);
        return {
            float(x) - 16 * step - padding,
            float(y) - 18 * step - padding,
            32 * step + 2 * padding,
            24 * step + 2 * padding
        };
    }
} // namespace Paladin

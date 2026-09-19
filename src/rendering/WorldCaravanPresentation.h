#pragma once
#include "rendering/WorldObjectPresentation.h"
#include "rendering/WorldPresentation.h"
#include "ui/UiTypes.h"
#include "world/WorldShipment.h"
#include <array>
#include <algorithm>
namespace Paladin
{
    inline constexpr std::array<const char*, 14> WorldCaravanRows{{
        "      hhhh      ", "    hhiiiihh    ", "   hiwwwwwih   ",
        "  hiwwwwwwwih  ", "  hiwwwwwwwih  ", "  hiwwwwwwwih  ",
        "  hhwwwwwiihh  ", "  hdhhhhhdhdh  ", "  dbbbbbbbbd   ",
        " ddbbbbbb bdd  ", " ddhdddddhddd  ", " ddhdddddhddd  ",
        "  dd     dd    ", "               "}};
    inline bool worldCaravanVisible(const WorldShipment& caravan, double pixels) noexcept
    { return caravan.active() && !caravan.path.empty() && worldArmyVisibility(pixels) > .001F; }
    inline float worldCaravanPixelStep(double pixels) noexcept
    { return float(std::max(worldObjectPixelPitch(pixels), pixels / 16.)); }
    inline UiRectangle worldCaravanBounds(double x, double y, double pixels) noexcept
    {
        const float step = worldCaravanPixelStep(pixels);
        const float padding = std::max(4.F, step);
        return {float(x) - 8 * step - padding, float(y) - 13 * step - padding,
            16 * step + 2 * padding, 14 * step + 2 * padding};
    }
}

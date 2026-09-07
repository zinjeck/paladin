#pragma once
#include <algorithm>
namespace Paladin
{
    // Screen-space thresholds, independent of simulation speed and map size.
    inline constexpr double StaticDetailPixels = 16;
    inline constexpr double AnimationDetailPixels = 24;
    inline double detailBlend(double pixels, double begin, double end)
    {
        const double t = std::clamp((pixels - begin) / (end - begin), 0., 1.);
        return t * t * (3 - 2 * t);
    }
} // namespace Paladin

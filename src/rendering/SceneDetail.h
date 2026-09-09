#pragma once
#include <algorithm>
namespace Paladin
{
    // Screen-space thresholds, independent of simulation speed and map size.
    // Keep authored city detail readable across a broad screenshot-friendly
    // zoom range. Animation remains the more expensive, closer tier.
    inline constexpr double StaticDetailPixels = 10;
    inline constexpr double AnimationDetailPixels = 16;
    inline double detailBlend(double pixels, double begin, double end)
    {
        const double t = std::clamp((pixels - begin) / (end - begin), 0., 1.);
        return t * t * (3 - 2 * t);
    }
} // namespace Paladin
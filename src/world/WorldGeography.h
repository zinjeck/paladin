#pragma once
#include "world/WorldTilePosition.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace Paladin
{
    // Great-circle angular distance is independent of projection, zoom and
    // the longitude seam. Positions denote tile centres on the same planet.
    inline double geographicDistance(WorldTilePosition a, WorldTilePosition b,
                                     int width, int height) noexcept
    {
        if (width <= 0 || height <= 0) return std::numbers::pi;
        const double latA = ((double(a.y)+.5)/height-.5)*std::numbers::pi;
        const double latB = ((double(b.y)+.5)/height-.5)*std::numbers::pi;
        const double lon = std::remainder((double(a.x)-b.x)/width*2*std::numbers::pi,
                                         2*std::numbers::pi);
        const double sinLat=std::sin((latA-latB)*.5), sinLon=std::sin(lon*.5);
        const double h=sinLat*sinLat+std::cos(latA)*std::cos(latB)*sinLon*sinLon;
        return 2*std::asin(std::sqrt(std::clamp(h,0.0,1.0)));
    }
}

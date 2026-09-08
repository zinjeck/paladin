#pragma once
#include "rendering/ScenePresentation.h"
#include <cmath>
#include <string_view>

namespace Paladin
{
    // Presentation only: no collision, pathfinding or simulation dimensions.
    struct CityPresentation
    {
        bool roofsVisible = true;
        bool shadowsVisible = true;
        bool cloudsEnabled = true;
        bool daylightEnabled = true;
        bool localLightsEnabled = true;
        double treeElevation = .45;
        double detailTilePixels = 8;
        double viewAzimuthDegrees = 0;
        RenderColor shadowColor{0x63, 0x3E, 0x4B, 100};

        RenderColor ambient(double hour) const
        {
            // Cool night is applied separately from sunlit material colors.
            const double daylight = std::clamp(
                std::min((hour - 5.0) / 2.0, (21.0 - hour) / 2.0),
                0.0,
                1.0
            );
            return {
                18,
                25,
                48,
                std::uint8_t(daylightEnabled ? (1 - daylight) * 85 : 0)
            };
        }
    };
} // namespace Paladin

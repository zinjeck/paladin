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
        bool daylightEnabled = true;
        double wallHeight = .65;
        double wallThickness = .12;
        double treeElevation = .45;
        double detailTilePixels = 8;
        RenderColor shadowColor{12, 16, 24, 65};

        static bool enclosed(std::string_view type)
        {
            return type == "house" || type == "city_keep" || type == "bakery";
        }
        RenderColor ambient(double hour) const
        {
            // Deliberately restrained until the artist supplies a palette.
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

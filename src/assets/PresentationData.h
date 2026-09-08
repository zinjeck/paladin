#pragma once
#include "assets/AssetTypes.h"
namespace Paladin
{
    // Artist-facing recipe; object IDs and artwork never select renderer code.
    struct ObjectPresentation
    {
        std::string mode = "ground";
        std::string floor, wall, roof, sprite;
        double moduleWidth = 1, moduleDepth = 1;
        double height = 0, thickness = .12;
        int frontShade = 30, sideShade = 65, edgeLight = 32, shadowAlpha = 65;
        bool outline = true;
        std::uint32_t fillRgb = 0x999999, frameRgb = 0x555555;
        double bodyWidth = .6, bodyDepth = .45;
        // Optional shared accessory recipe, independent of wall/roof materials.
        std::string decor = "-";
    };
    struct BlueprintLight
    {
        std::string object;
        double x = 0, y = 0, radius = 4.5, intensity = 1;
        AssetPixel color{255, 180, 90, 255};
    };
    struct BuildingPiece
    {
        std::string object, sprite, state;
        double x = 0, y = 0, depth = 0;
        unsigned choices = 1, choice = 0;
    };
} // namespace Paladin

#pragma once

#include "world/BiomeType.h"
#include "world/EnvironmentalValues.h"
#include "world/TerrainType.h"

namespace Paladin
{
    enum class ReliefType : std::uint8_t
    {
        Lowland,
        Hills,
        Mountain
    };
    struct WorldTile
    {
        TerrainType terrain = TerrainType::Water;

        BiomeType biome = BiomeType::Ocean;

        Elevation elevation{};
        Temperature temperature{};
        Rainfall rainfall{};
        // Landform is independent of climate: tundra and polar slopes may
        // still be hills even though their biome remains cold.
        ReliefType relief = ReliefType::Lowland;
    };
} // namespace Paladin

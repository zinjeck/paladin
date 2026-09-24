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
    enum class MineralDeposit : std::uint8_t
    {
        None,
        Coal,
        Iron,
        Gold
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
        MineralDeposit mineral = MineralDeposit::None;
        // Local polar ice exclusion; connected habitable land remains eligible.
        bool polarContinent = false;
        bool rockFloor = false;
        bool caveInterior = false;
    };
} // namespace Paladin

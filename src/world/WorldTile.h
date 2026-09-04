#pragma once

#include "world/BiomeType.h"
#include "world/TerrainType.h"

namespace Paladin
{
    struct WorldTile
    {
        TerrainType terrain =
            TerrainType::Water;

        BiomeType biome =
            BiomeType::Ocean;
    };
}

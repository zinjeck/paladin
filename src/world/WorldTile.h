#pragma once

#include "world/TerrainType.h"

namespace Paladin
{
    struct WorldTile
    {
        TerrainType terrain =
            TerrainType::Water;
    };
}
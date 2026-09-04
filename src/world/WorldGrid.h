#pragma once

#include "world/TileGrid.h"
#include "world/WorldTilePosition.h"

namespace Paladin
{
    class WorldGrid final : public TileGrid<WorldTilePosition>
    {
    public:
        using TileGrid<WorldTilePosition>::TileGrid;
    };
}

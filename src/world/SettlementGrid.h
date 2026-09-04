#pragma once

#include "world/SettlementTilePosition.h"
#include "world/TileGrid.h"

namespace Paladin
{
    class SettlementGrid final : public TileGrid<SettlementTilePosition>
    {
    public:
        using TileGrid<SettlementTilePosition>::TileGrid;
    };
}

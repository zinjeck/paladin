#pragma once

#include "core/StrongId.h"
#include "world/WorldTilePosition.h"

#include <cstddef>
#include <cstdint>

namespace Paladin
{
    class TerritoryMap;
    class WorldGrid;

    struct TerritoryFoundationPolicy;

    class TerritoryFoundationSystem
    {
    public:
        [[nodiscard]]
        std::size_t establishSettlementTerritory(
            const WorldGrid& grid,
            TerritoryMap& territory,
            WorldTilePosition settlementPosition,
            PolityId polityId,
            const TerritoryFoundationPolicy& policy,
            std::uint32_t borderlandTraversalBudget
        ) const;
    };
}

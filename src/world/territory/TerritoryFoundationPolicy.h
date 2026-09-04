#pragma once

#include "world/TerrainType.h"

#include <cstdint>

namespace Paladin
{
    struct TerritoryTerrainRule
    {
        bool controllable = false;
        std::uint32_t borderlandTraversalCost = 0;
    };

    struct TerritoryFoundationPolicy
    {
        std::int32_t settlementRegionWidth = 9;
        std::int32_t settlementRegionHeight = 9;
        std::uint32_t settlementBorderlandTraversalBudget = 0;
        std::uint32_t capitalBorderlandTraversalBudget = 2;
        std::uint32_t borderlandIrregularityMaximumCost = 2;
        std::uint64_t borderlandShapeSalt =
            0x6A09E667F3BCC909ULL;

        TerritoryTerrainRule land{
            true,
            1
        };

        TerritoryTerrainRule water{
            false,
            0
        };

        TerritoryTerrainRule mountain{
            true,
            2
        };

        [[nodiscard]]
        const TerritoryTerrainRule& ruleFor(
            TerrainType terrain
        ) const noexcept;
    };

    [[nodiscard]]
    const TerritoryFoundationPolicy&
    defaultTerritoryFoundationPolicy() noexcept;
}

#pragma once

#include "world/SettlementKind.h"
#include "world/TerrainType.h"
#include "world/territory/TribalInfluencePolicy.h"

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
        std::uint64_t borderlandShapeSalt = 0x6A09E667F3BCC909ULL;

        TerritoryTerrainRule land{true, 1};

        TerritoryTerrainRule water{false, 0};

        TerritoryTerrainRule mountain{true, 2};

        // Civic realms use the discrete controller map above. Tribal realms do
        // not claim those cells; their authority is derived continuously from
        // population-driven power centers using this policy.
        TribalInfluencePolicy tribalInfluence;

        // Control is independent of the playable region. Cities retain their
        // 9x9 maps but claim a smaller core; a fortress projects a wider
        // frontier.
        TerritoryFoundationPolicy forSettlement(
            SettlementKind kind
        ) const noexcept
        {
            auto result = *this;
            if (kind == SettlementKind::City)
            {
                result.settlementRegionWidth =
                    std::max(1, settlementRegionWidth * 5 / 9);
                result.settlementRegionHeight =
                    std::max(1, settlementRegionHeight * 5 / 9);
            }
            else
            {
                result.settlementBorderlandTraversalBudget += 6;
                result.capitalBorderlandTraversalBudget += 6;
            }
            return result;
        }

        [[nodiscard]]
        const TerritoryTerrainRule& ruleFor(TerrainType terrain) const noexcept;
    };

    [[nodiscard]]
    const TerritoryFoundationPolicy&
    defaultTerritoryFoundationPolicy() noexcept;
} // namespace Paladin

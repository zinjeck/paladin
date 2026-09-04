#pragma once

#include "world/settlements/SettlementSimulationTier.h"

#include <cstdint>

namespace Paladin
{
    struct SettlementSimulationPolicy
    {
        std::uint64_t minimumStepMinutes = 1;

        [[nodiscard]]
        bool isValid() const noexcept
        {
            return minimumStepMinutes > 0;
        }
    };

    struct SettlementSimulationPolicies
    {
        SettlementSimulationPolicy detailed;
        SettlementSimulationPolicy inactive;
        SettlementSimulationPolicy strategic;

        [[nodiscard]]
        const SettlementSimulationPolicy& forTier(
            SettlementSimulationTier tier
        ) const noexcept;

        [[nodiscard]]
        bool isValid() const noexcept;
    };

    [[nodiscard]]
    SettlementSimulationPolicies
    defaultSettlementSimulationPolicies() noexcept;
}

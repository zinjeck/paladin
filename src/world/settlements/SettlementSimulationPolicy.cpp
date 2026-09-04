#include "world/settlements/SettlementSimulationPolicy.h"

namespace Paladin
{
    const SettlementSimulationPolicy&
    SettlementSimulationPolicies::forTier(
        SettlementSimulationTier tier
    ) const noexcept
    {
        switch (tier)
        {
            case SettlementSimulationTier::Detailed:
                return detailed;

            case SettlementSimulationTier::Inactive:
                return inactive;

            case SettlementSimulationTier::Strategic:
                return strategic;
        }

        return strategic;
    }


    bool SettlementSimulationPolicies::isValid() const noexcept
    {
        return
            detailed.isValid() &&
            inactive.isValid() &&
            strategic.isValid();
    }


    SettlementSimulationPolicies
    defaultSettlementSimulationPolicies() noexcept
    {
        constexpr std::uint64_t minutesPerHour = 60;
        constexpr std::uint64_t minutesPerDay = 24 * minutesPerHour;

        return {
            {1},
            {minutesPerHour},
            {30 * minutesPerDay}
        };
    }
}

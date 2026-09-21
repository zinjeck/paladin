#include "world/settlements/SettlementSimulationPolicy.h"

namespace Paladin
{
    const SettlementSimulationPolicy& SettlementSimulationPolicies::forTier(
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
        return detailed.isValid() && inactive.isValid() &&
               strategic.isValid() &&
               detailed.resolution ==
                   SettlementSimulationResolution::DetailedLocal &&
               inactive.resolution ==
                   SettlementSimulationResolution::InactiveLocalAggregate &&
               strategic.resolution ==
                   SettlementSimulationResolution::StrategicAggregate;
    }


    SettlementSimulationPolicies defaultSettlementSimulationPolicies() noexcept
    {
        constexpr std::uint64_t minutesPerHour = 60;

        return {
            {SettlementSimulationResolution::DetailedLocal, 1},
            {SettlementSimulationResolution::InactiveLocalAggregate,
             minutesPerHour},
            {SettlementSimulationResolution::StrategicAggregate,
             minutesPerHour}
        };
    }
} // namespace Paladin

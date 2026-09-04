#pragma once

namespace Paladin
{
    // A tier selects a simulation policy; it never changes who owns the
    // settlement or where its authoritative state lives.
    enum class SettlementSimulationTier
    {
        Detailed,
        Inactive,
        Strategic
    };

    [[nodiscard]]
    constexpr bool isSettlementSimulationTier(
        SettlementSimulationTier tier
    ) noexcept
    {
        switch (tier)
        {
            case SettlementSimulationTier::Detailed:
            case SettlementSimulationTier::Inactive:
            case SettlementSimulationTier::Strategic:
                return true;
        }

        return false;
    }
}

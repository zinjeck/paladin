#include "world/settlements/SettlementSimulationState.h"

#include <cmath>
#include <utility>

namespace Paladin
{
    bool SettlementSimulationState::bootstrap(
        const SettlementFoundationProfile& profile
    )
    {
        if (active_)
        {
            return false;
        }

        ResourceStockpile initialStockpile;

        for (const StockpileEntry& entry : profile.initialResources)
        {
            if (
                entry.resourceId.empty() ||
                !std::isfinite(entry.amount) ||
                entry.amount < 0.0 ||
                !initialStockpile.setAmount(
                    entry.resourceId,
                    entry.amount
                )
            )
            {
                return false;
            }
        }

        population_ = SettlementPopulation(
            profile.initialPopulation,
            profile.demographicRates
        );

        stockpile_ = std::move(initialStockpile);
        active_ = true;
        return true;
    }

    bool SettlementSimulationState::isActive() const noexcept
    {
        return active_;
    }

    SettlementPopulation&
    SettlementSimulationState::population() noexcept
    {
        return population_;
    }

    const SettlementPopulation&
    SettlementSimulationState::population() const noexcept
    {
        return population_;
    }

    ResourceStockpile&
    SettlementSimulationState::stockpile() noexcept
    {
        return stockpile_;
    }

    const ResourceStockpile&
    SettlementSimulationState::stockpile() const noexcept
    {
        return stockpile_;
    }
}

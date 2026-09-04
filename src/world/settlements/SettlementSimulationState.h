#pragma once

#include "world/settlements/ResourceStockpile.h"
#include "world/settlements/SettlementFoundationProfile.h"
#include "world/settlements/SettlementPopulation.h"

namespace Paladin
{
    class SettlementSimulationState
    {
    public:
        [[nodiscard]]
        bool bootstrap(
            const SettlementFoundationProfile& profile
        );

        [[nodiscard]]
        bool isActive() const noexcept;

        [[nodiscard]]
        SettlementPopulation& population() noexcept;

        [[nodiscard]]
        const SettlementPopulation& population() const noexcept;

        [[nodiscard]]
        ResourceStockpile& stockpile() noexcept;

        [[nodiscard]]
        const ResourceStockpile& stockpile() const noexcept;

    private:
        bool active_ = false;
        SettlementPopulation population_;
        ResourceStockpile stockpile_;
    };
}

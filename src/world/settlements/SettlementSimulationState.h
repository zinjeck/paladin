#pragma once

#include "world/settlements/ResourceStockpile.h"
#include "world/settlements/SettlementEconomy.h"
#include "world/settlements/SettlementFoundationProfile.h"
#include "world/settlements/SettlementPopulation.h"
#include "world/settlements/SettlementSimulationTier.h"

namespace Paladin
{
    class WorldSimulationPipeline;

    class SettlementSimulationState
    {
    public:
        [[nodiscard]]
        bool bootstrap(
            const SettlementFoundationProfile& profile
        );

        [[nodiscard]]
        bool isInitialized() const noexcept;

        [[nodiscard]]
        SettlementPopulation& population() noexcept;

        [[nodiscard]]
        const SettlementPopulation& population() const noexcept;

        [[nodiscard]]
        ResourceStockpile& stockpile() noexcept;

        [[nodiscard]]
        const ResourceStockpile& stockpile() const noexcept;

        [[nodiscard]]
        SettlementEconomy& economy() noexcept;

        [[nodiscard]]
        const SettlementEconomy& economy() const noexcept;

        void setSimulationTier(
            SettlementSimulationTier tier
        ) noexcept;

        [[nodiscard]]
        SettlementSimulationTier simulationTier() const noexcept;

    private:
        friend class WorldSimulationPipeline;

        [[nodiscard]]
        double takeDueSimulationSeconds(
            double gameDeltaSeconds
        ) noexcept;

        bool initialized_ = false;
        SettlementPopulation population_;
        ResourceStockpile stockpile_;
        SettlementEconomy economy_;
        SettlementSimulationTier simulationTier_ =
            SettlementSimulationTier::Summary;
        double pendingGameSeconds_ = 0.0;
    };
}

#include "world/settlements/SettlementSimulationState.h"

#include <cmath>
#include <utility>

namespace Paladin
{
    bool SettlementSimulationState::bootstrap(
        const SettlementFoundationProfile& profile
    )
    {
        if (initialized_)
        {
            return false;
        }

        ResourceStockpile initialStockpile;
        SettlementEconomy initialEconomy;

        if (!initialEconomy.configure(profile.resourceFlowRates))
        {
            return false;
        }

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

        for (const ResourceFlowRate& flowRate
            : initialEconomy.flowRates())
        {
            if (
                !initialStockpile.setAmount(
                    flowRate.resourceId,
                    initialStockpile.amount(flowRate.resourceId)
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
        economy_ = std::move(initialEconomy);
        simulationTier_ = profile.initialSimulationTier;
        pendingGameSeconds_ = 0.0;
        initialized_ = true;
        return true;
    }

    bool SettlementSimulationState::isInitialized() const noexcept
    {
        return initialized_;
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

    SettlementEconomy&
    SettlementSimulationState::economy() noexcept
    {
        return economy_;
    }

    const SettlementEconomy&
    SettlementSimulationState::economy() const noexcept
    {
        return economy_;
    }

    void SettlementSimulationState::setSimulationTier(
        SettlementSimulationTier tier
    ) noexcept
    {
        simulationTier_ = tier;
    }

    SettlementSimulationTier
    SettlementSimulationState::simulationTier() const noexcept
    {
        return simulationTier_;
    }

    double SettlementSimulationState::takeDueSimulationSeconds(
        double gameDeltaSeconds
    ) noexcept
    {
        if (
            !initialized_ ||
            !std::isfinite(gameDeltaSeconds) ||
            gameDeltaSeconds <= 0.0
        )
        {
            return 0.0;
        }

        pendingGameSeconds_ += gameDeltaSeconds;

        if (!std::isfinite(pendingGameSeconds_))
        {
            pendingGameSeconds_ = 0.0;
            return 0.0;
        }

        constexpr double gameSecondsPerDay =
            24.0 * 60.0 * 60.0;

        double minimumStepSeconds = 0.0;

        switch (simulationTier_)
        {
            case SettlementSimulationTier::Detailed:
                minimumStepSeconds = 0.0;
                break;

            case SettlementSimulationTier::Summary:
                minimumStepSeconds = gameSecondsPerDay;
                break;

            case SettlementSimulationTier::Strategic:
                minimumStepSeconds = 30.0 * gameSecondsPerDay;
                break;
        }

        if (pendingGameSeconds_ < minimumStepSeconds)
        {
            return 0.0;
        }

        const double dueSeconds = pendingGameSeconds_;
        pendingGameSeconds_ = 0.0;
        return dueSeconds;
    }
}

#include "world/settlements/SettlementSimulationState.h"

#include <cmath>
#include <limits>
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
        pendingSimulationMinutes_ = 0;
        totalSimulatedMinutes_ = 0;
        completedSimulationSteps_ = 0;
        schedulingVersion_ = 1;
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
        if (simulationTier_ == tier)
        {
            return;
        }

        simulationTier_ = tier;
        ++schedulingVersion_;
    }

    SettlementSimulationTier
    SettlementSimulationState::simulationTier() const noexcept
    {
        return simulationTier_;
    }


    std::uint64_t
    SettlementSimulationState::pendingSimulationMinutes() const noexcept
    {
        return pendingSimulationMinutes_;
    }


    std::uint64_t
    SettlementSimulationState::totalSimulatedMinutes() const noexcept
    {
        return totalSimulatedMinutes_;
    }


    std::uint64_t
    SettlementSimulationState::completedSimulationSteps() const noexcept
    {
        return completedSimulationSteps_;
    }


    SettlementStateVersions
    SettlementSimulationState::versions() const noexcept
    {
        return {
            population_.version(),
            stockpile_.version(),
            economy_.version(),
            schedulingVersion_
        };
    }


    std::uint64_t
    SettlementSimulationState::takeDueSimulationMinutes(
        std::uint64_t gameMinutes,
        const SettlementSimulationPolicy& policy
    ) noexcept
    {
        if (!initialized_ || gameMinutes == 0 || !policy.isValid())
        {
            return 0;
        }

        constexpr std::uint64_t maximumMinutes =
            std::numeric_limits<std::uint64_t>::max();

        if (gameMinutes > maximumMinutes - pendingSimulationMinutes_)
        {
            pendingSimulationMinutes_ = maximumMinutes;
        }
        else
        {
            pendingSimulationMinutes_ += gameMinutes;
        }

        if (pendingSimulationMinutes_ < policy.minimumStepMinutes)
        {
            ++schedulingVersion_;
            return 0;
        }

        const std::uint64_t dueMinutes =
            pendingSimulationMinutes_ -
            pendingSimulationMinutes_ % policy.minimumStepMinutes;

        pendingSimulationMinutes_ -= dueMinutes;

        if (dueMinutes > maximumMinutes - totalSimulatedMinutes_)
        {
            totalSimulatedMinutes_ = maximumMinutes;
        }
        else
        {
            totalSimulatedMinutes_ += dueMinutes;
        }

        ++completedSimulationSteps_;
        ++schedulingVersion_;
        return dueMinutes;
    }
}

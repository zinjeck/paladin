#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Paladin
{
    class ResourceStockpile;

    struct ResourceFlowRate
    {
        std::string resourceId;
        double dailyProductionPerResident = 0.0;
        double dailyConsumptionPerResident = 0.0;

        // Zero means this resource does not directly affect population.
        // Positive values allow several future needs to contribute without
        // hardcoding food into the population system.
        double populationNeedWeight = 0.0;
    };

    struct ResourceFlowSnapshot
    {
        std::string resourceId;
        double elapsedDays = 0.0;
        double openingAmount = 0.0;
        double producedAmount = 0.0;
        double requestedAmount = 0.0;
        double consumedAmount = 0.0;
        double closingAmount = 0.0;
        double fulfillment = 1.0;
        double sustainableSupplyRatio = 1.0;
    };

    class SettlementEconomy
    {
    public:
        [[nodiscard]]
        bool configure(
            const std::vector<ResourceFlowRate>& flowRates
        );

        void simulate(
            ResourceStockpile& stockpile,
            std::uint64_t residents,
            double elapsedDays
        );

        [[nodiscard]]
        std::span<const ResourceFlowRate> flowRates() const noexcept;

        [[nodiscard]]
        std::span<const ResourceFlowSnapshot> lastFlows() const noexcept;

        [[nodiscard]]
        double populationNeedFulfillment() const noexcept;

        [[nodiscard]]
        double populationSustainableSupplyRatio() const noexcept;

        [[nodiscard]]
        std::uint64_t version() const noexcept;

    private:
        std::vector<ResourceFlowRate> flowRates_;
        std::vector<ResourceFlowSnapshot> lastFlows_;
        double populationNeedFulfillment_ = 1.0;
        double populationSustainableSupplyRatio_ = 1.0;
        std::uint64_t version_ = 0;
    };
}

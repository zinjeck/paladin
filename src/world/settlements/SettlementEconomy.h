#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
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

    struct ResourceMarketQuote
    {
        double dailyNeed = 0;
        double dailyOutput = 0;
        double reserve = 0;
        double offered = 0;
        double wanted = 0;
        std::int64_t unitPrice = 0; // Existing currency cents, never ore.
    };

    class SettlementEconomy
    {
    public:
        [[nodiscard]]
        bool configure(const std::vector<ResourceFlowRate>& flowRates);

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

        ResourceMarketQuote quote(
            const ResourceStockpile&,
            std::uint64_t,
            std::string_view resource
        ) const;
        double happiness() const noexcept
        {
            return happiness_;
        }
        // Only AI aggregate economies opt into geographic forecasts. Detailed
        // cities retain their real inventories, citizens and production.
        std::uint64_t geographyRevision = ~std::uint64_t(0);

    private:
        std::vector<ResourceFlowRate> flowRates_;
        std::vector<ResourceFlowSnapshot> lastFlows_;
        double populationNeedFulfillment_ = 1.0;
        double populationSustainableSupplyRatio_ = 1.0;
        double happiness_ = .75;
        std::uint64_t version_ = 0;
    };
} // namespace Paladin

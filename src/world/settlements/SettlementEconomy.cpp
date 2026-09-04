#include "world/settlements/SettlementEconomy.h"

#include "world/settlements/ResourceStockpile.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace Paladin
{
    namespace
    {
        bool isValidFlowRate(
            const ResourceFlowRate& flowRate
        ) noexcept
        {
            return
                !flowRate.resourceId.empty() &&
                std::isfinite(
                    flowRate.dailyProductionPerResident
                ) &&
                flowRate.dailyProductionPerResident >= 0.0 &&
                std::isfinite(
                    flowRate.dailyConsumptionPerResident
                ) &&
                flowRate.dailyConsumptionPerResident >= 0.0 &&
                std::isfinite(flowRate.populationNeedWeight) &&
                flowRate.populationNeedWeight >= 0.0 &&
                (
                    flowRate.populationNeedWeight == 0.0 ||
                    flowRate.dailyConsumptionPerResident > 0.0
                );
        }

        double boundedProduct(
            double first,
            double second,
            double third
        ) noexcept
        {
            const double product = first * second * third;

            return std::isfinite(product)
                ? product
                : std::numeric_limits<double>::max();
        }

        double boundedSum(
            double first,
            double second
        ) noexcept
        {
            const double sum = first + second;

            return std::isfinite(sum)
                ? sum
                : std::numeric_limits<double>::max();
        }
    }

    bool SettlementEconomy::configure(
        const std::vector<ResourceFlowRate>& flowRates
    )
    {
        std::vector<ResourceFlowRate> sortedRates = flowRates;

        for (const ResourceFlowRate& flowRate : sortedRates)
        {
            if (!isValidFlowRate(flowRate))
            {
                return false;
            }
        }

        std::sort(
            sortedRates.begin(),
            sortedRates.end(),
            [](
                const ResourceFlowRate& first,
                const ResourceFlowRate& second
            )
            {
                return first.resourceId < second.resourceId;
            }
        );

        for (
            std::size_t index = 1;
            index < sortedRates.size();
            ++index
        )
        {
            if (
                sortedRates[index - 1].resourceId ==
                sortedRates[index].resourceId
            )
            {
                return false;
            }
        }

        std::vector<ResourceFlowSnapshot> snapshots;
        snapshots.reserve(sortedRates.size());

        for (const ResourceFlowRate& flowRate : sortedRates)
        {
            snapshots.push_back({flowRate.resourceId});
        }

        flowRates_ = std::move(sortedRates);
        lastFlows_ = std::move(snapshots);
        populationNeedFulfillment_ = 1.0;
        populationSustainableSupplyRatio_ = 1.0;
        return true;
    }

    void SettlementEconomy::simulate(
        ResourceStockpile& stockpile,
        std::uint64_t residents,
        double elapsedDays
    )
    {
        if (!std::isfinite(elapsedDays) || elapsedDays <= 0.0)
        {
            return;
        }

        double totalNeedWeight = 0.0;
        double weightedFulfillment = 0.0;
        double weightedSupplyRatio = 0.0;
        const double residentCount =
            static_cast<double>(residents);

        for (
            std::size_t index = 0;
            index < flowRates_.size();
            ++index
        )
        {
            const ResourceFlowRate& flowRate = flowRates_[index];
            ResourceFlowSnapshot& snapshot = lastFlows_[index];

            const double openingAmount =
                stockpile.amount(flowRate.resourceId);

            const double producedAmount = boundedProduct(
                flowRate.dailyProductionPerResident,
                residentCount,
                elapsedDays
            );

            const double requestedAmount = boundedProduct(
                flowRate.dailyConsumptionPerResident,
                residentCount,
                elapsedDays
            );

            const double availableAmount = boundedSum(
                openingAmount,
                producedAmount
            );

            const double consumedAmount =
                std::min(availableAmount, requestedAmount);

            const double closingAmount =
                std::max(0.0, availableAmount - consumedAmount);

            static_cast<void>(
                stockpile.setAmount(
                    flowRate.resourceId,
                    closingAmount
                )
            );

            const double fulfillment =
                requestedAmount > 0.0
                    ? consumedAmount / requestedAmount
                    : 1.0;

            const double sustainableSupplyRatio =
                flowRate.dailyConsumptionPerResident > 0.0
                    ? flowRate.dailyProductionPerResident /
                        flowRate.dailyConsumptionPerResident
                    : 1.0;

            snapshot.elapsedDays = elapsedDays;
            snapshot.openingAmount = openingAmount;
            snapshot.producedAmount = producedAmount;
            snapshot.requestedAmount = requestedAmount;
            snapshot.consumedAmount = consumedAmount;
            snapshot.closingAmount = closingAmount;
            snapshot.fulfillment =
                std::clamp(fulfillment, 0.0, 1.0);
            snapshot.sustainableSupplyRatio =
                std::max(0.0, sustainableSupplyRatio);

            if (flowRate.populationNeedWeight > 0.0)
            {
                totalNeedWeight += flowRate.populationNeedWeight;
                weightedFulfillment +=
                    flowRate.populationNeedWeight * snapshot.fulfillment;

                weightedSupplyRatio +=
                    flowRate.populationNeedWeight *
                    std::clamp(
                        snapshot.sustainableSupplyRatio,
                        0.0,
                        2.0
                    );
            }
        }

        if (totalNeedWeight > 0.0)
        {
            populationNeedFulfillment_ =
                weightedFulfillment / totalNeedWeight;

            populationSustainableSupplyRatio_ =
                weightedSupplyRatio / totalNeedWeight;
        }
        else
        {
            populationNeedFulfillment_ = 1.0;
            populationSustainableSupplyRatio_ = 1.0;
        }
    }

    std::span<const ResourceFlowRate>
    SettlementEconomy::flowRates() const noexcept
    {
        return flowRates_;
    }

    std::span<const ResourceFlowSnapshot>
    SettlementEconomy::lastFlows() const noexcept
    {
        return lastFlows_;
    }

    double SettlementEconomy::populationNeedFulfillment() const noexcept
    {
        return populationNeedFulfillment_;
    }

    double SettlementEconomy::populationSustainableSupplyRatio()
        const noexcept
    {
        return populationSustainableSupplyRatio_;
    }
}

#include "world/settlements/SettlementPopulation.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Paladin
{
    namespace
    {
        DemographicRates normalizedRates(
            DemographicRates rates
        ) noexcept
        {
            rates.annualBirthsPerPerson =
                std::isfinite(rates.annualBirthsPerPerson)
                    ? std::max(0.0, rates.annualBirthsPerPerson)
                    : 0.0;

            rates.annualDeathsPerPerson =
                std::isfinite(rates.annualDeathsPerPerson)
                    ? std::max(0.0, rates.annualDeathsPerPerson)
                    : 0.0;

            if (!std::isfinite(rates.annualNetMigration))
            {
                rates.annualNetMigration = 0.0;
            }

            return rates;
        }
    }

    SettlementPopulation::SettlementPopulation(
        std::uint64_t residents,
        DemographicRates rates
    ) noexcept
        : residents_(residents),
          rates_(normalizedRates(rates))
    {
    }

    std::uint64_t SettlementPopulation::residents() const noexcept
    {
        return residents_;
    }

    DemographicRates SettlementPopulation::rates() const noexcept
    {
        return rates_;
    }

    void SettlementPopulation::setRates(
        DemographicRates rates
    ) noexcept
    {
        rates_ = normalizedRates(rates);
    }

    void SettlementPopulation::applyNetChange(
        double populationChange
    ) noexcept
    {
        if (!std::isfinite(populationChange))
        {
            return;
        }

        fractionalChange_ += populationChange;

        if (fractionalChange_ >= 1.0)
        {
            constexpr std::uint64_t maximumResidents =
                std::numeric_limits<std::uint64_t>::max();

            const std::uint64_t availableGrowth =
                maximumResidents - residents_;

            const double requestedGrowth =
                std::floor(fractionalChange_);

            if (
                availableGrowth == 0 ||
                requestedGrowth >=
                    static_cast<double>(availableGrowth)
            )
            {
                residents_ = maximumResidents;
                fractionalChange_ = 0.0;
                return;
            }

            const std::uint64_t appliedGrowth =
                static_cast<std::uint64_t>(requestedGrowth);

            residents_ += appliedGrowth;
            fractionalChange_ -=
                static_cast<double>(appliedGrowth);

            return;
        }

        if (fractionalChange_ <= -1.0)
        {
            const double requestedDecline =
                std::floor(-fractionalChange_);

            if (
                requestedDecline >=
                    static_cast<double>(residents_)
            )
            {
                residents_ = 0;
                fractionalChange_ = 0.0;
                return;
            }

            const std::uint64_t appliedDecline =
                static_cast<std::uint64_t>(requestedDecline);

            residents_ -= appliedDecline;
            fractionalChange_ +=
                static_cast<double>(appliedDecline);
        }
    }
}

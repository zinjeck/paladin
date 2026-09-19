#pragma once

#include <cstdint>

namespace Paladin
{
    class SettlementPopulationSystem;

    struct DemographicRates
    {
        double annualBirthsPerPerson = 0.025;
        double annualDeathsPerPerson = 0.015;
        double annualNetMigration = 0.0;
        double annualDeathsAtZeroNeedFulfillmentPerPerson = 0.20;
        double maximumAnnualSurplusBirthsPerPerson = 0.005;

        bool operator==(const DemographicRates&) const = default;
    };

    class SettlementPopulation
    {
    public:
        SettlementPopulation() noexcept = default;

        SettlementPopulation(
            std::uint64_t residents,
            DemographicRates rates
        ) noexcept;

        [[nodiscard]]
        std::uint64_t residents() const noexcept;

        [[nodiscard]]
        DemographicRates rates() const noexcept;

        [[nodiscard]]
        std::uint64_t version() const noexcept;

        void setRates(DemographicRates rates) noexcept;

    private:
        friend class SettlementPopulationSystem;
        friend class SettlementSimulationState;
        friend class MilitarySystem;
        // Exact civil/military transfers preserve the demographic fraction.
        bool transferResidents(std::int64_t delta) noexcept;

        void applyNetChange(double populationChange) noexcept;

        std::uint64_t residents_ = 0;
        double fractionalChange_ = 0.0;
        DemographicRates rates_;
        std::uint64_t version_ = 0;
    };
} // namespace Paladin

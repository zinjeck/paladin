#pragma once
#include <cstddef>
#include <cstdint>

namespace Paladin
{
    class SettlementMap;
    class SettlementCitizenState;
    struct ImmigrationPolicy
    {
        double minimumHappiness = 50;
        double idealHappiness = 100;
        double minimumFoodDays = 1;
        double idealFoodDays = 3;
        double baseApplicantsPerDay = 2;
        double applicantsPerResidentPerDay = .04;
        double waitingPoolDays = 3;
        double applicantDepartureDays = 2;
        double assessmentMinutes = 60;
        double housingHappinessPenalty = 8;
        double foodHappinessPenalty = 8;
        double happinessAdjustmentPerDay = 3;
        bool isValid() const noexcept;
    };
    struct ImmigrationConditions
    {
        std::size_t residents = 0;
        double happiness = 100;
        double storedFood = 0;
        double foodPerDay = 0;
        double foodDays = 0;
        double unhousedShare = 0;
        double attraction = 0;
        double applicantsPerDay = 0;
    };
    // Manual admissions, simulation-time arrivals. Owned by the settlement,
    // never by a panel; inactive cities retain and update their waiting pool.
    class SettlementImmigration
    {
    public:
        ImmigrationPolicy policy;
        void assess(const SettlementMap&, const SettlementCitizenState&);
        void advance(
            const SettlementMap&,
            const SettlementCitizenState&,
            double minute,
            double elapsed
        );
        bool admit(
            SettlementMap&,
            SettlementCitizenState&,
            std::uint64_t count,
            double minute
        );
        std::uint64_t available() const noexcept;
        const ImmigrationConditions& conditions() const noexcept
        {
            return conditions_;
        }

    private:
        ImmigrationConditions conditions_;
        double applicants_ = 0;
        double untilAssessment_ = 0;
    };
} // namespace Paladin

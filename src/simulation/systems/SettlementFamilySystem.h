#pragma once
#include <cstddef>
#include <cstdint>
namespace Paladin
{
    class SettlementMap;
    class SettlementCitizenState;
    class SettlementActivitySystem;
    struct CitizenSimulationPolicy;
    // Owns lifecycle timing and household allocation. Activity cancellation is
    // delegated to the activity authority so reservations have one release
    // path.
    class SettlementFamilySystem
    {
    public:
        void update(
            SettlementMap&,
            SettlementCitizenState&,
            const CitizenSimulationPolicy&,
            SettlementActivitySystem&,
            double minute,
            double elapsed
        );
        void assignHomes(
            SettlementMap&,
            SettlementCitizenState&,
            SettlementActivitySystem&
        );
        std::size_t housingCapacity() const noexcept
        {
            return housingCapacity_;
        }

    private:
        double currentMinute_ = 0;
        double familyElapsed_ = 0;
        std::uint64_t housingFamilies_ = ~std::uint64_t(0);
        std::size_t housingCapacity_ = 0;
        std::uint64_t housingTopology_ = ~std::uint64_t(0);
        std::size_t housedPopulation_ = ~std::size_t(0);
    };
} // namespace Paladin

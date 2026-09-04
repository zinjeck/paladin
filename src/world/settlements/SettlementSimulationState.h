#pragma once

#include "world/settlements/ResourceStockpile.h"
#include "world/settlements/SettlementEconomy.h"
#include "world/settlements/SettlementFoundationProfile.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/SettlementPopulation.h"
#include "world/settlements/SettlementSimulationPolicy.h"
#include "world/settlements/SettlementSimulationTier.h"
#include "world/settlements/citizens/SettlementCitizenState.h"

#include <cstdint>
#include <memory>

namespace Paladin
{
    class WorldSimulationPipeline;

    enum class SettlementStateDomain : std::uint8_t
    {
        None = 0,
        Population = 1 << 0,
        Resources = 1 << 1,
        Economy = 1 << 2,
        Scheduling = 1 << 3,
        LocalMap = 1 << 4,
        Citizens = 1 << 5
    };

    [[nodiscard]]
    constexpr SettlementStateDomain operator|(
        SettlementStateDomain first,
        SettlementStateDomain second
    ) noexcept
    {
        return static_cast<SettlementStateDomain>(
            static_cast<std::uint8_t>(first) |
            static_cast<std::uint8_t>(second)
        );
    }

    struct SettlementStateVersions
    {
        std::uint64_t population = 0;
        std::uint64_t resources = 0;
        std::uint64_t economy = 0;
        std::uint64_t scheduling = 0;
        std::uint64_t localMap = 0;
        std::uint64_t citizens = 0;

        bool operator==(const SettlementStateVersions&) const = default;
    };

    struct SettlementStateChanges
    {
        SettlementStateDomain domains = SettlementStateDomain::None;
        SettlementStateVersions currentVersions;

        [[nodiscard]]
        bool has(SettlementStateDomain domain) const noexcept
        {
            return (
                static_cast<std::uint8_t>(domains) &
                static_cast<std::uint8_t>(domain)
            ) != 0;
        }

        [[nodiscard]]
        bool empty() const noexcept
        {
            return domains == SettlementStateDomain::None;
        }
    };

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

        [[nodiscard]]
        SettlementCitizenState& citizens() noexcept;

        [[nodiscard]]
        const SettlementCitizenState& citizens() const noexcept;

        [[nodiscard]]
        SettlementSimulationTier simulationTier() const noexcept;

        [[nodiscard]]
        std::uint64_t pendingSimulationMinutes() const noexcept;

        [[nodiscard]]
        std::uint64_t totalSimulatedMinutes() const noexcept;

        [[nodiscard]]
        std::uint64_t completedSimulationSteps() const noexcept;

        [[nodiscard]]
        SettlementStateVersions versions() const noexcept;

        [[nodiscard]]
        SettlementStateChanges changesSince(
            const SettlementStateVersions& previousVersions
        ) const noexcept;

        [[nodiscard]]
        bool hasLocalMap() const noexcept;

        [[nodiscard]]
        const SettlementMap* localMap() const noexcept;

    private:
        friend class WorldSimulationPipeline;
        friend class Simulation;

        void setSimulationTier(
            SettlementSimulationTier tier
        ) noexcept;

        [[nodiscard]]
        std::uint64_t takeDueSimulationMinutes(
            std::uint64_t gameMinutes,
            const SettlementSimulationPolicy& policy
        ) noexcept;

        [[nodiscard]]
        std::uint64_t takeAllPendingSimulationMinutes() noexcept;

        void setLocalMap(
            std::unique_ptr<SettlementMap> localMap
        ) noexcept;

        void clearLocalMap() noexcept;

        bool initialized_ = false;
        SettlementPopulation population_;
        ResourceStockpile stockpile_;
        SettlementEconomy economy_;
        SettlementCitizenState citizens_;
        SettlementSimulationTier simulationTier_ =
            SettlementSimulationTier::Inactive;
        std::uint64_t pendingSimulationMinutes_ = 0;
        std::uint64_t totalSimulatedMinutes_ = 0;
        std::uint64_t completedSimulationSteps_ = 0;
        std::uint64_t schedulingVersion_ = 0;
        std::unique_ptr<SettlementMap> localMap_;
        std::uint64_t localMapVersion_ = 0;
    };
}

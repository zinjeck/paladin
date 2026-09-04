#pragma once

#include "world/settlements/ResourceStockpile.h"
#include "world/settlements/SettlementEconomy.h"
#include "world/settlements/SettlementFoundationProfile.h"
#include "world/settlements/SettlementPopulation.h"
#include "world/settlements/SettlementSimulationPolicy.h"
#include "world/settlements/SettlementSimulationTier.h"

#include <cstdint>

namespace Paladin
{
    class WorldSimulationPipeline;

    struct SettlementStateVersions
    {
        std::uint64_t population = 0;
        std::uint64_t resources = 0;
        std::uint64_t economy = 0;
        std::uint64_t scheduling = 0;

        bool operator==(const SettlementStateVersions&) const = default;
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

        void setSimulationTier(
            SettlementSimulationTier tier
        ) noexcept;

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

    private:
        friend class WorldSimulationPipeline;

        [[nodiscard]]
        std::uint64_t takeDueSimulationMinutes(
            std::uint64_t gameMinutes,
            const SettlementSimulationPolicy& policy
        ) noexcept;

        bool initialized_ = false;
        SettlementPopulation population_;
        ResourceStockpile stockpile_;
        SettlementEconomy economy_;
        SettlementSimulationTier simulationTier_ =
            SettlementSimulationTier::Inactive;
        std::uint64_t pendingSimulationMinutes_ = 0;
        std::uint64_t totalSimulatedMinutes_ = 0;
        std::uint64_t completedSimulationSteps_ = 0;
        std::uint64_t schedulingVersion_ = 0;
    };
}

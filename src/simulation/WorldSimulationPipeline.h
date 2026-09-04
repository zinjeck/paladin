#pragma once
#include "debug/TimingSamples.h"

#include "simulation/WorldSimulationSystem.h"
#include "world/settlements/SettlementSimulationPolicy.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace Paladin
{
    class World;

    class WorldSimulationPipeline
    {
    public:
        explicit WorldSimulationPipeline(
            SettlementSimulationPolicies policies =
                defaultSettlementSimulationPolicies()
        );
        ~WorldSimulationPipeline();

        WorldSimulationPipeline(
            const WorldSimulationPipeline&
        ) = delete;

        WorldSimulationPipeline& operator=(
            const WorldSimulationPipeline&
        ) = delete;

        [[nodiscard]]
        bool addSystem(
            std::unique_ptr<WorldSimulationSystem> system
        );

        void tick(
            World& world,
            std::uint64_t gameMinutes
        );

        // Commits all time retained under the previous policy before changing
        // resolution. A city view can therefore never reinterpret inactive
        // or strategic time as detailed simulation.
        [[nodiscard]]
        bool transitionSettlementTier(
            World& world,
            SettlementId settlementId,
            SettlementSimulationTier targetTier
        );

        [[nodiscard]]
        std::size_t systemCount() const noexcept;
        std::vector<TimingSamples> systemTimings;

        [[nodiscard]]
        const SettlementSimulationPolicies& policies() const noexcept;

    private:
        void runSystems(
            World& world,
            std::uint64_t gameMinutes,
            std::span<const SettlementSimulationStep> settlementSteps
        );

        // Registration order is execution order. This makes cross-system
        // dependencies explicit and deterministic.
        std::vector<std::unique_ptr<WorldSimulationSystem>> systems_;
        std::vector<SettlementSimulationStep> settlementSteps_;
        SettlementSimulationPolicies policies_;
    };
}

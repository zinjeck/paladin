#pragma once

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

        [[nodiscard]]
        std::size_t systemCount() const noexcept;

        [[nodiscard]]
        const SettlementSimulationPolicies& policies() const noexcept;

    private:
        // Registration order is execution order. This makes cross-system
        // dependencies explicit and deterministic.
        std::vector<std::unique_ptr<WorldSimulationSystem>> systems_;
        std::vector<SettlementSimulationStep> settlementSteps_;
        SettlementSimulationPolicies policies_;
    };
}

#pragma once

#include <cstddef>
#include <memory>
#include <vector>

namespace Paladin
{
    class World;
    class WorldSimulationSystem;

    class WorldSimulationPipeline
    {
    public:
        WorldSimulationPipeline();
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
            double gameDeltaSeconds
        );

        [[nodiscard]]
        std::size_t systemCount() const noexcept;

    private:
        // Registration order is execution order. This makes cross-system
        // dependencies explicit and deterministic.
        std::vector<std::unique_ptr<WorldSimulationSystem>> systems_;
    };
}

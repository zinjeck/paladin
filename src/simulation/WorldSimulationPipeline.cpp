#include "simulation/WorldSimulationPipeline.h"

#include "simulation/WorldSimulationSystem.h"
#include "simulation/systems/SettlementPopulationSystem.h"

#include <cmath>
#include <memory>
#include <utility>

namespace Paladin
{
    WorldSimulationPipeline::WorldSimulationPipeline()
    {
        static_cast<void>(
            addSystem(
                std::make_unique<SettlementPopulationSystem>()
            )
        );
    }

    WorldSimulationPipeline::~WorldSimulationPipeline() = default;

    bool WorldSimulationPipeline::addSystem(
        std::unique_ptr<WorldSimulationSystem> system
    )
    {
        if (!system)
        {
            return false;
        }

        systems_.push_back(std::move(system));
        return true;
    }

    void WorldSimulationPipeline::tick(
        World& world,
        double gameDeltaSeconds
    )
    {
        if (
            !std::isfinite(gameDeltaSeconds) ||
            gameDeltaSeconds <= 0.0
        )
        {
            return;
        }

        for (const std::unique_ptr<WorldSimulationSystem>& system
            : systems_)
        {
            system->tick(world, gameDeltaSeconds);
        }
    }

    std::size_t WorldSimulationPipeline::systemCount() const noexcept
    {
        return systems_.size();
    }
}

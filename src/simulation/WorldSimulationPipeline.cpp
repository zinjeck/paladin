#include "simulation/WorldSimulationPipeline.h"

#include "simulation/systems/SettlementEconomySystem.h"
#include "simulation/systems/SettlementPopulationSystem.h"
#include "world/Settlement.h"
#include "world/World.h"

#include <cmath>
#include <memory>
#include <utility>

namespace Paladin
{
    WorldSimulationPipeline::WorldSimulationPipeline()
    {
        static_cast<void>(
            addSystem(
                std::make_unique<SettlementEconomySystem>()
            )
        );

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

        settlementSteps_.clear();
        settlementSteps_.reserve(world.settlementCount());

        for (Settlement& settlement : world.settlements())
        {
            SettlementSimulationState& state =
                settlement.simulationState();

            const double dueSeconds =
                state.takeDueSimulationSeconds(gameDeltaSeconds);

            if (dueSeconds <= 0.0)
            {
                continue;
            }

            settlementSteps_.push_back(
                {
                    settlement.id(),
                    state.simulationTier(),
                    dueSeconds
                }
            );
        }

        const WorldSimulationStep step{
            gameDeltaSeconds,
            settlementSteps_
        };

        for (const std::unique_ptr<WorldSimulationSystem>& system
            : systems_)
        {
            system->tick(world, step);
        }
    }

    std::size_t WorldSimulationPipeline::systemCount() const noexcept
    {
        return systems_.size();
    }
}

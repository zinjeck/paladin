#include "simulation/WorldSimulationPipeline.h"

#include "simulation/systems/SettlementEconomySystem.h"
#include "simulation/systems/SettlementPopulationSystem.h"
#include "world/Settlement.h"
#include "world/World.h"

#include <memory>
#include <stdexcept>
#include <utility>

namespace Paladin
{
    WorldSimulationPipeline::WorldSimulationPipeline(
        SettlementSimulationPolicies policies
    )
        : policies_(std::move(policies))
    {
        if (!policies_.isValid())
        {
            throw std::invalid_argument(
                "Settlement simulation policies must use positive cadences."
            );
        }

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
        std::uint64_t gameMinutes
    )
    {
        if (gameMinutes == 0)
        {
            return;
        }

        settlementSteps_.clear();
        settlementSteps_.reserve(world.settlementCount());

        for (Settlement& settlement : world.settlements())
        {
            SettlementSimulationState& state =
                settlement.simulationState();

            const SettlementSimulationTier tier =
                state.simulationTier();

            const std::uint64_t dueMinutes =
                state.takeDueSimulationMinutes(
                    gameMinutes,
                    policies_.forTier(tier)
                );

            if (dueMinutes == 0)
            {
                continue;
            }

            settlementSteps_.push_back(
                {
                    settlement.id(),
                    tier,
                    dueMinutes
                }
            );
        }

        const WorldSimulationStep step{
            gameMinutes,
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


    const SettlementSimulationPolicies&
    WorldSimulationPipeline::policies() const noexcept
    {
        return policies_;
    }
}

#include "simulation/WorldSimulationPipeline.h"

#include "simulation/systems/SettlementEconomySystem.h"
#include "simulation/systems/SettlementPopulationSystem.h"
#include "world/Settlement.h"
#include "world/World.h"

#include <array>
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
                "Settlement simulation policies must use valid resolutions and positive cadences."
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

            const SettlementSimulationPolicy& policy =
                policies_.forTier(tier);

            const std::uint64_t dueMinutes =
                state.takeDueSimulationMinutes(
                    gameMinutes,
                    policy
                );

            if (dueMinutes == 0)
            {
                continue;
            }

            settlementSteps_.push_back(
                {
                    settlement.id(),
                    tier,
                    policy.resolution,
                    dueMinutes
                }
            );
        }

        runSystems(world, gameMinutes, settlementSteps_);
    }


    bool WorldSimulationPipeline::transitionSettlementTier(
        World& world,
        SettlementId settlementId,
        SettlementSimulationTier targetTier
    )
    {
        if (!isSettlementSimulationTier(targetTier))
        {
            return false;
        }

        Settlement* settlement = world.settlement(settlementId);

        if (!settlement)
        {
            return false;
        }

        SettlementSimulationState& state =
            settlement->simulationState();

        if (!state.isInitialized())
        {
            return false;
        }

        const SettlementSimulationTier previousTier =
            state.simulationTier();

        if (previousTier == targetTier)
        {
            return true;
        }

        const std::uint64_t retainedMinutes =
            state.takeAllPendingSimulationMinutes();

        if (retainedMinutes > 0)
        {
            const SettlementSimulationPolicy& previousPolicy =
                policies_.forTier(previousTier);

            const std::array<SettlementSimulationStep, 1> transitionStep{
                SettlementSimulationStep{
                    settlementId,
                    previousTier,
                    previousPolicy.resolution,
                    retainedMinutes
                }
            };

            runSystems(world, retainedMinutes, transitionStep);
        }

        state.setSimulationTier(targetTier);
        return true;
    }


    void WorldSimulationPipeline::runSystems(
        World& world,
        std::uint64_t gameMinutes,
        std::span<const SettlementSimulationStep> settlementSteps
    )
    {
        const WorldSimulationStep step{
            gameMinutes,
            settlementSteps
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

#include "simulation/Simulation.h"

#include "simulation/WorldSimulationPipeline.h"

#include "world/World.h"

#include <memory>

namespace Paladin
{
    Simulation::Simulation()
        : world_(std::make_unique<World>()),
          worldSimulationPipeline_(
              std::make_unique<WorldSimulationPipeline>()
          )
    {
        playerPolityId_ = world_->createPolity();
    }


    Simulation::~Simulation() = default;


    void Simulation::tick(
        double realDeltaSeconds
    )
    {
        const double multiplier =
            speedMultiplier();

        if (multiplier <= 0.0)
        {
            return;
        }

        ++tickCount_;

        const double gameDeltaSeconds =
            realDeltaSeconds * multiplier;

        synchronizeSettlementSimulationTiers();
        world_->tick(gameDeltaSeconds);
        worldSimulationPipeline_->tick(
            *world_,
            gameDeltaSeconds
        );
    }


    void Simulation::setSpeed(
        SimulationSpeed speed
    ) noexcept
    {
        speed_ = speed;
    }


    SimulationSpeed Simulation::speed() const noexcept
    {
        return speed_;
    }


    bool Simulation::isPaused() const noexcept
    {
        return speed_ ==
            SimulationSpeed::Paused;
    }


    World& Simulation::world() noexcept
    {
        return *world_;
    }


    const World& Simulation::world() const noexcept
    {
        return *world_;
    }


    std::uint64_t Simulation::tickCount() const noexcept
    {
        return tickCount_;
    }


    PolityId Simulation::playerPolityId() const noexcept
    {
        return playerPolityId_;
    }


    SettlementId Simulation::activeSettlementId() const noexcept
    {
        return activeSettlementId_;
    }


    bool Simulation::setActiveSettlement(
        SettlementId settlementId
    ) noexcept
    {
        const Settlement* settlement =
            world_->settlement(settlementId);

        if (
            !settlement ||
            settlement->ownerPolityId() != playerPolityId_
        )
        {
            return false;
        }

        activeSettlementId_ = settlementId;
        synchronizeSettlementSimulationTiers();
        return true;
    }


    SettlementId Simulation::foundPlayerCapital(
        WorldPosition position,
        const FoundingIdentity& identity
    )
    {
        const SettlementId settlementId =
            world_->foundCapitalSettlement(
            position,
            playerPolityId_,
            identity
        );

        if (settlementId.isValid())
        {
            static_cast<void>(
                setActiveSettlement(settlementId)
            );
        }

        return settlementId;
    }


    void Simulation::synchronizeSettlementSimulationTiers() noexcept
    {
        for (Settlement& settlement : world_->settlements())
        {
            SettlementSimulationTier tier =
                SettlementSimulationTier::Strategic;

            if (settlement.id() == activeSettlementId_)
            {
                tier = SettlementSimulationTier::Detailed;
            }
            else if (
                settlement.ownerPolityId() == playerPolityId_
            )
            {
                tier = SettlementSimulationTier::Summary;
            }

            settlement.simulationState().setSimulationTier(tier);
        }
    }


    double Simulation::speedMultiplier() const noexcept
    {
        switch (speed_)
        {
            case SimulationSpeed::Paused:
                return 0.0;

            case SimulationSpeed::Normal:
                return 1.0;

            case SimulationSpeed::Fast:
                return 2.0;

            case SimulationSpeed::VeryFast:
                return 3.0;
        }

        return 1.0;
    }
}

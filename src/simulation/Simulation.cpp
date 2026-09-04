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


    SettlementId Simulation::foundPlayerCapital(
        WorldPosition position,
        const FoundingIdentity& identity
    )
    {
        return world_->foundCapitalSettlement(
            position,
            playerPolityId_,
            identity
        );
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

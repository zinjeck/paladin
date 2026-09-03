#include "simulation/Simulation.h"

#include "world/World.h"

#include <memory>

namespace Paladin
{
    Simulation::Simulation()
        : world_(std::make_unique<World>())
    {
    }

    Simulation::~Simulation() = default;

    void Simulation::tick(double deltaSeconds)
    {
        world_->tick(deltaSeconds);

        ++tickCount_;
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
}
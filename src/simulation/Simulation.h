#pragma once

#include <cstdint>
#include <memory>

namespace Paladin
{
    class World;

    class Simulation
    {
    public:
        Simulation();
        ~Simulation();

        Simulation(const Simulation&) = delete;
        Simulation& operator=(const Simulation&) = delete;

        void tick(double deltaSeconds);

        World& world() noexcept;
        const World& world() const noexcept;

        std::uint64_t tickCount() const noexcept;

    private:
        std::unique_ptr<World> world_;
        std::uint64_t tickCount_ = 0;
    };
}
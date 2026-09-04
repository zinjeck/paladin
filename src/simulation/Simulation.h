#pragma once

#include "core/StrongId.h"
#include "world/FoundingIdentity.h"
#include "world/WorldPosition.h"

#include <cstdint>
#include <memory>

namespace Paladin
{
    class World;

    enum class SimulationSpeed
    {
        Paused,
        Normal,
        Fast,
        VeryFast
    };


    class Simulation
    {
    public:
        Simulation();
        ~Simulation();

        Simulation(const Simulation&) = delete;
        Simulation& operator=(const Simulation&) = delete;

        void tick(double realDeltaSeconds);

        void setSpeed(
            SimulationSpeed speed
        ) noexcept;

        [[nodiscard]]
        SimulationSpeed speed() const noexcept;

        [[nodiscard]]
        bool isPaused() const noexcept;

        [[nodiscard]]
        World& world() noexcept;

        [[nodiscard]]
        const World& world() const noexcept;

        [[nodiscard]]
        std::uint64_t tickCount() const noexcept;

        [[nodiscard]]
        PolityId playerPolityId() const noexcept;

        [[nodiscard]]
        SettlementId foundPlayerCapital(
            WorldPosition position,
            const FoundingIdentity& identity
        );

    private:
        [[nodiscard]]
        double speedMultiplier() const noexcept;

        std::unique_ptr<World> world_;

        PolityId playerPolityId_;

        SimulationSpeed speed_ =
            SimulationSpeed::Normal;

        std::uint64_t tickCount_ = 0;
    };
}

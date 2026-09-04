#pragma once

#include "core/StrongId.h"
#include "world/FoundingIdentity.h"
#include "world/WorldPosition.h"

#include <cstdint>
#include <memory>

namespace Paladin
{
    class World;
    class WorldSimulationPipeline;

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
        SettlementId activeSettlementId() const noexcept;

        [[nodiscard]]
        bool setActiveSettlement(
            SettlementId settlementId
        ) noexcept;

        [[nodiscard]]
        SettlementId foundPlayerCapital(
            WorldPosition position,
            const FoundingIdentity& identity
        );

    private:
        [[nodiscard]]
        double speedMultiplier() const noexcept;

        void synchronizeSettlementSimulationTiers() noexcept;

        std::unique_ptr<World> world_;
        std::unique_ptr<WorldSimulationPipeline>
            worldSimulationPipeline_;

        PolityId playerPolityId_;
        SettlementId activeSettlementId_;

        SimulationSpeed speed_ =
            SimulationSpeed::Normal;

        std::uint64_t tickCount_ = 0;
    };
}

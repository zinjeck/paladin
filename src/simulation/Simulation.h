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
    struct WorldGenerationSettings;

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
        explicit Simulation(
            const WorldGenerationSettings& generationSettings
        );
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
        SettlementId presentedSettlementId() const noexcept;

        [[nodiscard]]
        SettlementId detailedSimulationSettlementId() const noexcept;

        [[nodiscard]]
        bool setPresentedSettlement(
            SettlementId settlementId
        ) noexcept;

        [[nodiscard]]
        bool setDetailedSimulationSettlement(
            SettlementId settlementId
        ) noexcept;

        void clearDetailedSimulationSettlement() noexcept;

        [[nodiscard]]
        SettlementId foundPlayerCapital(
            WorldPosition position,
            const FoundingIdentity& identity
        );

        [[nodiscard]]
        bool renamePlayerCapital(std::string name);

        [[nodiscard]]
        bool editPlayerPolity(const FoundingIdentity& identity);

        [[nodiscard]]
        bool movePlayerCapital(WorldPosition position);

    private:
        [[nodiscard]]
        double speedMultiplier() const noexcept;

        void synchronizeSettlementSimulationTiers() noexcept;

        std::unique_ptr<World> world_;
        std::unique_ptr<WorldSimulationPipeline>
            worldSimulationPipeline_;

        PolityId playerPolityId_;
        SettlementId presentedSettlementId_;
        SettlementId detailedSimulationSettlementId_;

        SimulationSpeed speed_ =
            SimulationSpeed::Normal;

        std::uint64_t tickCount_ = 0;
        double pendingGameMinutes_ = 0.0;
    };
}

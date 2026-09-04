#pragma once
#include "debug/TimingSamples.h"

#include "core/StrongId.h"
#include "world/FoundingIdentity.h"
#include "world/WorldTilePosition.h"
#include "world/generation/SettlementMapGenerator.h"

#include <cstdint>
#include <memory>

namespace Paladin
{
    class World;
    class WorldSimulationPipeline;
    class SettlementMap;
    struct WorldGenerationSettings;

    struct SimulationTimingSettings
    {
        std::uint64_t gameMinutesPerStep = 2;
        double realSecondsPerStep = 5.0 / 6.0;
    };

    [[nodiscard]]
    constexpr SimulationTimingSettings
    defaultSimulationTimingSettings() noexcept
    {
        return {};
    }

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
            const WorldGenerationSettings& generationSettings,
            SimulationTimingSettings timingSettings =
                defaultSimulationTimingSettings()
        );
        ~Simulation();

        Simulation(const Simulation&) = delete;
        Simulation& operator=(const Simulation&) = delete;

        void tick(double realDeltaSeconds);
        TimingSamples tickTiming, citizenTiming, aggregateTiming;
        std::string systemTimingText() const;
        double gameMinutesPerTick(double seconds) const noexcept
        {
            return seconds * timingSettings_.gameMinutesPerStep /
                   timingSettings_.realSecondsPerStep;
        }

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
        );

        [[nodiscard]]
        bool clearDetailedSimulationSettlement();

        [[nodiscard]]
        bool prepareSettlementMap(
            SettlementId settlementId,
            const SettlementMapGenerationSettings& settings =
                defaultSettlementMapGenerationSettings()
        );

        [[nodiscard]]
        SettlementMap* settlementMap(
            SettlementId settlementId
        ) noexcept;

        [[nodiscard]]
        const SettlementMap* settlementMap(
            SettlementId settlementId
        ) const noexcept;

        [[nodiscard]]
        SettlementId foundPlayerCapital(
            WorldTilePosition position,
            const FoundingIdentity& identity
        );

        [[nodiscard]]
        bool renamePlayerCapital(std::string name);

        [[nodiscard]]
        bool editPlayerPolity(const FoundingIdentity& identity);

        [[nodiscard]]
        bool movePlayerCapital(WorldTilePosition position);

    private:
        [[nodiscard]]
        double speedMultiplier() const noexcept;

        [[nodiscard]]
        bool synchronizeSettlementSimulationTiers();

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
        SimulationTimingSettings timingSettings_ =
            defaultSimulationTimingSettings();
        SettlementMapGenerator settlementMapGenerator_;
    };
}

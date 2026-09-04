#pragma once

#include "core/StrongId.h"
#include "world/settlements/SettlementSimulationTier.h"

#include <cstdint>
#include <span>

namespace Paladin
{
    class World;

    struct SettlementSimulationStep
    {
        SettlementId settlementId;
        SettlementSimulationTier simulationTier =
            SettlementSimulationTier::Inactive;
        std::uint64_t gameMinutes = 0;
    };

    struct WorldSimulationStep
    {
        std::uint64_t gameMinutes = 0;
        std::span<const SettlementSimulationStep> settlementSteps;
    };

    class WorldSimulationSystem
    {
    public:
        virtual ~WorldSimulationSystem() = default;

        virtual void tick(
            World& world,
            const WorldSimulationStep& step
        ) = 0;
    };
}

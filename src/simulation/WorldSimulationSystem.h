#pragma once

#include "core/StrongId.h"
#include "world/settlements/SettlementSimulationTier.h"

#include <span>

namespace Paladin
{
    class World;

    struct SettlementSimulationStep
    {
        SettlementId settlementId;
        SettlementSimulationTier simulationTier =
            SettlementSimulationTier::Summary;
        double gameDeltaSeconds = 0.0;
    };

    struct WorldSimulationStep
    {
        double gameDeltaSeconds = 0.0;
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

#pragma once

#include "simulation/WorldSimulationSystem.h"

namespace Paladin
{
    class SettlementPopulationSystem final
        : public WorldSimulationSystem
    {
    public:
        void tick(
            World& world,
            double gameDeltaSeconds
        ) override;
    };
}

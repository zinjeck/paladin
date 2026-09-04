#pragma once

#include "simulation/WorldSimulationSystem.h"

namespace Paladin
{
    class SettlementEconomySystem final
        : public WorldSimulationSystem
    {
    public:
        void tick(
            World& world,
            const WorldSimulationStep& step
        ) override;
    };
}

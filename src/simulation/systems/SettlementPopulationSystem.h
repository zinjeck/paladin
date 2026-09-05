#pragma once

#include "simulation/WorldSimulationSystem.h"

namespace Paladin
{
    class SettlementPopulationSystem final : public WorldSimulationSystem
    {
    public:
        void tick(World& world, const WorldSimulationStep& step) override;
    };
} // namespace Paladin

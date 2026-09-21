#pragma once

#include "simulation/WorldSimulationSystem.h"
#include <cstddef>

namespace Paladin
{
    class SettlementEconomySystem final : public WorldSimulationSystem
    {
    public:
        void tick(World& world, const WorldSimulationStep& step) override;

    private:
        std::size_t forecastCursor_ = 0;
    };
} // namespace Paladin

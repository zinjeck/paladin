#pragma once

#include "Pr32ContinuationChecks.h"
#include "simulation/WorldSimulationPipeline.h"
#include <array>

namespace Paladin::Test::Pr32
{
    inline void hourlyEconomyCadence()
    {
        Fixture f;
        WorldSimulationPipeline pipeline;
        // Exercise the real scheduling/forecast/consumption pipeline in both
        // aggregate tiers, not just the configured minimum-step constants.
        PALADIN_CHECK(pipeline.transitionSettlementTier(
            f.world, f.destination, SettlementSimulationTier::Strategic));
        f.world.realm(f.buyer)->nextMarketMinute = 100000;
        for (const auto id : {f.destination, f.source})
        {
            auto* city = f.world.settlement(id);
            f.world.grid().tile(city->position())->mineral = MineralDeposit::None;
            PALADIN_CHECK(city->simulationState().stockpile().setAmount("iron", 100));
        }
        f.world.grid().terrainChanged();
        const auto advance = [&](std::uint64_t minutes)
        {
            f.world.advanceTime(minutes);
            pipeline.tick(f.world, minutes);
        };
        advance(59);
        for (const auto id : {f.destination, f.source})
        {
            const auto& state = f.world.settlement(id)->simulationState();
            PALADIN_CHECK(state.totalSimulatedMinutes() == 0);
            PALADIN_CHECK(state.stockpile().amount("iron") == 100);
        }
        advance(1);
        std::array<double, 2> afterFirstHour{};
        std::size_t i = 0;
        for (const auto id : {f.destination, f.source})
        {
            const auto& city = *f.world.settlement(id);
            const auto& state = city.simulationState();
            PALADIN_CHECK(state.totalSimulatedMinutes() == 60);
            PALADIN_CHECK(state.completedSimulationSteps() == 1);
            PALADIN_CHECK(WorldMarketSystem::quote(f.world, city, "iron").dailyNeed > 0);
            afterFirstHour[i] = state.stockpile().amount("iron");
            PALADIN_CHECK(afterFirstHour[i] > 0 && afterFirstHour[i] < 100);
            ++i;
        }
        advance(0); // A paused pipeline must not consume or complete a step.
        advance(59);
        i = 0;
        for (const auto id : {f.destination, f.source})
        {
            const auto& state = f.world.settlement(id)->simulationState();
            PALADIN_CHECK(state.totalSimulatedMinutes() == 60);
            PALADIN_CHECK(state.completedSimulationSteps() == 1);
            PALADIN_CHECK(state.stockpile().amount("iron") == afterFirstHour[i++]);
        }
        advance(1);
        i = 0;
        for (const auto id : {f.destination, f.source})
        {
            const auto& state = f.world.settlement(id)->simulationState();
            PALADIN_CHECK(state.totalSimulatedMinutes() == 120);
            PALADIN_CHECK(state.completedSimulationSteps() == 2);
            PALADIN_CHECK(state.stockpile().amount("iron") < afterFirstHour[i++]);
        }
        PALADIN_CHECK(f.world.shipments().empty());
    }
}

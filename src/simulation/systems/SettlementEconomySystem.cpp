#include "simulation/systems/SettlementEconomySystem.h"

#include "world/Settlement.h"
#include "world/World.h"

namespace Paladin
{
    void SettlementEconomySystem::tick(
        World& world,
        const WorldSimulationStep& step
    )
    {
        constexpr double gameMinutesPerDay =
            24.0 * 60.0;

        for (
            const SettlementSimulationStep& settlementStep
            : step.settlementSteps
        )
        {
            Settlement* settlement =
                world.settlement(settlementStep.settlementId);

            if (!settlement)
            {
                continue;
            }

            SettlementSimulationState& state =
                settlement->simulationState();

            if (state.hasLocalMap())
            {
                continue;
            }

            state.economy().simulate(
                state.stockpile(),
                state.population().residents(),
                static_cast<double>(settlementStep.gameMinutes) /
                    gameMinutesPerDay
            );
        }
    }
}

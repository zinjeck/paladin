#include "simulation/systems/SettlementPopulationSystem.h"

#include "world/Settlement.h"
#include "world/World.h"

#include <algorithm>

namespace Paladin
{
    void SettlementPopulationSystem::tick(
        World& world,
        const WorldSimulationStep& step
    )
    {
        constexpr double gameSecondsPerYear =
            365.0 * 24.0 * 60.0 * 60.0;

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

            if (!state.isInitialized())
            {
                continue;
            }

            SettlementPopulation& population =
                state.population();

            const DemographicRates rates = population.rates();

            const double needFulfillment =
                state.economy().populationNeedFulfillment();

            const double sustainableSupplyRatio =
                state.economy()
                    .populationSustainableSupplyRatio();

            const double shortageDeathsPerPerson =
                (1.0 - needFulfillment) *
                rates
                    .annualDeathsAtZeroNeedFulfillmentPerPerson;

            const double surplusBirthsPerPerson =
                std::clamp(
                    sustainableSupplyRatio - 1.0,
                    0.0,
                    1.0
                ) * rates.maximumAnnualSurplusBirthsPerPerson;

            const double annualNaturalChange =
                static_cast<double>(population.residents())
                * (
                    rates.annualBirthsPerPerson
                    + surplusBirthsPerPerson
                    - rates.annualDeathsPerPerson
                    - shortageDeathsPerPerson
                );

            const double elapsedYears =
                settlementStep.gameDeltaSeconds /
                gameSecondsPerYear;

            const double projectedChange =
                (
                    annualNaturalChange
                    + rates.annualNetMigration
                )
                * elapsedYears;

            population.applyNetChange(projectedChange);
        }
    }
}

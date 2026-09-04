#include "simulation/systems/SettlementPopulationSystem.h"

#include "world/Settlement.h"
#include "world/World.h"

namespace Paladin
{
    void SettlementPopulationSystem::tick(
        World& world,
        double gameDeltaSeconds
    )
    {
        constexpr double gameSecondsPerYear =
            365.0 * 24.0 * 60.0 * 60.0;

        const double elapsedYears =
            gameDeltaSeconds / gameSecondsPerYear;

        for (Settlement& settlement : world.settlements())
        {
            SettlementSimulationState& state =
                settlement.simulationState();

            if (!state.isActive())
            {
                continue;
            }

            SettlementPopulation& population =
                state.population();

            const DemographicRates rates = population.rates();

            const double annualNaturalChange =
                static_cast<double>(population.residents())
                * (
                    rates.annualBirthsPerPerson
                    - rates.annualDeathsPerPerson
                );

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

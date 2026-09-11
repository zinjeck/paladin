#include <exception>
#include <iostream>

#include "CelestialSunChecks.h"
#include "CityResourceFlowChecks.h"
#include "PastureDormancyRegression.h"
#include "Pr16Regression.h"
#include "TribalInfluenceChecks.h"

void runEntityAttributeTests();
void runWorldReliefTests();
void runSettlementSimulationLoopTests();
void runSettlementEmploymentTests();
void runSettlementActivityTests();
void runCoreTests();
void runWorldTests();
void runWorldGenerationTests();

int main()
{
    try
    {
        Paladin::Test::runCityResourceFlowChecks();
        runEntityAttributeTests();
        runWorldReliefTests();
        runSettlementSimulationLoopTests();
        runSettlementEmploymentTests();
        runSettlementActivityTests();
        runPastureDormancyRegression();
        runPr16Regression();
        runCoreTests();
        runWorldTests();
        runWorldGenerationTests();
        Paladin::Test::runCelestialSunChecks();
        Paladin::Test::runTribalInfluenceTests();
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Paladin test failure: " << exception.what() << '\n';

        return 1;
    }

    std::cout << "All Paladin tests passed.\n";

    return 0;
}

#include <exception>
#include <iostream>

#include "PastureDormancyRegression.h"
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
        runEntityAttributeTests();
        runWorldReliefTests();
        runSettlementSimulationLoopTests();
        runSettlementEmploymentTests();
        runSettlementActivityTests();
        runPastureDormancyRegression();
        runCoreTests();
        runWorldTests();
        runWorldGenerationTests();
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
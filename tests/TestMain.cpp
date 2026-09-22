#include <exception>
#include <iostream>

#include "CelestialSunChecks.h"
#include "CityResourceFlowChecks.h"
#include "CityCorrectionsChecks.h"
#include "TradeDepotRegressionChecks.h"
#include "CloseWorldStabilityChecks.h"
#include "PastureDormancyRegression.h"
#include "Pr31AnimalNavigationChecks.h"
#include "Pr31BattleChecks.h"
#include "Pr32ContinuationChecks.h"
#include "Pr32EconomyCadenceChecks.h"
#include "Pr16Regression.h"
#include "TribalInfluenceChecks.h"

void runMilitaryIndustryTests();
void runPr30SocietyTests();
void runPr30ShipmentTests();
void runPr30StrategyTests();
void runDiplomacyTests();
void runRealmFeatureTests();
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
        Paladin::Test::DepotRegression::run();
        Paladin::Test::CityCorrections::run();
        Paladin::Test::Pr32::run();
        Paladin::Test::Pr32::hourlyEconomyCadence();
        runPr31BattleChecks();
        runPr31AnimalNavigationChecks();
        runPr30ShipmentTests();
        runPr30StrategyTests();
        runPr30SocietyTests();
        runDiplomacyTests();
        runMilitaryIndustryTests();
        runRealmFeatureTests();
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
        Paladin::Test::runCloseWorldStabilityChecks();
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

#pragma once
#include "TestFramework.h"
#include "simulation/BattleSystem.h"
#include "simulation/DiplomacySystem.h"
#include "simulation/WorldLandNavigation.h"
#include "world/World.h"
#include <iostream>

inline void runPr31BattleChecks()
{
    using namespace Paladin;
    WorldGenerationSettings options;options.width=64;options.height=64;options.seed=31910;options.populateAiRealms=false;
    World world(options);
    for(int y=0;y<64;++y) for(int x=0;x<64;++x) world.grid().tile({x,y})->terrain=TerrainType::Land;
    const auto a=world.createRealm(),b=world.createRealm();
    world.realm(a)->aiControlled=world.realm(b)->aiControlled=true;
    auto profile=defaultSettlementFoundationProfile();profile.initialPopulation=100;profile.initialDetailedCitizenCount=0;profile.initialResources={{"bread",1000}};
    const auto home=world.foundCapitalSettlement({16,24},a,{"West","West Folk","West",{},"civic"},profile);
    const auto foe=world.foundCapitalSettlement({32,24},b,{"East","East Folk","East",{},"civic"},profile);
    PALADIN_CHECK(home && foe);
    const auto first=MilitarySystem::maintainStrategicGarrison(world,a,home,8),second=MilitarySystem::maintainStrategicGarrison(world,b,foe,4);
    PALADIN_CHECK(first && second);
    PALADIN_CHECK(
        world.army(first)->garrisoned() && world.army(second)->garrisoned()
    );
    PALADIN_CHECK(
        BattleSystem::orderAttack(world, a, first, second) ==
        MilitaryResult::InvalidUnit
    );
    PALADIN_CHECK(
        MilitarySystem::setGarrison(world, a, first, {}) ==
        MilitaryResult::Success
    );
    PALADIN_CHECK(
        MilitarySystem::setGarrison(world, b, second, {}) ==
        MilitaryResult::Success
    );
    PALADIN_CHECK(BattleSystem::orderAttack(world,b,first,second)==MilitaryResult::NotOwned);
    PALADIN_CHECK(BattleSystem::orderAttack(world,a,first,first)==MilitaryResult::InvalidUnit);
    PALADIN_CHECK(!world.diplomacy().between(a,b));
    // An enclosed target cannot be attacked through mountain/water barriers.
    for(const auto p:{WorldTilePosition{31,24},{33,24},{32,23},{32,25}}) world.grid().tile(p)->terrain=TerrainType::Mountain;
    PALADIN_CHECK(BattleSystem::orderAttack(world,a,first,second)==MilitaryResult::NoLandRoute);
    PALADIN_CHECK(!world.diplomacy().between(a,b));
    for(const auto p:{WorldTilePosition{31,24},{33,24},{32,23},{32,25}}) world.grid().tile(p)->terrain=TerrainType::Land;
    PALADIN_CHECK(BattleSystem::orderAttack(world,a,first,second)==MilitaryResult::Success);
    PALADIN_CHECK(world.diplomacy().between(a,b)->atWar);
    MilitarySystem::tick(world,360,2);
    const double x=world.army(first)->visualX();
    PALADIN_CHECK(x>16 && x<17);
    PALADIN_CHECK(MilitarySystem::orderMove(world,b,second,{30,26})==MilitaryResult::Success);
    for(int i=0;i<160 && !BattleSystem::pendingFor(world,a);++i) MilitarySystem::tick(world,362+i,1);
    auto battle=BattleSystem::pendingFor(world,a);
    PALADIN_CHECK(battle && BattleSystem::valid(world,*battle));
    PALADIN_CHECK(world.army(first)->position()==world.army(second)->position());
    PALADIN_CHECK(!world.army(first)->moving() && !world.army(second)->moving());
    PALADIN_CHECK(MilitarySystem::orderMove(world,a,first,{10,10})==MilitaryResult::InBattle);
    PALADIN_CHECK(MilitarySystem::disbandUnit(world,a,first)==MilitaryResult::InBattle);
    PALADIN_CHECK(!BattleSystem::retreat(world,b,*battle));
    PALADIN_CHECK(BattleSystem::retreat(world,a,*battle));
    PALADIN_CHECK(!BattleSystem::pendingFor(world,a) && !world.army(first)->engagedOpponent());
    PALADIN_CHECK(world.army(first)->soldierCount()==8 && world.army(second)->soldierCount()==4);
    PALADIN_CHECK(BattleSystem::orderAttack(world,a,first,second)==MilitaryResult::Success);
    battle=BattleSystem::pendingFor(world,a);PALADIN_CHECK(battle);
    const auto population=world.settlement(home)->population()+world.settlement(foe)->population();
    const auto records=world.settlement(home)->simulationState().citizens().citizens().size()+world.settlement(foe)->simulationState().citizens().citizens().size();
    const auto result=BattleSystem::simulate(world,a,*battle);
    PALADIN_CHECK(result.resolved && result.winner==first && result.enemyLosses==4 && result.playerLosses>0);
    PALADIN_CHECK(world.army(first)->soldierCount()==8-result.playerLosses);
    PALADIN_CHECK(!world.army(second) || world.army(second)->soldierCount()==0);
    PALADIN_CHECK(world.soldiers().size()==12-result.playerLosses-result.enemyLosses);
    const auto after=world.settlement(home)->simulationState().citizens().citizens().size()+world.settlement(foe)->simulationState().citizens().citizens().size();
    PALADIN_CHECK(records-after==result.playerLosses+result.enemyLosses);
    PALADIN_CHECK(world.settlement(home)->population()+world.settlement(foe)->population()==population); // deployed people already excluded
    PALADIN_CHECK(world.settlement(foe)->simulationState().citizens().ancestors().size()>=4);
    PALADIN_CHECK(!BattleSystem::simulate(world,a,*battle).resolved); // no double casualties
    PALADIN_CHECK(!BattleSystem::pendingFor(world,a));
    std::cout<<"[pr31/battle] ownership, blocked targets, hostility, moving pursuit, contact stop, retreat, canonical casualties/ancestry and no double resolution passed\n";
}

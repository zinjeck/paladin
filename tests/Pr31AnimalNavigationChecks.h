#pragma once
#include "TestFramework.h"
#include "simulation/WorldLandNavigation.h"
#include "simulation/MilitarySystem.h"
#include "world/World.h"
#include "world/settlements/SettlementMap.h"
#include <cmath>
#include <iostream>

inline void runPr31AnimalNavigationChecks()
{
    using namespace Paladin;
    WorldGrid grid(16, 12);
    for (int y=0;y<12;++y) for (int x=0;x<16;++x) grid.tile({x,y})->terrain=TerrainType::Land;
    auto route=worldLandRoute(grid,{3,3},{6,6},WorldLandMovement::EightWay);
    PALADIN_CHECK(route && route->size()==4);
    grid.tile({4,3})->terrain=TerrainType::Water;
    grid.tile({3,4})->terrain=TerrainType::Mountain;
    PALADIN_CHECK(!worldLandStepAllowed(grid,{3,3},{4,4}));
    route=worldLandRoute(grid,{3,3},{6,6},WorldLandMovement::EightWay);
    PALADIN_CHECK(route);
    for (std::size_t i=1;i<route->size();++i)
        PALADIN_CHECK(worldLandStepAllowed(grid,(*route)[i-1],(*route)[i]));
    PALADIN_CHECK(worldLandStepAllowed(grid,{15,5},{0,6}));
    grid.tile({0,5})->terrain=TerrainType::Mountain;
    PALADIN_CHECK(!worldLandStepAllowed(grid,{15,5},{0,6}));
    PALADIN_CHECK(std::abs(worldLandStepDistance({15,5},{0,6},16)-std::sqrt(2.))<1e-9);
    PALADIN_CHECK(!worldLandStepAllowed(grid,{3,3},{3,-1}));
    PALADIN_CHECK(!worldLandRoute(grid,{3,3},{4,3},WorldLandMovement::EightWay));

    WorldGenerationSettings settings; settings.width=64; settings.height=32; settings.seed=3101; settings.populateAiRealms=false;
    World world(settings);
    for(int y=0;y<32;++y) for(int x=0;x<64;++x) world.grid().tile({x,y})->terrain=TerrainType::Land;
    const auto realm=world.createRealm(); world.realm(realm)->aiControlled=true;
    auto profile=defaultSettlementFoundationProfile(); profile.initialPopulation=100; profile.initialDetailedCitizenCount=0;
    profile.initialResources={{"food",1000}};
    const auto home=world.foundCapitalSettlement({16,16},realm,{"March","Marchers","March City",{},"civic"},profile);
    PALADIN_CHECK(home);
    const auto army=MilitarySystem::maintainStrategicGarrison(world,realm,home,2);
    PALADIN_CHECK(army);
    PALADIN_CHECK(MilitarySystem::orderMove(world,realm,army,{18,18})==MilitaryResult::Success);
    MilitarySystem::tick(world,360,Army::MarchMinutesPerTile*std::sqrt(2.)*.5);
    PALADIN_CHECK(std::abs(world.army(army)->visualX()-16.5)<1e-6);
    PALADIN_CHECK(std::abs(world.army(army)->visualY()-16.5)<1e-6);
    MilitarySystem::tick(world,364,Army::MarchMinutesPerTile*std::sqrt(2.)*.5);
    PALADIN_CHECK((world.army(army)->position()==WorldTilePosition{17,17}));
    world.grid().tile({18,17})->terrain=TerrainType::Water;
    MilitarySystem::tick(world,368,.1);
    PALADIN_CHECK(!world.army(army)->moving());
    PALADIN_CHECK((world.army(army)->position()==WorldTilePosition{17,17}));

    SettlementGrid local(32,32);
    for(int y=0;y<32;++y) for(int x=0;x<32;++x)
    { auto& t=*local.tile({x,y}); t.terrain=TerrainType::Land; t.biome=BiomeType::Plain; t.temperature=Temperature{.5}; }
    SettlementMap map(std::move(local),{0,0},1,1,32,3101);
    const auto place=[&](std::string_view type,SettlementObjectFootprint f)
    {
        auto def=*SettlementObjectCatalog::definition(type); def.bypassesConstruction=true;
        PALADIN_CHECK(map.objectState().placeCompletedObject(map.grid(),def,f));
        return map.objectState().completedObjects().back().id;
    };
    place("city_keep",{{2,2},5,7}); const auto pasture=place("pastureland",{{20,20},6,6});
    map.logistics.synchronize(map.objectState(),600);
    SettlementCitizenState people; PALADIN_CHECK(people.initialize(1,3101)); people.placeUnpositionedCitizens(map);
    auto& worker=const_cast<SettlementCitizen&>(people.citizens().front());
    worker.child=false; worker.ageYears=25; worker.youngDependents=0; worker.hunger=0; worker.energy=100;
    worker.tilePosition={21,21}; worker.destination=worker.tilePosition; worker.path.clear(); worker.task={};
    map.employment().synchronize(map.objectState(),people);
    PALADIN_CHECK(map.employment().adjust(map.employment().forObject(pasture),1,people));
    const auto resident=map.animals.spawn(map,"cow",{23,23});
    PALADIN_CHECK(resident); map.animals.find(resident)->pasture=pasture;
    const auto wild=map.animals.spawn(map,"cow",{16,21});
    PALADIN_CHECK(wild); map.animals.find(wild)->nextWanderMinute=10000;
    PALADIN_CHECK(map.animals.designate({{16,21},1,1},AnimalOrder::Gather)==1);
    bool gathered=false;
    for(int i=0;i<150 && !gathered;++i)
    {
        map.activities.tick(map,people,600+i,1);
        gathered=map.animals.find(wild)->pasture==pasture;
    }
    PALADIN_CHECK(gathered);
    PALADIN_CHECK(!map.animals.hasOrders());
    PALADIN_CHECK(map.animals.containedCount(pasture)==2);
    PALADIN_CHECK(worker.workplaceId==map.employment().forObject(pasture));
    std::cout<<"[pr31] diagonal army speed/interpolation, blocked corners/seam and occupied-pasture gathering passed\n";
}

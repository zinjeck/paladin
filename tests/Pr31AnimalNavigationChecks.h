#pragma once
#include "TestFramework.h"
#include "interaction/SettlementCommandController.h"
#include "simulation/MilitarySystem.h"
#include "simulation/Simulation.h"
#include "simulation/WorldLandNavigation.h"
#include "world/World.h"
#include "world/settlements/SettlementMap.h"
#include <cmath>
#include <iostream>

inline void runPr31AnimalNavigationChecks()
{
    using namespace Paladin;
    WorldGrid grid(16, 12);
    for (int y = 0; y < 12; ++y)
    {
        for (int x = 0; x < 16; ++x)
        {
            grid.tile({x, y})->terrain = TerrainType::Land;
        }
    }
    auto route =
        worldLandRoute(grid, {3, 3}, {6, 6}, WorldLandMovement::EightWay);
    PALADIN_CHECK(route && route->size() == 4);
    grid.tile({4, 3})->terrain = TerrainType::Water;
    grid.tile({3, 4})->terrain = TerrainType::Mountain;
    PALADIN_CHECK(!worldLandStepAllowed(grid, {3, 3}, {4, 4}));
    route = worldLandRoute(grid, {3, 3}, {6, 6}, WorldLandMovement::EightWay);
    PALADIN_CHECK(route);
    for (std::size_t i = 1; i < route->size(); ++i)
    {
        PALADIN_CHECK(worldLandStepAllowed(grid, (*route)[i - 1], (*route)[i]));
    }
    PALADIN_CHECK(worldLandStepAllowed(grid, {15, 5}, {0, 6}));
    grid.tile({0, 5})->terrain = TerrainType::Mountain;
    PALADIN_CHECK(!worldLandStepAllowed(grid, {15, 5}, {0, 6}));
    PALADIN_CHECK(
        std::abs(worldLandStepDistance({15, 5}, {0, 6}, 16) - std::sqrt(2.)) <
        1e-9
    );
    PALADIN_CHECK(!worldLandStepAllowed(grid, {3, 3}, {3, -1}));
    PALADIN_CHECK(
        !worldLandRoute(grid, {3, 3}, {4, 3}, WorldLandMovement::EightWay)
    );

    WorldGenerationSettings settings;
    settings.width = 64;
    settings.height = 32;
    settings.seed = 3101;
    settings.populateAiRealms = false;
    World world(settings);
    for (int y = 0; y < 32; ++y)
    {
        for (int x = 0; x < 64; ++x)
        {
            world.grid().tile({x, y})->terrain = TerrainType::Land;
        }
    }
    const auto realm = world.createRealm();
    world.realm(realm)->aiControlled = true;
    auto profile = defaultSettlementFoundationProfile();
    profile.initialPopulation = 100;
    profile.initialDetailedCitizenCount = 0;
    profile.initialResources = {{"bread", 1000}};
    const auto home = world.foundCapitalSettlement(
        {16, 16},
        realm,
        {"March", "Marchers", "March City", {}, "civic"},
        profile
    );
    PALADIN_CHECK(home);
    const auto army =
        MilitarySystem::maintainStrategicGarrison(world, realm, home, 2);
    PALADIN_CHECK(army);
    PALADIN_CHECK(
        MilitarySystem::orderMove(world, realm, army, {18, 18}) ==
        MilitaryResult::Success
    );
    MilitarySystem::tick(
        world,
        360,
        Army::MarchMinutesPerTile * std::sqrt(2.) * .5
    );
    PALADIN_CHECK(std::abs(world.army(army)->visualX() - 16.5) < 1e-6);
    PALADIN_CHECK(std::abs(world.army(army)->visualY() - 16.5) < 1e-6);
    MilitarySystem::tick(
        world,
        364,
        Army::MarchMinutesPerTile * std::sqrt(2.) * .5
    );
    PALADIN_CHECK((world.army(army)->position() == WorldTilePosition{17, 17}));
    world.grid().tile({18, 17})->terrain = TerrainType::Water;
    MilitarySystem::tick(world, 368, .1);
    PALADIN_CHECK(!world.army(army)->moving());
    PALADIN_CHECK((world.army(army)->position() == WorldTilePosition{17, 17}));

    SettlementGrid local(32, 32);
    for (int y = 0; y < 32; ++y)
    {
        for (int x = 0; x < 32; ++x)
        {
            auto& t = *local.tile({x, y});
            t.terrain = TerrainType::Land;
            t.biome = BiomeType::Plain;
            t.temperature = Temperature{.5};
        }
    }
    SettlementMap map(std::move(local), {0, 0}, 1, 1, 32, 3101);
    const auto place = [&](std::string_view type, SettlementObjectFootprint f)
    {
        auto def = *SettlementObjectCatalog::definition(type);
        def.bypassesConstruction = true;
        PALADIN_CHECK(
            map.objectState().placeCompletedObject(map.grid(), def, f)
        );
        return map.objectState().completedObjects().back().id;
    };
    place("city_keep", {{2, 2}, 5, 7});
    const auto pasture = place("pastureland", {{20, 20}, 6, 6});
    map.logistics.synchronize(map.objectState(), 600);
    SettlementCitizenState people;
    PALADIN_CHECK(people.initialize(1, 3101));
    people.placeUnpositionedCitizens(map);
    auto& worker = const_cast<SettlementCitizen&>(people.citizens().front());
    worker.child = false;
    worker.ageYears = 25;
    worker.youngDependents = 0;
    worker.hunger = 0;
    worker.energy = 100;
    worker.tilePosition = {21, 21};
    worker.destination = worker.tilePosition;
    worker.path.clear();
    worker.task = {};
    map.employment().synchronize(map.objectState(), people);
    PALADIN_CHECK(
        map.employment().adjust(map.employment().forObject(pasture), 1, people)
    );
    const auto resident = map.animals.spawn(map, "cow", {23, 23});
    PALADIN_CHECK(resident);
    map.animals.find(resident)->pasture = pasture;
    const auto market = place("market", {{10, 18}, 5, 5});
    const auto marketFootprint =
        map.objectState().completedObject(market)->footprint;
    CitizenMovementPolicy herdPolicy;
    herdPolicy.avoidBuildingFootprints = true;
    SettlementNavigation herdNavigation;
    const auto herdRoute =
        herdNavigation.findPath(map, {8, 21}, {21, 21}, herdPolicy);
    PALADIN_CHECK(!herdRoute.empty());
    for (const auto tile : herdRoute)
    {
        PALADIN_CHECK(!marketFootprint.contains(tile));
    }
    const auto wild = map.animals.spawn(map, "cow", {8, 21});
    PALADIN_CHECK(wild);
    map.animals.find(wild)->nextWanderMinute = 10000;
    SettlementCommandController gather;
    PALADIN_CHECK(gather.begin("gather"));
    gather.pointerPressed(SettlementTilePosition{8, 21});
    PALADIN_CHECK(
        gather.pointerReleased(SettlementTilePosition{8, 21}, map, people, 600)
    );
    bool gathered = false;
    for (int i = 0; i < 150 && !gathered; ++i)
    {
        map.activities.tick(map, people, 600 + i, 1);
        gathered = map.animals.find(wild)->pasture == pasture;
        PALADIN_CHECK(
            !marketFootprint.contains(map.animals.find(wild)->tilePosition)
        );
    }
    PALADIN_CHECK(gathered);
    PALADIN_CHECK(!map.animals.hasOrders());
    PALADIN_CHECK(map.animals.containedCount(pasture) == 2);
    PALADIN_CHECK(worker.workplaceId == map.employment().forObject(pasture));
    const auto covered = map.animals.spawn(map, "pig", {13, 12});
    const auto outside = map.animals.spawn(map, "pig", {10, 12});
    PALADIN_CHECK(covered && outside);
    const SettlementObjectFootprint blueprint{{11, 10}, 5, 5};
    PALADIN_CHECK(map.objectState().createConstructionSites(
        map.grid(),
        *SettlementObjectCatalog::definition("house"),
        blueprint
    ));
    map.animals.find(outside)->handler = worker.id;
    map.animals.follow(outside, worker.id, {11, 12}, map);
    PALADIN_CHECK(
        (map.animals.find(outside)->tilePosition ==
         SettlementTilePosition{10, 12})
    );
    for (int minute = 900; minute < 916; ++minute)
    {
        map.animals.tick(map, people, minute, 1);
        PALADIN_CHECK(
            !blueprint.contains(map.animals.find(outside)->tilePosition)
        );
    }
    PALADIN_CHECK(!blueprint.contains(map.animals.find(covered)->tilePosition));
    PALADIN_CHECK(!map.animals.find(covered)->escapingSite);
    std::cout
        << "[pr31] diagonal army speed/interpolation, blocked corners/seam, "
           "building-safe animal leads and construction escape passed\n";

    // Escape from a huge blueprint must be proportional to distance, rather
    // than exhausting the bounded path search before reaching its edge.
    SettlementGrid largeGrid(128, 128);
    for (int y = 0; y < 128; ++y)
    {
        for (int x = 0; x < 128; ++x)
        {
            largeGrid.tile({x, y})->terrain = TerrainType::Land;
        }
    }
    SettlementMap large(std::move(largeGrid), {0, 0}, 1, 1, 128, 3102);
    const auto largeCow = large.animals.spawn(large, "cow", {64, 64});
    PALADIN_CHECK(largeCow);
    auto largeDefinition = *SettlementObjectCatalog::definition("stockpile");
    largeDefinition.constructionResourceCosts = {};
    const SettlementObjectFootprint largeSite{{14, 14}, 100, 100};
    PALADIN_CHECK(large.objectState().createConstructionSites(
        large.grid(),
        largeDefinition,
        largeSite
    ));
    const auto largeSiteId = large.objectState().constructionSites().back().id;
    for (int minute = 0; minute < 60; ++minute)
    {
        const auto before = large.animals.find(largeCow)->tilePosition;
        large.animals.tick(large, people, minute, 1);
        const auto after = large.animals.find(largeCow)->tilePosition;
        PALADIN_CHECK(
            std::abs(before.x - after.x) + std::abs(before.y - after.y) <= 1
        );
    }
    PALADIN_CHECK(
        !largeSite.contains(large.animals.find(largeCow)->tilePosition)
    );
    PALADIN_CHECK(!large.animals.find(largeCow)->escapingSite);
    PALADIN_CHECK(large.objectState().build(largeSiteId, 1, 1, {15, 15}));
    PALADIN_CHECK(large.objectState().constructionSites().empty());
    std::cout
        << "[pr31/animals] 100x100 blueprint escape and completion passed\n";

    // Two real prepared settlements: physical work must advance off-screen,
    // including visits shorter than the background batching interval.
    Simulation sim(settings);
    for (int y = 0; y < 32; ++y)
    {
        for (int x = 0; x < 64; ++x)
        {
            auto& t = *sim.world().grid().tile({x, y});
            t.terrain = TerrainType::Land;
            t.biome = BiomeType::Plain;
            t.elevation = Elevation{.5};
        }
    }
    const auto first =
        sim.foundPlayerCapital({16, 16}, {"A", "A Folk", "A", {}, "civic"});
    auto secondProfile = defaultSettlementFoundationProfile();
    const auto second = sim.world().foundSettlement(
        {44, 16},
        sim.playerRealmId(),
        secondProfile
    );
    PALADIN_CHECK(first && second);
    SettlementMapGenerationSettings localSettings;
    localSettings.localTilesPerWorldTile = 16;
    PALADIN_CHECK(
        sim.prepareSettlementMap(first, localSettings) &&
        sim.prepareSettlementMap(second, localSettings)
    );
    for (const auto id : {first, second})
    {
        auto& localMap = *sim.settlementMap(id);
        for (int y = 0; y < localMap.grid().height(); ++y)
        {
            for (int x = 0; x < localMap.grid().width(); ++x)
            {
                localMap.grid().tile({x, y})->terrain = TerrainType::Land;
            }
        }
        localMap.naturalFeatures().clear(
            {{0, 0}, localMap.grid().width(), localMap.grid().height()}
        );
        PALADIN_CHECK(localMap.objectState().placeCompletedObject(
            localMap.grid(),
            *SettlementObjectCatalog::definition("city_keep"),
            {{2, 2}, 5, 7}
        ));
        localMap.logistics.synchronize(localMap.objectState(), 360);
        auto& citizens =
            sim.world().settlement(id)->simulationState().citizens();
        citizens.placeUnpositionedCitizens(localMap);
        localMap.naturalFeatures().set({12, 12}, NaturalFeatureKind::Tree);
        SettlementCommandController command;
        PALADIN_CHECK(command.begin("chop_tree"));
        command.pointerPressed(SettlementTilePosition{12, 12});
        PALADIN_CHECK(command.pointerReleased(
            SettlementTilePosition{12, 12},
            localMap,
            citizens,
            360
        ));
        PALADIN_CHECK(localMap.objectState().createConstructionSites(
            localMap.grid(),
            *SettlementObjectCatalog::definition("house"),
            {{14, 4}, 5, 5}
        ));
    }
    PALADIN_CHECK(sim.setDetailedSimulationSettlement(first));
    sim.setSpeed(SimulationSpeed::Normal);
    for (int i = 0; i < 6; ++i)
    {
        sim.tick(.05);
        PALADIN_CHECK(
            sim.setDetailedSimulationSettlement(i % 2 ? first : second)
        );
    }
    PALADIN_CHECK(sim.setDetailedSimulationSettlement(second));
    const auto before =
        sim.world().settlement(first)->simulationState().localActivityUpdates();
    for (int i = 0; i < 3600; ++i)
    {
        sim.tick(.05);
    }
    PALADIN_CHECK(
        sim.world()
            .settlement(first)
            ->simulationState()
            .localActivityUpdates() > before
    );
    for (const auto id : {first, second})
    {
        const auto& localMap = *sim.settlementMap(id);
        PALADIN_CHECK(
            localMap.naturalFeatures().at({12, 12}).kind ==
            NaturalFeatureKind::None
        );
        PALADIN_CHECK(localMap.objectState().constructionSites().empty());
        PALADIN_CHECK(localMap.objectState().completedObjectAt({15, 5}));
        PALADIN_CHECK(sim.setDetailedSimulationSettlement(id));
        PALADIN_CHECK(
            sim.world()
                .settlement(id)
                ->simulationState()
                .pendingLocalActivityMinutes() == 0
        );
    }
    const auto ticks =
        sim.world().settlement(first)->simulationState().localActivityUpdates();
    sim.setSpeed(SimulationSpeed::Paused);
    sim.tick(10);
    PALADIN_CHECK(
        sim.world()
            .settlement(first)
            ->simulationState()
            .localActivityUpdates() == ticks
    );
    std::cout
        << "[pr31/background] short A/B visits, off-screen gathering/material "
           "delivery/building, reactivation flush and pause passed\n";
}

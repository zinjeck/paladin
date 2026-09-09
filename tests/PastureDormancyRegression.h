#pragma once

#include "TestFramework.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include "world/settlements/commands/SettlementCommandDefinition.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include <utility>

using namespace Paladin;

inline void runPastureDormancyRegression()
{
    SettlementGrid grid(32, 32);
    for (int y = 0; y < 32; ++y)
    {
        for (int x = 0; x < 32; ++x)
        {
            auto& tile = *grid.tile({x, y});
            tile.terrain = TerrainType::Land;
            tile.biome = BiomeType::Forest;
            tile.temperature = Temperature(.5F);
            tile.rainfall = Rainfall(.7F);
        }
    }

    SettlementMap map(std::move(grid), {0, 0}, 1, 1, 32, 991);
    auto keepDefinition =
        *SettlementObjectCatalog::definition(SettlementObjectTypes::CityKeep);
    auto pastureDefinition =
        *SettlementObjectCatalog::definition(SettlementObjectTypes::Pastureland);
    keepDefinition.bypassesConstruction = true;
    pastureDefinition.bypassesConstruction = true;
    PALADIN_CHECK(map.objectState().placeCompletedObject(
        map.grid(),
        keepDefinition,
        {{2, 2}, keepDefinition.previewWidth, keepDefinition.previewHeight}
    ));
    PALADIN_CHECK(map.objectState().placeCompletedObject(
        map.grid(),
        pastureDefinition,
        {{20, 20}, 4, 4}
    ));
    const auto pasture = map.objectState().completedObjects().back().id;

    map.logistics.synchronize(map.objectState(), 0);
    SettlementCitizenState citizens;
    PALADIN_CHECK(citizens.initialize(1, 123));
    citizens.placeUnpositionedCitizens(map);
    map.employment().synchronize(map.objectState(), citizens);
    const auto pastureJob = map.employment().forObject(pasture);
    PALADIN_CHECK(pastureJob);
    PALADIN_CHECK(map.employment().adjust(pastureJob, 1, citizens));

    auto& worker =
        const_cast<SettlementCitizen&>(citizens.citizens().front());
    worker.child = false;
    worker.youngDependents = 0;
    worker.health = 100;
    worker.hunger = 0;
    worker.energy = 100;
    map.activities.policy.gatheringMinutes = 120;

    map.naturalFeatures().set({12, 12}, NaturalFeatureKind::Tree);
    PALADIN_CHECK(map.commandState().add(
        map,
        SettlementCommandTypes::ChopTree,
        {{12, 12}, 1, 1},
        citizens
    ));
    const auto commandId = map.commandState().commands().front().id;

    bool beganCivicWork = false;
    for (int i = 0; i < 60 && !beganCivicWork; ++i)
    {
        map.activities.tick(map, citizens, 360 + i, 1);
        const auto& current = citizens.citizens().front();
        beganCivicWork = current.task.kind == CitizenTaskKind::Gather;
    }
    PALADIN_CHECK(beganCivicWork);
    PALADIN_CHECK(citizens.citizens().front().workplaceId == pastureJob);
    PALADIN_CHECK(map.activities.pastureWorkerAvailableForGeneralLabor(
        map,
        citizens.citizens().front()
    ));

    const auto cow = map.animals.spawn(map, "cow", {21, 21});
    PALADIN_CHECK(cow);
    map.animals.find(cow)->pasture = pasture;
    PALADIN_CHECK(!map.activities.pastureWorkerAvailableForGeneralLabor(
        map,
        citizens.citizens().front()
    ));

    map.activities.tick(map, citizens, 420, 1);
    PALADIN_CHECK(
        citizens.citizens().front().task.kind != CitizenTaskKind::Gather
    );
    PALADIN_CHECK(map.commandState().contains(
        map,
        commandId,
        {12, 12},
        {},
        {}
    ));

    bool returnedToPasture = false;
    for (int i = 0; i < 120 && !returnedToPasture; ++i)
    {
        map.activities.tick(map, citizens, 421 + i, 1);
        const auto& current = citizens.citizens().front();
        returnedToPasture =
            current.task.kind == CitizenTaskKind::Work &&
            current.task.object == pasture;
    }
    PALADIN_CHECK(returnedToPasture);
    PALADIN_CHECK(citizens.citizens().front().workplaceId == pastureJob);
}

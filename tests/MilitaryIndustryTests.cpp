#include "TestFramework.h"
#include "simulation/MilitarySystem.h"
#include "simulation/Simulation.h"
#include "world/World.h"
#include "world/generation/WorldGenerationSettings.h"
#include "world/settlements/SettlementFoodDemand.h"
#include "world/settlements/SettlementIndustry.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include "world/settlements/commands/SettlementCommandDefinition.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <set>

namespace
{
    using namespace Paladin;
    SettlementObjectId complete(SettlementMap& map, std::string_view type,
                                SettlementObjectFootprint footprint)
    {
        auto definition = *SettlementObjectCatalog::definition(type);
        definition.bypassesConstruction = true;
        PALADIN_CHECK(map.objectState().placeCompletedObject(map.grid(), definition, footprint));
        map.naturalFeatures().clear(footprint);
        map.logistics.synchronize(map.objectState(), 360);
        return map.objectState().completedObjects().back().id;
    }
    SettlementMap land()
    {
        SettlementGrid grid(40, 40);
        for (int y = 0; y < 40; ++y)
            for (int x = 0; x < 40; ++x)
            {
                auto& tile = *grid.tile({x,y});
                tile.terrain = TerrainType::Land; tile.biome = BiomeType::Plain;
                tile.temperature = Temperature{.5}; tile.rainfall = Rainfall{.7};
            }
        return SettlementMap(std::move(grid), {0,0}, 1, 1, 40, 771);
    }
    Money cash(const SettlementMap& map)
    { return map.commerce.treasury->balance + map.commerce.businessTotal() + map.commerce.householdTotal(); }
    int food(const SettlementMap& map)
    {
        int total = 0;
        for (const auto& definition : SettlementResourceCatalog::definitions())
            if (definition.edible) total += int(map.logistics.total(definition.id));
        return total;
    }
    void testIndustry()
    {
        auto map = land();
        const auto keep = complete(map, "city_keep", {{2,2},5,7});
        const auto bakery = complete(map, "bakery", {{11,2},5,5});
        const auto depot = complete(map, "army_supply_depot", {{18,2},5,5});
        const auto barracks = complete(map, "barracks", {{25,2},5,5});
        const auto farm = complete(map, "wheat_farm", {{12,12},3,3});
        SettlementCitizenState people;
        PALADIN_CHECK(people.initialize(8, 711));
        people.placeUnpositionedCitizens(map);
        map.employment().synchronize(map.objectState(), people);
        for (const auto object : {bakery, depot, barracks, farm})
            PALADIN_CHECK(map.employment().adjust(map.employment().forObject(object), 1, people));
        map.commerce.treasury->balance = 100000;
        map.commerce.update(map, people, 360, 0);
        const Money totalCash = cash(map);
        const auto source = map.logistics.forObject(keep);
        const auto bakeryInventory = map.logistics.forObject(bakery);
        const auto depotInventory = map.logistics.forObject(depot);
        const auto barracksInventory = map.logistics.forObject(barracks);
        // The founding keep is full (40 lumber, 40 stone, 20 fish).
        PALADIN_CHECK(map.logistics.consumeAvailable(source, "lumber", 20));
        PALADIN_CHECK(map.logistics.add(source, "wheat", 20, 360));
        const auto bakeryCash = map.commerce.businessCash(bakery);
        const auto wheat = map.logistics.total("wheat");
        PALADIN_CHECK(produceIndustry(map, bakery, 1, 360, 30));
        PALADIN_CHECK(map.logistics.total("bread") == 1);
        PALADIN_CHECK(map.logistics.total("wheat") == wheat - 1);
        PALADIN_CHECK(map.commerce.businessCash(bakery) < bakeryCash);
        PALADIN_CHECK(cash(map) == totalCash);
        const int ordinary = food(map);
        const auto depotCash = map.commerce.businessCash(depot);
        PALADIN_CHECK(produceIndustry(map, depot, 1, 360, 180));
        PALADIN_CHECK(map.logistics.total("rations") == 6);
        PALADIN_CHECK(food(map) == ordinary); // conversion, not duplicate food
        PALADIN_CHECK(map.commerce.businessCash(depot) < depotCash);
        const auto sellerCash = map.commerce.businessCash(depot);
        const auto soldierCash = map.commerce.businessCash(barracks);
        PALADIN_CHECK(produceIndustry(map, barracks, 1, 360, 1));
        PALADIN_CHECK(map.logistics.inventory(barracksInventory)->amount("rations") == 6);
        PALADIN_CHECK(map.logistics.inventory(depotInventory)->amount("rations") == 0);
        PALADIN_CHECK(map.commerce.businessCash(depot) > sellerCash);
        PALADIN_CHECK(map.commerce.businessCash(barracks) < soldierCash);
        PALADIN_CHECK(cash(map) == totalCash);
        PALADIN_CHECK(!map.logistics.canEat("rations"));
        PALADIN_CHECK(!map.logistics.mayExport(map.objectState(), *map.logistics.inventory(barracksInventory), "rations"));
        PALADIN_CHECK(!map.logistics.mayExport(map.objectState(), *map.logistics.inventory(bakeryInventory), "wheat"));
        PALADIN_CHECK(map.logistics.mayExport(map.objectState(), *map.logistics.inventory(bakeryInventory), "fish"));
        // Ingredient depletion belongs to industry, never the civilian meal mix.
        const auto report = map.commerce.dailyResourceReport(map, people, 361);
        double depletion = 0;
        for (const auto& definition : SettlementResourceCatalog::definitions())
            if (definition.edible) depletion += report.at(std::string(definition.id)).depletion;
        PALADIN_CHECK(std::abs(depletion - (16 + 6)) < 1e-8);
        PALADIN_CHECK(report.at("rations").depletion == 0);
        // Calendar maturation, finite harvest, and no second harvest at the same time.
        const auto farmInventory = map.logistics.forObject(farm);
        PALADIN_CHECK(produceIndustry(map, farm, 1, 360, 1));
        PALADIN_CHECK(map.logistics.inventory(farmInventory)->amount("wheat") == 0);
        PALADIN_CHECK(produceIndustry(map, farm, 1, 360 + 3*1440, 120));
        PALADIN_CHECK(map.logistics.inventory(farmInventory)->amount("wheat") == 9);
        PALADIN_CHECK(produceIndustry(map, farm, 1, 360 + 3*1440, 120));
        PALADIN_CHECK(map.logistics.inventory(farmInventory)->amount("wheat") == 9);
        // A full processor can replace input with output; reserved input stays put.
        const int full = map.logistics.freeSpace(bakeryInventory);
        PALADIN_CHECK(map.logistics.add(bakeryInventory, "wheat", full, 361));
        PALADIN_CHECK(map.logistics.reserve(CitizenId{1000}, bakeryInventory, source, "wheat", 2));
        const int before = map.logistics.inventory(bakeryInventory)->used();
        const int available = map.logistics.available(bakeryInventory, "wheat");
        PALADIN_CHECK(map.logistics.convert(bakeryInventory, "wheat", "bread", 100000, 361) == available);
        PALADIN_CHECK(map.logistics.inventory(bakeryInventory)->used() == before);
        PALADIN_CHECK(map.logistics.inventory(bakeryInventory)->amount("wheat") == 2);
        PALADIN_CHECK(map.logistics.convert(bakeryInventory, "wheat", "bread", 2, 361) == 0);
        PALADIN_CHECK(map.logistics.convert(bakeryInventory, "bread", "bread", 1, 361) == 0);
        // No phantom output while frozen: stocked recipe inputs still limit it.
        const auto recipeTotal = map.logistics.total("wheat") + map.logistics.total("bread");
        map.commerce.captureInactive(map, people);
        map.commerce.tickInactive(map, people, 361, 60);
        PALADIN_CHECK(map.logistics.total("wheat") + map.logistics.total("bread") <= recipeTotal);
        PALADIN_CHECK(cash(map) == totalCash);
        std::cout << "[military/industry] paid purchases, conversion, mature harvest and frozen-input conservation passed\n";
    }
    void testEmergencyFoodAndGathering()
    {
        auto map = land();
        const auto keep = complete(map, "city_keep", {{2,2},5,7});
        const auto destination = map.logistics.forObject(keep);
        for (const auto& inventory : map.logistics.inventories())
            for (const auto& definition : SettlementResourceCatalog::definitions())
                if (definition.edible && map.logistics.available(inventory.id, definition.id) > 0)
                    PALADIN_CHECK(map.logistics.consumeAvailable(inventory.id, definition.id,
                        map.logistics.available(inventory.id, definition.id)));
        PALADIN_CHECK(map.logistics.add(destination, "rations", 8, 360));
        PALADIN_CHECK(map.logistics.canEat("rations"));
        const auto fish = map.logistics.drop({12,12}, "fish", 1, 360);
        PALADIN_CHECK(!map.logistics.canEat("rations"));
        PALADIN_CHECK(map.logistics.reserve(CitizenId{100}, fish, destination, "fish", 1));
        PALADIN_CHECK(map.logistics.pickUp(CitizenId{100}));
        PALADIN_CHECK(!map.logistics.canEat("rations")); // ordinary food in transit
        PALADIN_CHECK(map.logistics.consumeCarriedUnit(CitizenId{100}));
        PALADIN_CHECK(map.logistics.canEat("rations"));
        SettlementCitizenState people;
        PALADIN_CHECK(people.initialize(1, 47));
        people.placeUnpositionedCitizens(map);
        auto& person = const_cast<SettlementCitizen&>(people.citizens().front());
        person.hunger = 0; person.energy = 100;
        map.naturalFeatures().set({10,10}, NaturalFeatureKind::Wheat);
        PALADIN_CHECK(map.commandState().add(map, SettlementCommandTypes::Gather, {{10,10},1,1}, people));
        map.activities.tick(map, people, 360, 100);
        PALADIN_CHECK(map.naturalFeatures().at({10,10}).kind == NaturalFeatureKind::None);
        PALADIN_CHECK(map.logistics.total("wheat") == 4);
        PALADIN_CHECK(map.commerce.resourceTotals().at("wheat").produced == 4);
        const auto* farm = SettlementObjectCatalog::definition("wheat_farm");
        PALADIN_CHECK(std::any_of(farm->constructionResourceCosts.begin(), farm->constructionResourceCosts.end(),
            [](const auto& cost) { return cost.resourceId == "wheat" && cost.requiredAmount > 0; }));
        PALADIN_CHECK(person.walkDistance > 0 && person.workAnimationMinutes > 0);
        std::cout << "[military/industry] last-resort food, carried-food exclusion and harvest animation clocks passed\n";
    }
    void testUnits()
    {
        WorldGenerationSettings settings;
        settings.width = settings.height = 64; settings.seed = 711; settings.populateAiRealms = false;
        Simulation sim(settings);
        auto& world = sim.world();
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 64; ++x)
            {
                auto& tile = *world.grid().tile({x,y});
                tile.terrain = TerrainType::Land; tile.biome = BiomeType::Plain;
                tile.temperature = Temperature{.5}; tile.rainfall = Rainfall{.7};
            }
        const auto city = sim.foundPlayerCapital({32,32}, {"Supply Realm", "Supply Folk", "Supply City", {}, "civic", {}});
        PALADIN_CHECK(city);
        SettlementMapGenerationSettings localSettings; localSettings.localTilesPerWorldTile = 4;
        PALADIN_CHECK(sim.prepareSettlementMap(city, localSettings));
        auto& map = *sim.settlementMap(city);
        for (int y=0;y<map.grid().height();++y)
            for (int x=0;x<map.grid().width();++x) map.grid().tile({x,y})->terrain = TerrainType::Land;
        complete(map, "city_keep", {{2,2},5,7});
        const auto barracks = complete(map, "barracks", {{11,2},5,5});
        auto& people = world.settlement(city)->simulationState().citizens();
        people.placeUnpositionedCitizens(map);
        const auto population = world.settlement(city)->population();
        const auto actor = sim.playerRealmId(), enemy = world.createRealm();
        PALADIN_CHECK(MilitarySystem::recruit(world, actor, city, 1) == MilitaryResult::Success);
        PALADIN_CHECK(MilitarySystem::recruit(world, actor, city, 1) == MilitaryResult::Success);
        PALADIN_CHECK(world.soldiers().size() == 2 && MilitarySystem::available(world, city) == 2);
        std::set<std::uint64_t> sources;
        for (const auto& soldier : world.soldiers())
        {
            PALADIN_CHECK(soldier.homeSettlementId() == city && soldier.barracksId() == barracks);
            PALADIN_CHECK(sources.insert(soldier.sourceCitizenId().value()).second);
        }
        const auto unitId = MilitarySystem::createUnit(world, actor, city);
        PALADIN_CHECK(unitId);
        PALADIN_CHECK(!MilitarySystem::createUnit(world, enemy, city));
        PALADIN_CHECK(MilitarySystem::resizeUnit(world, enemy, unitId, 1) == MilitaryResult::NotOwned);
        PALADIN_CHECK(MilitarySystem::resizeUnit(world, actor, unitId, 5) == MilitaryResult::Success);
        PALADIN_CHECK(world.army(unitId)->soldierCount() == 2);
        PALADIN_CHECK(MilitarySystem::available(world, city) == 0);
        PALADIN_CHECK(MilitarySystem::recruit(world, actor, city, -1) == MilitaryResult::NoBarracksEmployee);
        PALADIN_CHECK(world.settlement(city)->population() == population);
        const auto inventory = map.logistics.forObject(barracks);
        PALADIN_CHECK(map.logistics.add(inventory, "rations", 12, 360));
        for (const auto& c : people.citizens()) const_cast<SettlementCitizen&>(c).hunger = 0;
        PALADIN_CHECK(MilitarySystem::orderMove(world, actor, unitId, {33,32}) == MilitaryResult::Success);
        PALADIN_CHECK(world.army(unitId)->rations() == 12);
        PALADIN_CHECK(map.logistics.inventory(inventory)->amount("rations") == 0);
        PALADIN_CHECK(MilitarySystem::resizeUnit(world, actor, unitId, -1) == MilitaryResult::ReturnHome);
        MilitarySystem::tick(world, 360, 15);
        PALADIN_CHECK(std::abs(world.army(unitId)->visualX() - 32.5) < 1e-8);
        int deployed = 0;
        for (const auto& c : people.citizens())
            if (c.militaryDeployed)
            {
                ++deployed;
                PALADIN_CHECK(c.soldierId && c.militaryUnitId == unitId && c.task.kind == CitizenTaskKind::None);
                PALADIN_CHECK(citizenFoodPerDay(c, map.activities.policy) == 0);
            }
        PALADIN_CHECK(deployed == 2);
        // A mid-step re-order preserves the current visual position.
        PALADIN_CHECK(MilitarySystem::orderMove(world, actor, unitId, {32,32}) == MilitaryResult::Success);
        PALADIN_CHECK(std::abs(world.army(unitId)->visualX() - 32.5) < 1e-8);
        MilitarySystem::tick(world, 375, 45);
        PALADIN_CHECK(MilitarySystem::presentAt(world, *world.army(unitId), city));
        PALADIN_CHECK(MilitarySystem::resizeUnit(world, actor, unitId, -2) == MilitaryResult::Success);
        PALADIN_CHECK(map.logistics.inventory(inventory)->amount("rations") == 12);
        PALADIN_CHECK(world.army(unitId)->rations() == 0 && MilitarySystem::available(world, city) == 2);
        PALADIN_CHECK(MilitarySystem::disbandUnit(world, actor, unitId) == MilitaryResult::Success);
        PALADIN_CHECK(!world.army(unitId));
        const auto empty = MilitarySystem::createUnit(world, actor, city);
        PALADIN_CHECK(world.setArmyPosition(empty, {20,20}));
        PALADIN_CHECK(MilitarySystem::disbandUnit(world, enemy, empty) == MilitaryResult::NotOwned);
        PALADIN_CHECK(MilitarySystem::disbandUnit(world, actor, empty) == MilitaryResult::Success);
        const auto march = MilitarySystem::createUnit(world, actor, city);
        PALADIN_CHECK(MilitarySystem::resizeUnit(world, actor, march, 2) == MilitaryResult::Success);
        world.grid().tile({33,32})->terrain = TerrainType::Water;
        PALADIN_CHECK(MilitarySystem::orderMove(world, actor, march, {33,32}) == MilitaryResult::InvalidDestination);
        world.grid().tile({33,32})->terrain = TerrainType::Land;
        PALADIN_CHECK(MilitarySystem::orderMove(world, enemy, march, {33,32}) == MilitaryResult::NotOwned);
        PALADIN_CHECK(MilitarySystem::orderMove(world, actor, march, {33,32}) == MilitaryResult::Success);
        sim.setSpeed(SimulationSpeed::Paused);
        sim.tick(1);
        PALADIN_CHECK(world.army(march)->visualX() == 32);
        sim.setSpeed(SimulationSpeed::Normal);
        sim.tick(1);
        PALADIN_CHECK(world.army(march)->visualX() > 32);
        // Dead source people cannot persist as phantom soldiers.
        const auto casualty = world.soldier(world.army(march)->soldiers().front())->sourceCitizenId();
        for (const auto& c : people.citizens())
            if (c.id == casualty) const_cast<SettlementCitizen&>(c).health = 0;
        MilitarySystem::synchronize(world, 430);
        PALADIN_CHECK(world.army(march)->soldierCount() == 1 && world.soldiers().size() == 1);
        std::cout << "[military/industry] actual employment, authority, pause, travel, redeployment and casualty rosters passed\n";
    }
}
void runMilitaryIndustryTests()
{
    testIndustry();
    testEmergencyFoodAndGathering();
    testUnits();
}

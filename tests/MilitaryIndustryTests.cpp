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
    void testHouseholdFuel()
    {
        auto map = land();
        const auto house = complete(map, "house", {{12, 12}, 5, 5});
        const auto market = complete(map, "market", {{22, 12}, 4, 6});
        SettlementCitizenState people;
        PALADIN_CHECK(people.initialize(3, 772));
        for (const auto& record : people.citizens())
        {
            const_cast<SettlementCitizen&>(record).homeId = house;
        }
        map.commerce.treasury->balance = 100000;
        map.commerce.policy.startingSavings = 60;
        map.employment().synchronize(map.objectState(), people);
        PALADIN_CHECK(map.employment().adjust(
            map.employment().forObject(market),
            1,
            people
        ));
        map.commerce.update(map, people, 360, 0);
        const auto from =
            *map.logistics.inventory(map.logistics.forObject(market));
        const auto to =
            *map.logistics.inventory(map.logistics.forObject(house));
        PALADIN_CHECK(map.logistics.add(from.id, "lumber", 4));
        const auto totalCash = cash(map);
        const auto storeCash = map.commerce.businessCash(market);
        PALADIN_CHECK(
            map.commerce.affordableTradeUnits(from, to, 4, &people) == 3
        );
        PALADIN_CHECK(!map.commerce.buyGoods(from, to, 4, &people));
        PALADIN_CHECK(cash(map) == totalCash);
        PALADIN_CHECK(map.commerce.buyGoods(from, to, 3, &people));
        PALADIN_CHECK(
            map.logistics.moveAvailable(from.id, to.id, "lumber", 3) == 3
        );
        PALADIN_CHECK(map.commerce.householdTotal() == 0);
        PALADIN_CHECK(map.commerce.businessCash(market) == storeCash + 180);
        PALADIN_CHECK(cash(map) == totalCash);
        PALADIN_CHECK(!map.commerce.buyGoods(from, to, 1, &people));
        map.heating.advance(map.logistics, {house}, 0, 1440);
        PALADIN_CHECK(map.heating.heated(house));
        PALADIN_CHECK(map.heating.lumberBurned() == 3);
        PALADIN_CHECK(map.logistics.total("lumber") == 1);
        std::cout << "Household fuel: pooled adult purchasing, insufficient "
                     "funds, conserved cash and daily lumber use passed\n";
    }
    void testMining()
    {
        auto map = land();
        complete(map, "city_keep", {{2, 2}, 5, 7});
        const auto* definition =
            SettlementObjectCatalog::definition("iron_mine");
        PALADIN_CHECK(!map.objectState().placeCompletedObject(
            map.grid(),
            *definition,
            {{12, 12}, 3, 3}
        ));
        for (int y = 12; y < 15; ++y)
        {
            for (int x = 12; x < 15; ++x)
            {
                map.grid().tile({x, y})->mineral = MineralDeposit::Iron;
            }
        }
        map.objectState().invalidateTerrainCache();
        map.objectState().rebuildOccupancy();
        const auto mine = complete(map, "iron_mine", {{12, 12}, 3, 3});
        const auto object = *map.objectState().completedObject(mine);
        PALADIN_CHECK(produceIndustry(map, mine, 2, 360, 120));
        PALADIN_CHECK(map.logistics.total("iron") == 2);
        const auto depth = map.mining.depth(object);
        PALADIN_CHECK(depth > 0 && depth < .52);
        const auto inventory = map.logistics.forObject(mine);
        PALADIN_CHECK(
            map.logistics
                .add(inventory, "iron", map.logistics.freeSpace(inventory), 361)
        );
        PALADIN_CHECK(produceIndustry(map, mine, 2, 361, 120));
        PALADIN_CHECK(map.mining.depth(object) == depth);
        SettlementMiningState single, split;
        const auto a = single.work(map.grid(), object, 3, 720, 10000);
        int b = 0;
        for (int i = 0; i < 720; ++i)
        {
            b += split.work(map.grid(), object, 3, 1, 10000);
        }
        PALADIN_CHECK(a == b && a == 18);
        PALADIN_CHECK(
            std::abs(single.depth(object) - split.depth(object)) < 1e-9
        );
        PALADIN_CHECK(single.work(map.grid(), object, 0, 100, 100) == 0);
        PALADIN_CHECK(single.work(map.grid(), object, 1, 0, 100) == 0);
        int removed = a;
        for (int i = 0; i < 4; ++i)
        {
            removed += single.work(map.grid(), object, 100, 100000, 10000);
        }
        PALADIN_CHECK(removed == 9 * 250 && single.find(mine)->exhausted);
        auto rebuilt = object;
        rebuilt.id = SettlementObjectId{999};
        PALADIN_CHECK(
            single.work(map.grid(), rebuilt, 100, 100000, 10000) == 0
        );
        rebuilt.objectTypeId = SettlementObjectTypes::Quarry;
        rebuilt.id = SettlementObjectId{1000};
        PALADIN_CHECK(single.work(map.grid(), rebuilt, 1, 90, 100) == 1);
        PALADIN_CHECK(single.depth(rebuilt) > 0);
        std::cout << "Mining: required ore, actual output, full storage, time "
                     "partition and persistent separate deposits passed\n";
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
        PALADIN_CHECK(!map.logistics.add(source, "wheat", 20, 360));
        PALADIN_CHECK(map.logistics.drop({9, 4}, "wheat", 20, 360));
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
        PALADIN_CHECK(
            map.logistics
                .reserve(CitizenId{1000}, bakeryInventory, {}, "wheat", 2)
        );
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
        const auto store = complete(map, "stockpile", {{20, 20}, 3, 3});
        const auto destination = map.logistics.forObject(store);
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
        PALADIN_CHECK(world.army(unitId)->soldierCount()==1);
        PALADIN_CHECK(world.army(unitId)->stationedSettlementId()==city);
        PALADIN_CHECK(MilitarySystem::available(world,city)==1);
        PALADIN_CHECK(unitId);
        PALADIN_CHECK(!MilitarySystem::createUnit(world, enemy, city));
        PALADIN_CHECK(MilitarySystem::resizeUnit(world, enemy, unitId, 1) == MilitaryResult::NotOwned);
        PALADIN_CHECK(MilitarySystem::resizeUnit(world, actor, unitId, 5) == MilitaryResult::Success);
        PALADIN_CHECK(world.army(unitId)->soldierCount() == 2);
        PALADIN_CHECK(MilitarySystem::available(world, city) == 0);
        PALADIN_CHECK(MilitarySystem::recruit(world, actor, city, -1) == MilitaryResult::NoBarracksEmployee);
        PALADIN_CHECK(world.settlement(city)->population() == population);
        PALADIN_CHECK(!MilitarySystem::createUnit(world,actor,city));
        PALADIN_CHECK(MilitarySystem::orderMove(world,actor,unitId,{32,32})==MilitaryResult::Success);
        PALADIN_CHECK(world.army(unitId)->stationedSettlementId()==city);
        PALADIN_CHECK(!world.army(unitId)->moving());
        const auto inventory = map.logistics.forObject(barracks);
        PALADIN_CHECK(map.logistics.add(inventory, "rations", 12, 360));
        for (const auto& c : people.citizens()) const_cast<SettlementCitizen&>(c).hunger = 0;
        PALADIN_CHECK(MilitarySystem::orderMove(world, actor, unitId, {33,32}) == MilitaryResult::Success);
        PALADIN_CHECK(world.army(unitId)->rations() == 12);
        PALADIN_CHECK(!world.army(unitId)->stationedSettlementId());
        PALADIN_CHECK(!MilitarySystem::presentAt(world,*world.army(unitId),city));
        PALADIN_CHECK(map.employment().employed(map.employment().forObject(barracks),people)==0);
        PALADIN_CHECK(map.employment().unemployed(people)==6);
        for (const auto id : world.army(unitId)->soldiers())
            PALADIN_CHECK(!people.citizen(world.soldier(id)->sourceCitizenId())->workplaceId);
        PALADIN_CHECK(map.logistics.inventory(inventory)->amount("rations") == 0);
        PALADIN_CHECK(MilitarySystem::resizeUnit(world, actor, unitId, -1) == MilitaryResult::ReturnHome);
        MilitarySystem::tick(world, 360, Army::MarchMinutesPerTile*.5);
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
        const auto empty = world.createArmy({32,32});
        PALADIN_CHECK(world.assignArmyToRealm(empty, actor));
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
    void testIndependentFieldForce()
    {
        WorldGenerationSettings settings;
        settings.width=settings.height=64; settings.seed=711; settings.populateAiRealms=false;
        Simulation sim(settings); auto& world=sim.world();
        for(int y=0;y<64;++y) for(int x=0;x<64;++x)
        { auto& t=*world.grid().tile({x,y}); t.terrain=TerrainType::Land; t.biome=BiomeType::Plain; }
        const auto origin=sim.foundPlayerCapital({30,32},{"Field Realm","Field Folk","Origin",{},"civic",{}});
        const auto other=sim.foundPlayerSettlement({42,32},"Supply City");
        PALADIN_CHECK(origin && other);
        SettlementMapGenerationSettings settingsLocal; settingsLocal.localTilesPerWorldTile=4;
        const auto prepare=[&](SettlementId id)
        {
            PALADIN_CHECK(sim.prepareSettlementMap(id,settingsLocal));
            auto& local=*sim.settlementMap(id);
            for(int y=0;y<local.grid().height();++y) for(int x=0;x<local.grid().width();++x)
                local.grid().tile({x,y})->terrain=TerrainType::Land;
            complete(local,"city_keep",{{2,2},5,7});
            const auto barracks=complete(local,"barracks",{{11,2},5,5});
            world.settlement(id)->simulationState().citizens().placeUnpositionedCitizens(local);
            return barracks;
        };
        const auto originalBarracks=prepare(origin), otherBarracks=prepare(other);
        const auto actor=sim.playerRealmId(), enemy=world.createRealm();
        PALADIN_CHECK(MilitarySystem::recruit(world,actor,origin,1)==MilitaryResult::Success);
        PALADIN_CHECK(MilitarySystem::recruit(world,actor,other,1)==MilitaryResult::Success);
        // The realm reserve can form a unit in another owned city without
        // losing the source person's identity or duplicating population.
        const auto canonicalCount = world.soldiers().size();
        const auto pool = MilitarySystem::reserves(world, actor);
        const auto outpost = world.foundSettlement({44, 46}, actor);
        PALADIN_CHECK(outpost && !sim.settlementMap(outpost));
        const auto reserveUnit =
            MilitarySystem::createUnit(world, actor, outpost, true);
        PALADIN_CHECK(
            reserveUnit &&
            world.army(reserveUnit)->garrisonSettlementId() == outpost
        );
        PALADIN_CHECK(
            !sim.settlementMap(outpost)
        ); // no hidden terrain generation
        const auto reserveSoldier = world.army(reserveUnit)->soldiers().front();
        const auto sourceCity =
            world.soldier(reserveSoldier)->homeSettlementId();
        const auto sourcePerson =
            world.soldier(reserveSoldier)->sourceCitizenId();
        const auto sourceAge = world.settlement(sourceCity)
                                   ->simulationState()
                                   .citizens()
                                   .citizen(sourcePerson)
                                   ->ageYears;
        PALADIN_CHECK(
            sourceCity == origin &&
            MilitarySystem::reserves(world, actor) == pool - 1
        );
        PALADIN_CHECK(
            MilitarySystem::setGarrison(world, enemy, reserveUnit, {}) ==
            MilitaryResult::NotOwned
        );
        PALADIN_CHECK(
            MilitarySystem::setGarrison(world, actor, reserveUnit, origin) ==
            MilitaryResult::ReturnHome
        );
        PALADIN_CHECK(
            MilitarySystem::resizeUnit(world, actor, reserveUnit, -1) ==
            MilitaryResult::Success
        );
        PALADIN_CHECK(
            world.soldier(reserveSoldier)->homeSettlementId() == sourceCity
        );
        PALADIN_CHECK(
            world.settlement(sourceCity)
                ->simulationState()
                .citizens()
                .citizen(sourcePerson)
                ->ageYears == sourceAge
        );
        PALADIN_CHECK(
            MilitarySystem::reserves(world, actor) == pool &&
            world.soldiers().size() == canonicalCount
        );
        PALADIN_CHECK(
            MilitarySystem::disbandUnit(world, actor, reserveUnit) ==
            MilitaryResult::Success
        );
        const auto unit=MilitarySystem::createUnit(world,actor,origin);
        PALADIN_CHECK(unit);
        const auto soldierId=world.army(unit)->soldiers().front();
        const auto personId=world.soldier(soldierId)->sourceCitizenId();
        auto& map=*sim.settlementMap(origin); auto& supply=*sim.settlementMap(other);
        const auto population=world.settlement(origin)->population()+world.settlement(other)->population();
        world.realm(actor)->treasury->balance=100000;
        map.commerce.treasury=world.realm(actor)->treasury;
        PALADIN_CHECK(MilitarySystem::orderMove(world,actor,unit,{42,32})==MilitaryResult::Success);
        PALADIN_CHECK(map.objectState().demolish(originalBarracks,{11,2}));
        PALADIN_CHECK(world.assignSettlementToRealm(origin,enemy));
        map.commerce.treasury=world.realm(enemy)->treasury;
        map.commerce.treasury->balance=50000;
        const auto originalEnemyCash=map.commerce.treasury->balance;
        const auto cashBefore=world.realm(actor)->treasury->balance+map.commerce.householdTotal();
        MilitarySystem::tick(world,360,360);
        PALADIN_CHECK(world.army(unit)->ownerRealmId()==actor);
        PALADIN_CHECK(world.army(unit)->soldierCount()==1 && world.soldier(soldierId));
        PALADIN_CHECK(world.army(unit)->position()==WorldTilePosition(42,32));
        PALADIN_CHECK(world.army(unit)->stationedSettlementId()==other);
        PALADIN_CHECK(MilitarySystem::canOrganize(world,*world.army(unit)));
        PALADIN_CHECK(map.commerce.treasury->balance==originalEnemyCash);
        PALADIN_CHECK(world.realm(actor)->treasury->balance<100000);
        PALADIN_CHECK(world.realm(actor)->treasury->balance+map.commerce.householdTotal()==cashBefore);
        const auto& person=*world.settlement(origin)->simulationState().citizens().citizen(personId);
        PALADIN_CHECK(person.militaryDeployed && !person.workplaceId && person.militaryUnitId==unit);
        PALADIN_CHECK(MilitarySystem::resizeUnit(world,actor,unit,1)==MilitaryResult::Success);
        PALADIN_CHECK(world.army(unit)->soldierCount()==2 && MilitarySystem::available(world,other)==0);
        // A mixed-origin roster can recruit locally. Disband returns each
        // original person as a civilian, without requiring barracks seats.
        const auto pack=supply.logistics.forObject(otherBarracks);
        PALADIN_CHECK(supply.logistics.add(pack,"rations",12,450));
        PALADIN_CHECK(MilitarySystem::orderMove(world,actor,unit,{43,32})==MilitaryResult::Success);
        PALADIN_CHECK(world.army(unit)->rations()==12);
        PALADIN_CHECK(!world.army(unit)->stationedSettlementId());
        MilitarySystem::tick(world,450,30);
        PALADIN_CHECK(world.army(unit)->position()==WorldTilePosition(43,32));
        PALADIN_CHECK(world.army(unit)->soldierCount()==2);
        PALADIN_CHECK(world.settlement(origin)->population()+world.settlement(other)->population()==population-2);
        // Land movement wraps by one adjacent tile across the world seam.
        PALADIN_CHECK(world.setArmyPosition(unit,{63,32}));
        PALADIN_CHECK(MilitarySystem::orderMove(world,actor,unit,{0,32})==MilitaryResult::Success);
        MilitarySystem::tick(world,480,Army::MarchMinutesPerTile*.5);
        PALADIN_CHECK(std::abs(world.army(unit)->visualX()-63.5)<1e-8);
        MilitarySystem::tick(world,482.5,Army::MarchMinutesPerTile*.5);
        PALADIN_CHECK(world.army(unit)->position()==WorldTilePosition(0,32));
        const auto originalRecords=world.settlement(origin)->simulationState().citizens().citizens().size();
        const auto supplyRecords=world.settlement(other)->simulationState().citizens().citizens().size();
        const auto remainingRations=world.army(unit)->rations();
        const auto storedRations=map.logistics.total("rations");
        PALADIN_CHECK(MilitarySystem::disbandUnit(world,enemy,unit)==MilitaryResult::NotOwned);
        PALADIN_CHECK(MilitarySystem::disbandUnit(world,actor,unit)==MilitaryResult::Success);
        PALADIN_CHECK(!world.army(unit) && !world.soldier(soldierId));
        PALADIN_CHECK(world.settlement(origin)->population()+world.settlement(other)->population()==population);
        PALADIN_CHECK(world.settlement(origin)->simulationState().citizens().citizens().size()==originalRecords);
        PALADIN_CHECK(world.settlement(other)->simulationState().citizens().citizens().size()==supplyRecords);
        const auto& returned=*world.settlement(origin)->simulationState().citizens().citizen(personId);
        PALADIN_CHECK(!returned.militaryDeployed && !returned.soldierId && !returned.militaryUnitId && !returned.workplaceId);
        PALADIN_CHECK(map.logistics.total("rations")==storedRations+remainingRations);
        MilitarySystem::synchronize(world,490);
        PALADIN_CHECK(!world.soldier(soldierId));
        PALADIN_CHECK(world.settlement(origin)->population()+world.settlement(other)->population()==population);

        std::cout<<"[military] free barracks seats, captured/demolished origin independence, mixed-city recruitment, field cash conservation and seam travel passed\n";
    }

}
void runMilitaryIndustryTests()
{
    testHouseholdFuel();
    testMining();
    testIndustry();
    testEmergencyFoodAndGathering();
    testUnits();
    testIndependentFieldForce();
}

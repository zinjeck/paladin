#include "TestFramework.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include "world/settlements/SettlementSimulationState.h"
#include "world/settlements/commands/SettlementCommandDefinition.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include "world/settlements/objects/jobs/fishery/FisheryJob.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>

using namespace Paladin;
namespace Paladin
{
struct SettlementActivityTestFixture
{
    static SettlementCitizen& resident(
        SettlementCitizenState& state,
        std::size_t index = 0
    )
    {
        return state.citizens_[index];
    }
};
} // namespace Paladin
namespace
{
SettlementMap land(int side = 40)
{
    SettlementGrid grid(side, side);
    for (int y = 0; y < side; ++y)
    {
        for (int x = 0; x < side; ++x)
        {
            grid.tile({x, y})->terrain = TerrainType::Land;
        }
    }
    return SettlementMap(std::move(grid), {0, 0}, 1, 1, side, 701);
}
SettlementObjectId completed(
    SettlementMap& map,
    std::string_view type,
    SettlementObjectFootprint footprint
)
{
    auto definition = *SettlementObjectCatalog::definition(type);
    definition.bypassesConstruction = true;
    PALADIN_CHECK(map.objectState()
                      .placeCompletedObject(map.grid(), definition, footprint));
    map.logistics.synchronize(map.objectState(), 0);
    return map.objectState().completedObjects().back().id;
}
void found(SettlementMap& map, SettlementCitizenState& citizens, int count = 8)
{
    PALADIN_CHECK(citizens.initialize(count, 91));
    completed(map, SettlementObjectTypes::CityKeep, {{2, 2}, 3, 7});
    citizens.placeUnpositionedCitizens(map);
}
void advance(
    SettlementMap& map,
    SettlementCitizenState& citizens,
    double minute,
    int duration
)
{
    map.activities.tick(map, citizens, minute, duration);
}
void emptyFood(SettlementMap& map)
{
    const auto keep = map.logistics.forObject(
        map.objectState().completedObjects().front().id
    );
    const int fish = map.logistics.available(keep, "fish");
    if (fish > 0)
    {
        PALADIN_CHECK(
            map.logistics.reserve(CitizenId{999999}, keep, {}, "fish", fish)
        );
        PALADIN_CHECK(map.logistics.pickUp(CitizenId{999999}));
        map.logistics.release(CitizenId{999999});
    }
}
double allGoods(
    const SettlementMap& map,
    const SettlementCitizenState& citizens,
    std::string_view resource
)
{
    double result = map.logistics.total(resource);
    for (const auto& c : citizens.citizens())
    {
        if (c.carriedResource == resource)
        {
            result += c.carriedAmount;
        }
    }
    return result;
}
} // namespace
void runSettlementSimulationLoopTests()
{
    std::cout << "Checking physical storage and exclusive reservations...\n";
    {
        auto map = land();
        SettlementCitizenState citizens;
        found(map, citizens);
        const auto keep = map.logistics.forObject(
            map.objectState().completedObjects().front().id
        );
        PALADIN_CHECK(map.logistics.inventory(keep)->used() == 100);
        PALADIN_CHECK(map.logistics.total("lumber") == 40);
        PALADIN_CHECK(map.logistics.total("stone") == 40);
        PALADIN_CHECK(map.logistics.total("fish") == 20);
        PALADIN_CHECK(!map.logistics.add(keep, "stone", 1));
        const auto stock = map.logistics.forObject(
            completed(map, SettlementObjectTypes::Stockpile, {{10, 10}, 2, 2})
        );
        PALADIN_CHECK(map.logistics.add(stock, "stone", 249));
        PALADIN_CHECK(
            map.logistics.reserve(CitizenId{1}, keep, stock, "fish", 1)
        );
        PALADIN_CHECK(
            !map.logistics.reserve(CitizenId{2}, keep, stock, "fish", 1)
        );
        PALADIN_CHECK(map.logistics.pickUp(CitizenId{1}));
        PALADIN_CHECK(map.logistics.freeSpace(stock) == 0);
        PALADIN_CHECK(map.logistics.deliver(CitizenId{1}));
        PALADIN_CHECK(map.logistics.total("fish") == 20);
        PALADIN_CHECK(map.logistics.inventory(stock)->used() == 250);
        map.logistics.synchronize(map.objectState(), 100);
        PALADIN_CHECK(map.logistics.total("lumber") == 40);
    }
    std::cout
        << "Checking gathering, individual road labor, and construction...\n";
    {
        auto map = land();
        SettlementCitizenState citizens;
        found(map, citizens);
        map.naturalFeatures().set({8, 5}, NaturalFeatureKind::Tree);
        map.naturalFeatures().set({8, 6}, NaturalFeatureKind::Rock);
        PALADIN_CHECK(map.commandState().add(
            map,
            SettlementCommandTypes::ChopTree,
            {{8, 5}, 1, 1},
            citizens
        ));
        PALADIN_CHECK(map.commandState().add(
            map,
            SettlementCommandTypes::CollectRock,
            {{8, 6}, 1, 1},
            citizens
        ));
        advance(map, citizens, 480, 120);
        PALADIN_CHECK(
            map.naturalFeatures().at({8, 5}).kind == NaturalFeatureKind::None
        );
        PALADIN_CHECK(
            map.naturalFeatures().at({8, 6}).kind == NaturalFeatureKind::None
        );
        PALADIN_CHECK(allGoods(map, citizens, "lumber") == 44);
        PALADIN_CHECK(allGoods(map, citizens, "stone") == 44);
        PALADIN_CHECK(map.objectState().createConstructionSites(
            map.grid(),
            *SettlementObjectCatalog::definition(SettlementObjectTypes::Road),
            {{7, 12}, 3, 1}
        ));
        advance(map, citizens, 600, 100);
        PALADIN_CHECK(map.objectState().constructionSites().empty());
        for (int x = 7; x < 10; ++x)
        {
            PALADIN_CHECK(map.objectState().completedObjectAt({x, 12}));
        }
        PALADIN_CHECK(allGoods(map, citizens, "lumber") == 44);
        PALADIN_CHECK(map.objectState().createConstructionSites(
            map.grid(),
            *SettlementObjectCatalog::definition(SettlementObjectTypes::House),
            {{12, 5}, 3, 3}
        ));
        advance(map, citizens, 700, 240);
        PALADIN_CHECK(map.objectState().constructionSites().empty());
        PALADIN_CHECK(map.objectState().completedObjectAt({12, 5}));
        PALADIN_CHECK(allGoods(map, citizens, "lumber") == 40);
        PALADIN_CHECK(
            std::count_if(
                citizens.citizens().begin(),
                citizens.citizens().end(),
                [](const auto& c) { return bool(c.homeId); }
            ) == 4
        );
        advance(map, citizens, 960, 160);
        for (const auto& c : citizens.citizens())
        {
            if (c.homeId)
            {
                PALADIN_CHECK(c.activity == CitizenActivity::AtHome);
            }
        }
        const auto lumber = allGoods(map, citizens, "lumber");
        PALADIN_CHECK(map.commandState().add(
            map,
            SettlementCommandTypes::Demolish,
            {{7, 12}, 1, 1},
            citizens
        ));
        advance(map, citizens, 1920, 80);
        PALADIN_CHECK(!map.objectState().completedObjectAt({7, 12}));
        PALADIN_CHECK(allGoods(map, citizens, "lumber") == lumber);
    }
    std::cout
        << "Checking hunger, physical pickup, starvation and recovery...\n";
    {
        auto map = land();
        SettlementCitizenState citizens;
        found(map, citizens, 1);
        auto& c = SettlementActivityTestFixture::resident(citizens);
        c.tilePosition = {25, 25};
        c.hunger = 50;
        advance(map, citizens, 480, 1);
        PALADIN_CHECK(c.hunger >= 50);
        PALADIN_CHECK(map.logistics.total("fish") == 20);
        advance(map, citizens, 481, 180);
        PALADIN_CHECK(c.hunger < 10);
        PALADIN_CHECK(map.logistics.total("fish") == 19);
        emptyFood(map);
        c.hunger = 75;
        c.health = 100;
        advance(map, citizens, 700, 360);
        PALADIN_CHECK(std::abs(c.hunger - 87.5) < 1e-6);
        PALADIN_CHECK(std::abs(c.health - 75) < 1e-6);
        PALADIN_CHECK(c.happiness < 100);
        c.carriedResource = "lumber";
        c.carriedAmount = 4;
        const double lumber = allGoods(map, citizens, "lumber");
        advance(map, citizens, 1060, 361);
        PALADIN_CHECK(citizens.citizens().empty());
        PALADIN_CHECK(map.logistics.total("lumber") == lumber);
        PALADIN_CHECK(!map.logistics.reservation(CitizenId{1}));
    }
    std::cout << "Checking unreachable-food fallback and exclusive fishery "
                 "zones...\n";
    {
        auto map = land(60);
        SettlementCitizenState citizens;
        found(map, citizens, 2);
        for (int y = 0; y < 60; ++y)
        {
            for (int x = 30; x < 60; ++x)
            {
                map.grid().tile({x, y})->terrain = TerrainType::Water;
            }
        }
        const auto fishery = completed(
            map,
            SettlementObjectTypes::FishingGrounds,
            {{24, 14}, 3, 3}
        );
        const auto originalWater =
            map.objectState().completedObject(fishery)->productionWater;
        PALADIN_CHECK(!originalWater.empty());
        const auto preview =
            fisheryZonePreview(map.grid(), map.objectState(), {{24, 19}, 3, 3});
        PALADIN_CHECK(!preview.excludedWater.empty());
        PALADIN_CHECK(!preview.availableWater.empty());
        const auto second = completed(
            map,
            SettlementObjectTypes::FishingGrounds,
            {{24, 19}, 3, 3}
        );
        for (auto tile :
             map.objectState().completedObject(second)->productionWater)
        {
            PALADIN_CHECK(
                std::find(originalWater.begin(), originalWater.end(), tile) ==
                originalWater.end()
            );
        }
        map.employment().synchronize(map.objectState(), citizens);
        const auto job = map.employment().forObject(fishery);
        PALADIN_CHECK(map.employment().workplace(job)->capacity == 0);
        PALADIN_CHECK(map.employment().adjust(job, 1, citizens));
        emptyFood(map);
        auto& worker = SettlementActivityTestFixture::resident(citizens);
        worker.hunger = 60;
        advance(map, citizens, 480, 350);
        PALADIN_CHECK(worker.health > 0 && worker.workplaceId == job);
        PALADIN_CHECK(worker.hunger < 50);
        PALADIN_CHECK(worker.task.kind == CitizenTaskKind::Work);
        PALADIN_CHECK(map.logistics.total("fish") > 0);
        advance(map, citizens, 960, 2);
        PALADIN_CHECK(worker.task.kind != CitizenTaskKind::Work);
    }
    std::cout << "Checking cancellation preserves delivered and carried "
                 "materials...\n";
    {
        auto map = land();
        SettlementCitizenState citizens;
        found(map, citizens, 2);
        PALADIN_CHECK(map.objectState().createConstructionSites(
            map.grid(),
            *SettlementObjectCatalog::definition(SettlementObjectTypes::House),
            {{15, 14}, 3, 3}
        ));
        bool carrying = false;
        double minute = 480;
        for (; minute < 580 && !carrying; ++minute)
        {
            advance(map, citizens, minute, 1);
            carrying = std::any_of(
                citizens.citizens().begin(),
                citizens.citizens().end(),
                [](const auto& c) { return c.carriedAmount > 0; }
            );
        }
        PALADIN_CHECK(carrying);
        PALADIN_CHECK(
            map.commandState()
                .cancelIntersecting(map, {{15, 14}, 3, 3}, citizens) == 1
        );
        advance(map, citizens, minute, 1);
        PALADIN_CHECK(allGoods(map, citizens, "lumber") == 40);
        PALADIN_CHECK(map.objectState().constructionSites().empty());
        for (const auto& c : citizens.citizens())
        {
            PALADIN_CHECK(c.carriedAmount == 0);
        }
    }
    std::cout
        << "Checking assigned-stockpile ownership and unemployed fallback...\n";
    {
        auto map = land();
        SettlementCitizenState citizens;
        found(map, citizens, 1);
        emptyFood(map);
        const auto keep = map.logistics.forObject(
            map.objectState().completedObjects().front().id
        );
        const auto storeObject =
            completed(map, SettlementObjectTypes::Stockpile, {{12, 12}, 2, 2});
        const auto store = map.logistics.forObject(storeObject);
        PALADIN_CHECK(map.logistics.add(store, "stone", 250));
        map.employment().synchronize(map.objectState(), citizens);
        const auto job = map.employment().forObject(storeObject);
        PALADIN_CHECK(map.employment().adjust(job, 1, citizens));
        map.logistics.drop({10, 12}, "lumber", 4, 480);
        advance(map, citizens, 480, 180);
        PALADIN_CHECK(map.logistics.inventory(keep)->amount("lumber") == 40);
        PALADIN_CHECK(map.logistics.inventory(store)->used() == 250);
        PALADIN_CHECK(map.logistics.total("lumber") == 44);
        PALADIN_CHECK(citizens.spawn(1));
        advance(map, citizens, 660, 180);
        PALADIN_CHECK(map.logistics.inventory(keep)->amount("lumber") == 44);
        PALADIN_CHECK(map.logistics.inventory(store)->used() == 250);
        // Once its own storage has room, its employee delivers there, even
        // when the keep is a closer possible destination.
        PALADIN_CHECK(
            map.logistics.reserve(CitizenId{999999}, store, {}, "stone", 4)
        );
        PALADIN_CHECK(map.logistics.pickUp(CitizenId{999999}));
        map.logistics.release(CitizenId{999999});
        map.logistics.drop({9, 11}, "lumber", 4, 840);
        advance(map, citizens, 840, 115);
        PALADIN_CHECK(map.logistics.inventory(store)->amount("lumber") == 4);
        PALADIN_CHECK(map.logistics.inventory(keep)->amount("lumber") == 44);
    }
    std::cout
        << "Checking distant construction materials and fed recovery...\n";
    {
        auto map = land(80);
        SettlementCitizenState citizens;
        found(map, citizens, 1);
        const auto keep = map.logistics.forObject(
            map.objectState().completedObjects().front().id
        );
        PALADIN_CHECK(
            map.logistics.reserve(CitizenId{999999}, keep, {}, "lumber", 40)
        );
        PALADIN_CHECK(map.logistics.pickUp(CitizenId{999999}));
        map.logistics.release(CitizenId{999999});
        const auto pile = map.logistics.drop({53, 8}, "lumber", 4, 480);
        advance(map, citizens, 480, 90);
        PALADIN_CHECK(map.logistics.inventory(pile)->amount("lumber") == 4);
        PALADIN_CHECK(map.objectState().createConstructionSites(
            map.grid(),
            *SettlementObjectCatalog::definition(SettlementObjectTypes::House),
            {{51, 12}, 3, 3}
        ));
        advance(map, citizens, 570, 370);
        PALADIN_CHECK(map.objectState().completedObjectAt({51, 12}));
        PALADIN_CHECK(allGoods(map, citizens, "lumber") == 0);
        auto& c = SettlementActivityTestFixture::resident(citizens);
        c.health = 30;
        c.hunger = 0;
        advance(map, citizens, 940, 500);
        PALADIN_CHECK(c.health > 38 && c.health < 40);
    }
}

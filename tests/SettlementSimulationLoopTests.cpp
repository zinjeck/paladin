#include "TestFramework.h"
#include "interaction/SettlementCommandController.h"
#include "world/generation/GenerationNoise.h"
#include <limits>
#include "interaction/SettlementObjectPlacementController.h"
#include "rendering/NaturalSurfaceShape.h"
#include "rendering/ScenePresentation.h"
#include "world/Season.h"
#include "world/settlements/SettlementHomeBeds.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include "world/settlements/SettlementSimulationState.h"
#include "world/settlements/commands/SettlementCommandDefinition.h"
#include "world/settlements/objects/SettlementDoor.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include "world/settlements/objects/jobs/LoggingGroundsJob.h"
#include "world/settlements/objects/jobs/fishery/FisheryJob.h"
#include "world/settlements/objects/jobs/market/MarketJob.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <set>

using namespace Paladin;
namespace Paladin
{
    struct SettlementActivityTestFixture
    {
        static bool routeWithoutPathSearch(
            SettlementMap& map,
            SettlementCitizenState& citizens,
            SettlementCitizen& citizen,
            SettlementTilePosition goal
        )
        {
            map.activities.pathsRemaining_ = 0;
            return map.activities.route(map, citizens, citizen, {goal, 1, 1}, true);
        }
        static bool enterHome(
            SettlementMap& map,
            const SettlementCitizenState& citizens,
            SettlementCitizen& citizen
        )
        {
            return map.activities.enterHome(map, citizens, citizen);
        }
        static void executeWithoutPathSearch(
            SettlementMap& map,
            SettlementCitizenState& citizens,
            SettlementCitizen& citizen,
            double minute
        )
        {
            map.activities.pathsRemaining_ = 0;
            map.activities.execute(map, citizens, citizen, minute, 1);
        }
        static void needs(
            SettlementMap& map,
            SettlementCitizen& c,
            double elapsed,
            double minute
        )
        {
            map.activities.needs(map, c, elapsed, minute);
        }
        static void produce(
            SettlementMap& map,
            const SettlementCitizenState& citizens,
            double minute,
            double elapsed
        )
        {
            map.activities.produce(map, citizens, minute, elapsed);
        }
        static void rematch(SettlementCitizenState& state)
        {
            for (auto& c : state.citizens_)
            {
                c.spouseId = {};
            }
            ++state.familyVersion_;
            state.matchSingles();
        }

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
        PALADIN_CHECK(map.objectState().placeCompletedObject(
            map.grid(),
            definition,
            footprint
        ));
        map.logistics.synchronize(map.objectState(), 0);
        return map.objectState().completedObjects().back().id;
    }
    void found(
        SettlementMap& map,
        SettlementCitizenState& citizens,
        int count = 8
    )
    {
        PALADIN_CHECK(citizens.initialize(count, 91));
        // Existing monetary scenarios explicitly start after gold is
        // introduced.
        map.commerce.treasury->balance = 100000;
        map.commerce.policy.startingSavings = 600;
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
    // Audit regression: cancellation preserves goods and their real creation
    // time through both the state API and the player command controller.
    for (const bool throughController : {false, true})
    {
        auto map = land();
        SettlementCitizenState citizens;
        found(map, citizens, 1);
        constexpr double minute = 23 * 1440 + 610.25;
        const auto& house = *SettlementObjectCatalog::definition(
            SettlementObjectTypes::House
        );
        PALADIN_CHECK(map.objectState().createConstructionSites(
            map.grid(), house, {{15, 14}, 3, 3}
        ));
        const auto site = map.objectState().constructionSites().back().id;
        map.logistics.synchronize(map.objectState(), minute - 10);
        const auto inventory = map.logistics.forSite(site);
        PALADIN_CHECK(inventory);
        PALADIN_CHECK(map.logistics.add(inventory, "lumber", 3, minute - 10));
        PALADIN_CHECK(map.logistics.add(inventory, "stone", 1, minute - 10));
        PALADIN_CHECK(map.objectState().deliverMaterials(site, "lumber", 3));
        PALADIN_CHECK(map.objectState().deliverMaterials(site, "stone", 1));
        const auto lumber = allGoods(map, citizens, "lumber");
        const auto stone = allGoods(map, citizens, "stone");
        if (throughController)
        {
            SettlementCommandController controller;
            PALADIN_CHECK(controller.begin(SettlementCommandTypes::Cancel));
            controller.pointerPressed(SettlementTilePosition{15, 14});
            PALADIN_CHECK(controller.pointerReleased(
                SettlementTilePosition{17, 16}, map, citizens, minute
            ));
        }
        else
        {
            PALADIN_CHECK(map.commandState().cancelIntersecting(
                map, {{15, 14}, 3, 3}, citizens, minute
            ) == 1);
        }
        PALADIN_CHECK(!map.objectState().constructionSite(site));
        PALADIN_CHECK(!map.logistics.forSite(site));
        PALADIN_CHECK(allGoods(map, citizens, "lumber") == lumber);
        PALADIN_CHECK(allGoods(map, citizens, "stone") == stone);
        int recoveredLumber = 0, recoveredStone = 0;
        for (const auto& pile : map.logistics.inventories())
        {
            if (pile.kind != InventoryKind::Groundpile)
            {
                continue;
            }
            PALADIN_CHECK(pile.createdMinute == minute);
            PALADIN_CHECK(minute - pile.createdMinute <
                          map.activities.policy.stockpile.employeePreferenceMinutes);
            recoveredLumber += pile.amount("lumber");
            recoveredStone += pile.amount("stone");
        }
        PALADIN_CHECK(recoveredLumber == 3 && recoveredStone == 1);
        PALADIN_CHECK(map.commandState().cancelIntersecting(
            map, {{15, 14}, 3, 3}, citizens, minute + 1
        ) == 0);
        PALADIN_CHECK(allGoods(map, citizens, "lumber") == lumber);
        PALADIN_CHECK(allGoods(map, citizens, "stone") == stone);
    }
    std::cout << "[audit] cancellation timestamps and conservation passed\n";

    // Both death paths must clear relationships before dense-vector erasure.
    // Exercise first, middle and last residents, then verify cleanup is not
    // repeated and carried resources are returned exactly once.
    for (const bool alreadyDead : {false, true})
    {
        for (const std::size_t victimIndex : {std::size_t(0), std::size_t(1), std::size_t(3)})
        {
            auto map = land();
            SettlementCitizenState citizens;
            found(map, citizens, 4);
            map.activities.policy.decisionsPerMinute = 0;
            map.activities.policy.dailyBirthChance = 0;
            citizens.idlePolicy.decisionsPerTick = 0;
            for (std::size_t i = 0; i < citizens.citizens().size(); ++i)
            {
                auto& resident = SettlementActivityTestFixture::resident(citizens, i);
                resident.health = 100;
                resident.hunger = 0;
                resident.energy = 100;
                for (const auto& other : citizens.citizens())
                {
                    if (resident.id != other.id)
                    {
                        resident.familiarities[other.id] = 17;
                    }
                }
            }
            auto& victim = SettlementActivityTestFixture::resident(citizens, victimIndex);
            const auto victimId = victim.id;
            const auto keep = map.logistics.forObject(
                map.objectState().completedObjects().front().id
            );
            PALADIN_CHECK(map.logistics.reserve(victimId, keep, {}, "lumber", 2));
            PALADIN_CHECK(map.logistics.pickUp(victimId));
            victim.carriedResource = "lumber";
            victim.carriedAmount = 2;
            victim.hunger = 100;
            victim.health = alreadyDead ? 0 : .1;
            const auto before = allGoods(map, citizens, "lumber");
            advance(map, citizens, 600, 1);
            // Never retain a reference into the compacted citizen vector.
            PALADIN_CHECK(citizens.citizen(victimId) == nullptr);
            PALADIN_CHECK(citizens.citizens().size() == 3);
            PALADIN_CHECK(map.logistics.reservation(victimId) == nullptr);
            PALADIN_CHECK(allGoods(map, citizens, "lumber") == before);
            for (const auto& survivor : citizens.citizens())
            {
                PALADIN_CHECK(!survivor.familiarities.contains(victimId));
                PALADIN_CHECK(survivor.familiarities.size() == 2);
                for (const auto& other : citizens.citizens())
                {
                    if (survivor.id != other.id)
                    {
                        PALADIN_CHECK(survivor.familiarityWith(other.id) == 17);
                    }
                }
            }
            advance(map, citizens, 601, 1);
            PALADIN_CHECK(citizens.citizens().size() == 3);
            PALADIN_CHECK(allGoods(map, citizens, "lumber") == before);
        }
    }
    std::cout << "[audit] both death paths and cargo cleanup passed\n";

    // Rendering/capture must stay finite even for malformed timing, without
    // changing ordinary interpolation or accessing an exhausted path.
    {
        SettlementCitizen c;
        c.tilePosition = {10, 20};
        c.path = {{11, 20}, {12, 22}};
        c.pathIndex = 1;
        c.stepDuration = 2;
        c.stepProgress = .5;
        PALADIN_CHECK(c.visualX() == 10.5 && c.visualY() == 20.5);
        c.stepProgress = -1;
        PALADIN_CHECK(c.visualX() == 10 && c.visualY() == 20);
        c.stepProgress = 3;
        PALADIN_CHECK(c.visualX() == 12 && c.visualY() == 22);
        for (const double duration : {
                 0.0, -1.0, std::numeric_limits<double>::infinity(),
                 std::numeric_limits<double>::quiet_NaN()})
        {
            c.stepDuration = duration;
            for (const double progress : {0.0, .5})
            {
                c.stepProgress = progress;
                PALADIN_CHECK(std::isfinite(c.visualX()) && std::isfinite(c.visualY()));
                PALADIN_CHECK(c.visualX() == 10 && c.visualY() == 20);
            }
        }
        c.stepDuration = 1;
        for (const double progress : {
                 std::numeric_limits<double>::infinity(),
                 -std::numeric_limits<double>::infinity(),
                 std::numeric_limits<double>::quiet_NaN()})
        {
            c.stepProgress = progress;
            PALADIN_CHECK(c.visualX() == 10 && c.visualY() == 20);
        }
        c.stepDuration = 0;
        c.pathIndex = c.path.size();
        PALADIN_CHECK(c.visualX() == 10 && c.visualY() == 20);
        c.path.clear();
        c.pathIndex = 0;
        PALADIN_CHECK(c.visualX() == 10 && c.visualY() == 20);
    }
    std::cout << "[audit] finite and ordinary interpolation passed\n";

    // Direct home, childcare and bed paths initialize their actual first-edge
    // cost, including non-default diagonal costs, before the next movement tick.
    for (const auto task : {CitizenTaskKind::Home, CitizenTaskKind::Care,
                           CitizenTaskKind::Sleep})
    {
        auto map = land();
        SettlementCitizenState citizens;
        found(map, citizens, 1);
        const auto home = completed(map, SettlementObjectTypes::House, {{12, 12}, 3, 3});
        auto& c = SettlementActivityTestFixture::resident(citizens);
        c.tilePosition = {13, 13};
        c.destination = c.tilePosition;
        c.homeId = home;
        c.insideHome = true;
        c.path.clear();
        c.pathIndex = 0;
        c.stepProgress = 0;
        c.stepDuration = 99;
        c.task = {};
        c.task.kind = task;
        c.task.object = home;
        c.task.target = {14, 14};
        c.youngDependents = task == CitizenTaskKind::Care ? 1 : 0;
        c.nextHomeWander = 0;
        c.bedHomeId = {};
        c.bedSlot = -1;
        citizens.movementPolicy.diagonalCost = 1.75;
        map.activities.policy.leisureRadius = 1;
        // Select a nonzero offset deterministically. Home may use an
        // interior fallback or a zero-search exit; both must initialize timing.
        for (std::uint64_t sequence = 0; sequence < 100; ++sequence)
        {
            const auto random = GenerationNoise::mix(c.id.value() ^ (sequence + 1));
            if (random % 3 != 1 || (random >> 8) % 3 != 1)
            {
                c.choiceSequence = sequence;
                break;
            }
        }
        SettlementActivityTestFixture::executeWithoutPathSearch(map, citizens, c, 600);
        PALADIN_CHECK(!c.path.empty());
        PALADIN_CHECK(c.pathIndex == 0 && c.stepProgress == 0);
        const auto expected = citizens.navigationDiagnostics().stepCost(
            map, c.tilePosition, c.path.front(), citizens.movementPolicy
        );
        PALADIN_CHECK(c.stepDuration == expected);
        PALADIN_CHECK(c.stepDuration > 0 && c.stepDuration < 99);
        PALADIN_CHECK(c.visualX() == 13 && c.visualY() == 13);
    }
    std::cout << "[audit] direct interior path timing passed\n";

    // A blocked exit forces Home's one-tile interior fallback specifically.
    {
        auto map = land();
        SettlementCitizenState citizens;
        found(map, citizens, 1);
        const auto homeId = completed(map, SettlementObjectTypes::House, {{12, 12}, 3, 3});
        const auto* home = map.objectState().completedObject(homeId);
        PALADIN_CHECK(home && home->door);
        map.grid().tile(outsideDoor(home->footprint, *home->door))->terrain = TerrainType::Water;
        auto& c = SettlementActivityTestFixture::resident(citizens);
        c.homeId = homeId;
        c.insideHome = true;
        c.tilePosition = {13, 13};
        c.destination = c.tilePosition;
        c.task.kind = CitizenTaskKind::Home;
        c.task.object = homeId;
        c.stepDuration = 99;
        for (std::uint64_t sequence = 0; sequence < 100; ++sequence)
        {
            const auto r = GenerationNoise::mix(c.id.value() ^ (sequence + 1));
            if (r % 3 != 1 || (r >> 8) % 3 != 1)
            {
                c.choiceSequence = sequence;
                break;
            }
        }
        SettlementActivityTestFixture::executeWithoutPathSearch(map, citizens, c, 600);
        PALADIN_CHECK(c.path.size() == 1);
        PALADIN_CHECK(home->footprint.contains(c.path.front()));
        PALADIN_CHECK(c.stepDuration == citizens.navigationDiagnostics().stepCost(
            map, c.tilePosition, c.path.front(), citizens.movementPolicy
        ));
    }
    // Door-entry and prepended-exit paths use the first physical edge, while
    // a route replacement halfway through a step preserves its progress.
    {
        auto map = land();
        SettlementCitizenState citizens;
        found(map, citizens, 1);
        const auto homeId = completed(map, SettlementObjectTypes::House, {{12, 12}, 3, 3});
        const auto* home = map.objectState().completedObject(homeId);
        PALADIN_CHECK(home && home->door);
        const auto outside = outsideDoor(home->footprint, *home->door);
        auto& c = SettlementActivityTestFixture::resident(citizens);
        c.homeId = homeId;
        c.tilePosition = outside;
        c.destination = outside;
        c.stepDuration = 99;
        PALADIN_CHECK(!SettlementActivityTestFixture::enterHome(map, citizens, c));
        PALADIN_CHECK(c.path.size() == 1 && c.path.front() == *home->door);
        PALADIN_CHECK(c.stepDuration == 1);
        c.tilePosition = {13, 13};
        c.destination = c.tilePosition;
        c.insideHome = true;
        c.path.clear();
        c.pathIndex = 0;
        c.stepProgress = 0;
        c.stepDuration = 99;
        PALADIN_CHECK(SettlementActivityTestFixture::routeWithoutPathSearch(
            map, citizens, c, outside
        ));
        PALADIN_CHECK(!c.path.empty() && c.path.back() == outside);
        PALADIN_CHECK(c.stepDuration == citizens.navigationDiagnostics().stepCost(
            map, c.tilePosition, c.path.front(), citizens.movementPolicy
        ));
        citizens.movementPolicy.diagonalCost = 1.75;
        c.path = {{14, 14}};
        c.pathIndex = 0;
        c.stepDuration = 1.75;
        c.stepProgress = .7;
        const auto x = c.visualX(), y = c.visualY();
        PALADIN_CHECK(SettlementActivityTestFixture::routeWithoutPathSearch(
            map, citizens, c, outside
        ));
        PALADIN_CHECK(c.path.front().x == 14 && c.path.front().y == 14);
        PALADIN_CHECK(c.stepProgress == .7 && c.stepDuration == 1.75);
        PALADIN_CHECK(std::abs(c.visualX() - x) < 1e-9);
        PALADIN_CHECK(std::abs(c.visualY() - y) < 1e-9);
    }
    std::cout << "[audit] entry, exit, fallback and mid-step rerouting passed\n";
    {
        auto map = land();
        const auto home =
            completed(map, SettlementObjectTypes::House, {{12, 12}, 3, 3});
        const auto inventory = map.logistics.forObject(home);
        PALADIN_CHECK(
            map.logistics.inventory(inventory)->kind == InventoryKind::Home
        );
        PALADIN_CHECK(!map.logistics.add(inventory, "stone", 1));
        PALADIN_CHECK(map.logistics.add(inventory, "lumber", 8));
        const std::unordered_set<SettlementObjectId, StrongIdHash> occupied{
            home
        };
        map.heating.advance(map.logistics, {}, 0, 1440);
        PALADIN_CHECK(
            map.logistics.inventory(inventory)->amount("lumber") == 8
        );
        PALADIN_CHECK(!map.heating.heated(home));
        map.heating.advance(map.logistics, occupied, 0, 1440);
        PALADIN_CHECK(
            map.logistics.inventory(inventory)->amount("lumber") == 6
        );
        PALADIN_CHECK(map.heating.heated(home));
        map.heating.advance(map.logistics, occupied, 9 * 1440, 1440);
        PALADIN_CHECK(
            map.logistics.inventory(inventory)->amount("lumber") == 2
        );
        PALADIN_CHECK(map.heating.heated(home));
        map.heating.advance(map.logistics, occupied, 10 * 1440, 1440);
        PALADIN_CHECK(
            map.logistics.inventory(inventory)->amount("lumber") == 0
        );
        PALADIN_CHECK(!map.heating.heated(home));
        PALADIN_CHECK(std::abs(map.heating.coldFraction(home) - .5) < 1e-9);
        map.heating.advance(map.logistics, occupied, 10 * 1440, 60);
        SettlementCitizen cold;
        cold.homeId = home;
        cold.hunger = 0;
        cold.energy = 100;
        cold.health = 80;
        cold.happiness = 80;
        auto warm = cold;
        SettlementActivityTestFixture::needs(map, cold, 60, 10 * 1440);
        PALADIN_CHECK(cold.health < 80 && cold.happiness < 80);
        PALADIN_CHECK(map.logistics.add(inventory, "lumber", 1));
        map.heating.advance(map.logistics, occupied, 10 * 1440, 60);
        SettlementActivityTestFixture::needs(map, warm, 60, 10 * 1440);
        PALADIN_CHECK(
            warm.health > cold.health && warm.happiness > cold.happiness
        );
        // Reservations cannot be consumed by a fire before physical delivery.
        PALADIN_CHECK(map.logistics.add(inventory, "lumber", 1));
        PALADIN_CHECK(
            map.logistics.reserve(CitizenId{1000}, inventory, {}, "lumber", 1)
        );
        PALADIN_CHECK(!map.logistics.consumeAvailable(inventory, "lumber", 1));
        map.logistics.release(CitizenId{1000});
    }
    {
        auto map = land();
        map.naturalFeatures().set({12, 12}, NaturalFeatureKind::Tree);
        map.naturalFeatures().set({18, 18}, NaturalFeatureKind::Tree);
        map.naturalFeatures().set({25, 25}, NaturalFeatureKind::Rock);
        map.naturalFeatures().harvest({12, 12}, 100);
        map.naturalFeatures().harvest({18, 18}, 100);
        map.naturalFeatures().harvest({25, 25}, 100);
        map.naturalFeatures()
            .regrow(map.grid(), map.objectState(), 100 + 11 * 1440);
        PALADIN_CHECK(
            map.naturalFeatures().at({12, 12}).kind == NaturalFeatureKind::None
        );
        completed(map, SettlementObjectTypes::LoggingGrounds, {{18, 18}, 3, 3});
        map.naturalFeatures()
            .regrow(map.grid(), map.objectState(), 100 + 21 * 1440);
        PALADIN_CHECK(
            map.naturalFeatures().at({12, 12}).kind == NaturalFeatureKind::Tree
        );
        PALADIN_CHECK(
            map.naturalFeatures().at({18, 18}).kind == NaturalFeatureKind::None
        );
        PALADIN_CHECK(
            map.naturalFeatures().at({25, 25}).kind == NaturalFeatureKind::None
        );
    }
    {
        // An employed resident must finish both legs of a firewood delivery;
        // assigning a home or having wood elsewhere cannot instantly heat it.
        auto map = land();
        SettlementCitizenState citizens;
        found(map, citizens, 1);
        const auto home =
            completed(map, SettlementObjectTypes::House, {{12, 12}, 3, 3});
        const auto grounds = completed(
            map,
            SettlementObjectTypes::LoggingGrounds,
            {{18, 12}, 3, 3}
        );
        map.employment().synchronize(map.objectState(), citizens);
        const auto job = map.employment().forObject(grounds);
        PALADIN_CHECK(map.employment().adjust(job, 1, citizens));
        auto& c = SettlementActivityTestFixture::resident(citizens);
        c.hunger = 0;
        c.energy = 100;
        advance(map, citizens, 600, 1);
        PALADIN_CHECK(!map.heating.heated(home));
        PALADIN_CHECK(
            map.logistics.inventory(map.logistics.forObject(home))
                ->amount("lumber") == 0
        );
        advance(map, citizens, 601, 180);
        PALADIN_CHECK(map.heating.heated(home));
        PALADIN_CHECK(
            map.logistics.inventory(map.logistics.forObject(home))
                ->amount("lumber") > 0
        );
        PALADIN_CHECK(c.workplaceId == job && map.heating.lumberBurned() > 0);
    }
    {
        auto map = land();
        SettlementCitizenState citizens;
        found(map, citizens, 2);
        const auto grounds = completed(
            map,
            SettlementObjectTypes::LoggingGrounds,
            {{12, 12}, 6, 3}
        );
        map.employment().synchronize(map.objectState(), citizens);
        const auto job = map.employment().forObject(grounds);
        PALADIN_CHECK(map.employment().workplace(job)->maximumCapacity == 2);
        for (std::size_t i = 0; i < 2; ++i)
        {
            auto& c = SettlementActivityTestFixture::resident(citizens, i);
            c.workplaceId = job;
            c.child = false;
            c.youngDependents = 0;
            c.tilePosition = c.destination = {13 + int(i) * 3, 14};
            c.path.clear();
            c.task.kind = CitizenTaskKind::Work;
            c.task.object = grounds;
            c.breakUntil = 0;
        }
        SettlementActivityTestFixture::produce(map, citizens, 600, 30);
        const auto inventory = map.logistics.forObject(grounds);
        PALADIN_CHECK(
            map.logistics.inventory(inventory)->amount("lumber") == 2
        );
        // Production persists without any harvestable trees, regardless of
        // which presentation phase its decorative trees happen to be in.
        PALADIN_CHECK(
            loggingTreeGrowth(0, 0, true) != loggingTreeGrowth(18, 0, true)
        );
        SettlementActivityTestFixture::produce(map, citizens, 630, 30);
        PALADIN_CHECK(
            map.logistics.inventory(inventory)->amount("lumber") == 4
        );
        SettlementActivityTestFixture::resident(citizens, 1).task = {};
        SettlementActivityTestFixture::produce(map, citizens, 660, 30);
        PALADIN_CHECK(
            map.logistics.inventory(inventory)->amount("lumber") == 5
        );
        PALADIN_CHECK(
            loggingProductionPerMinute(9, 4) == loggingProductionPerMinute(9, 1)
        );
        PALADIN_CHECK(loggingProductionPerMinute(18, 0) == 0);
        PALADIN_CHECK(loggingTreeGrowth(18, 0, false) == 1);
    }
    {
        // Continuous material masks agree across adjoining tiles and cut
        // convex corners instead of exposing square cells.
        const auto road = [](int x, int y)
        { return y == 0 && x >= 0 && x <= 2; };
        PALADIN_CHECK(surfaceField(1, .5, road) == 1);
        PALADIN_CHECK(surfaceField(.05, .05, road) < .5);
        PALADIN_CHECK(
            std::abs(
                surfaceField(1 - 1e-7, .3, road) -
                surfaceField(1 + 1e-7, .3, road)
            ) < 1e-6
        );
    }
    {
        auto map = land();
        SettlementCitizenState people;
        found(map, people, 1);
        const auto pasture = completed(
            map,
            SettlementObjectTypes::Pastureland,
            {{12, 12}, 4, 4}
        );
        for (const auto order : {AnimalOrder::Gather, AnimalOrder::Hunt})
        {
            const auto id = map.animals.spawn(map, "cow", {28, 28});
            PALADIN_CHECK(id);
            map.animals.designate({{28, 28}, 1, 1}, order);
            auto& person = SettlementActivityTestFixture::resident(people);
            person.task.kind = CitizenTaskKind::AnimalWork;
            person.task.animal = id;
            PALADIN_CHECK(map.animals.reserve(id, person.id, pasture, map));
            std::set<std::pair<int, int>> positions;
            for (int minute = 0; minute < 50; ++minute)
            {
                map.animals.tick(map, people, minute, 1);
                const auto p = map.animals.find(id)->tilePosition;
                positions.emplace(p.x, p.y);
            }
            PALADIN_CHECK(positions.size() > 1);
            // Interrupted transport establishes a valid local roaming area.
            auto* animal = map.animals.find(id);
            animal->beingLed = true;
            animal->tilePosition = animal->previousTile = {8, 30};
            animal->visualProgress = 1;
            map.animals.release(person.id);
            PALADIN_CHECK(animal->herdCenter == animal->tilePosition);
            const auto start = animal->tilePosition;
            map.animals.tick(map, people, 100, 1);
            PALADIN_CHECK(animal->tilePosition != start);
        }
    }
    {
        auto map = land();
        SettlementCitizenState people;
        found(map, people, 1);
        const auto pasture = completed(
            map,
            SettlementObjectTypes::Pastureland,
            {{12, 20}, 4, 4}
        );
        completed(map, SettlementObjectTypes::House, {{20, 23}, 3, 3});
        const auto id = map.animals.spawn(map, "cow", {30, 25});
        PALADIN_CHECK(id);
        map.animals.designate({{30, 25}, 1, 1}, AnimalOrder::Gather);
        for (int minute = 360; minute < 660 && !map.animals.find(id)->pasture;
             ++minute)
        {
            advance(map, people, minute, 1);
        }
        PALADIN_CHECK(map.animals.find(id)->pasture == pasture);
        PALADIN_CHECK(!map.animals.find(id)->beingLed);
    }
    {
        auto map = land(60);
        SettlementCitizenState citizens;
        found(map, citizens, 2);
        const auto small = completed(
            map,
            SettlementObjectTypes::Pastureland,
            {{12, 12}, 4, 4}
        );
        const auto large = completed(
            map,
            SettlementObjectTypes::Pastureland,
            {{30, 12}, 8, 8}
        );
        for (auto [pasture, x] : {std::pair{small, 12}, std::pair{large, 30}})
        {
            for (int i = 0; i < 5; ++i)
            {
                const auto animal =
                    map.animals.spawn(map, "cow", {x + i % 3, 12 + i / 3});
                PALADIN_CHECK(animal);
                map.animals.find(animal)->pasture = pasture;
            }
            // Five cows jointly make their first unit before any individual
            // cow could have made a whole unit under the old accumulator.
            map.animals.produce(map, pasture, 1, 360, 48);
            PALADIN_CHECK(
                map.logistics
                    .available(map.logistics.forObject(pasture), "meat") == 1
            );
            map.animals.produce(map, pasture, 1, 408, 672);
            PALADIN_CHECK(
                map.logistics
                    .available(map.logistics.forObject(pasture), "meat") == 15
            );
        }
        PALADIN_CHECK(map.animals.capacity({{0, 0}, 4, 6}) == 21);
        PALADIN_CHECK(map.animals.capacity({{0, 0}, 2, 2}) == 4);
    }
    {
        auto map = land(50);
        SettlementCitizenState citizens;
        found(map, citizens, 3);
        auto& mother = SettlementActivityTestFixture::resident(citizens, 0);
        auto& father = SettlementActivityTestFixture::resident(citizens, 1);
        auto& child = SettlementActivityTestFixture::resident(citizens, 2);
        mother.sex = CitizenSex::Female;
        father.sex = CitizenSex::Male;
        child.child = true;
        child.ageYears = 2;
        child.ageMinutes =
            map.activities.policy.childMaturationMinutes * 2 / 16;
        child.motherId = mother.id;
        child.fatherId = father.id;
        SettlementActivityTestFixture::rematch(citizens);
        const auto home =
            completed(map, SettlementObjectTypes::House, {{8, 8}, 3, 3});
        map.activities.synchronizeHomes(map, citizens);
        map.activities.policy.dailyBirthChance = 0;
        child.tilePosition = child.destination = {20, 12};
        child.insideHome = false;
        mother.tilePosition = mother.destination = {9, 9};
        mother.insideHome = true;
        father.tilePosition = father.destination = {9, 9};
        father.insideHome = true;
        father.energy = 0;
        advance(map, citizens, 360, 60);
        PALADIN_CHECK(child.homeId == home && child.insideHome);
        advance(map, citizens, 420, 60);
        PALADIN_CHECK(child.insideHome);
        std::set<std::pair<int, int>> parentPositions, childPositions;
        for (int minute = 480; minute < 500; ++minute)
        {
            advance(map, citizens, minute, 1);
            parentPositions.emplace(
                mother.tilePosition.x,
                mother.tilePosition.y
            );
            childPositions.emplace(child.tilePosition.x, child.tilePosition.y);
            PALADIN_CHECK(mother.insideHome && child.insideHome);
        }
        PALADIN_CHECK(parentPositions.size() > 1 && childPositions.size() > 1);
        SettlementFamilySystem family;
        child.health = child.happiness = 60;
        mother.task.kind = CitizenTaskKind::Care;
        mother.insideHome = true;
        family.update(
            map,
            citizens,
            map.activities.policy,
            map.activities,
            501,
            1
        );
        PALADIN_CHECK(child.health > 60 && child.happiness > 60);
        mother.insideHome = father.insideHome = false;
        const auto caredHealth = child.health;
        const auto caredHappiness = child.happiness;
        family.update(
            map,
            citizens,
            map.activities.policy,
            map.activities,
            502,
            1
        );
        PALADIN_CHECK(
            child.health == caredHealth && child.happiness == caredHappiness
        );
        child.unsupervisedMinutes =
            map.activities.policy.childcareHealthGraceMinutes;
        family.update(
            map,
            citizens,
            map.activities.policy,
            map.activities,
            503,
            1
        );
        PALADIN_CHECK(
            child.health < caredHealth && child.happiness < caredHappiness
        );
        mother.task.kind = CitizenTaskKind::Eat;
        mother.tilePosition = {8, 11};
        advance(map, citizens, 503, 1);
        PALADIN_CHECK(child.insideHome && !child.task.partner);
        // The available father takes over and feeds without the child leaving.
        father.insideHome = true;
        father.energy = 100;
        father.hunger = 10;
        father.task.kind = CitizenTaskKind::Home;
        father.tilePosition = {9, 9};
        mother.insideHome = false;
        mother.task.kind = CitizenTaskKind::Eat;
        child.hunger = 40;
        family.update(
            map,
            citizens,
            map.activities.policy,
            map.activities,
            504,
            1
        );
        PALADIN_CHECK(
            child.caregiverId == father.id && father.youngDependents == 1
        );
        PALADIN_CHECK(child.hunger < 40 && father.hunger > 10);
        // Mother resumes the primary role after the essential trip.
        mother.insideHome = true;
        mother.task.kind = CitizenTaskKind::Home;
        family.update(
            map,
            citizens,
            map.activities.policy,
            map.activities,
            505,
            1
        );
        mother.energy = 100;
        family.update(
            map,
            citizens,
            map.activities.policy,
            map.activities,
            505,
            1
        );
        PALADIN_CHECK(child.caregiverId == mother.id);
        const auto pasture = completed(
            map,
            SettlementObjectTypes::Pastureland,
            {{20, 20}, 4, 4}
        );
        map.employment().synchronize(map.objectState(), citizens);
        const auto job = map.employment().forObject(pasture);
        PALADIN_CHECK(map.employment().adjust(job, 1, citizens));
        PALADIN_CHECK(mother.workplaceId == job);
        mother.task.kind = CitizenTaskKind::Care;
        mother.task.object = home;
        mother.tilePosition = {9, 9};
        PALADIN_CHECK(map.activities.caregivingAtWorkTime(map, mother, 720));
        PALADIN_CHECK(!map.activities.caregivingAtWorkTime(map, mother, 0));
        for (int i = 0; i < 5; ++i)
        {
            const auto animal =
                map.animals.spawn(map, "cow", {20 + i % 3, 20 + i / 3});
            PALADIN_CHECK(animal);
            map.animals.find(animal)->pasture = pasture;
        }
        SettlementActivityTestFixture::produce(map, citizens, 720, 720);
        PALADIN_CHECK(map.logistics.total("meat") == 15);
        PALADIN_CHECK(map.employment().employed(job, citizens) == 1);
        SettlementActivityTestFixture::produce(map, citizens, 0, 720);
        PALADIN_CHECK(map.logistics.total("meat") == 15);
        father.task.kind = mother.task.kind = CitizenTaskKind::Sleep;
        child.task.kind = CitizenTaskKind::Sleep;
        child.hunger = 40;
        const auto absence = child.unsupervisedMinutes;
        family.update(
            map,
            citizens,
            map.activities.policy,
            map.activities,
            506,
            1
        );
        PALADIN_CHECK(
            child.hunger == 40 && child.unsupervisedMinutes < absence
        );
        child.ageMinutes =
            map.activities.policy.childMaturationMinutes * 6 / 16;
        const auto olderHealth = child.health;
        const auto olderHappiness = child.happiness;
        family.update(
            map,
            citizens,
            map.activities.policy,
            map.activities,
            504,
            1
        );
        PALADIN_CHECK(
            child.health == olderHealth && child.happiness == olderHappiness
        );
        PALADIN_CHECK(!child.caregiverId && child.unsupervisedMinutes == 0);
        PALADIN_CHECK(
            child.task.kind == CitizenTaskKind::Care ||
            child.task.kind == CitizenTaskKind::Sleep
        );
        child.task.kind = CitizenTaskKind::Home;
        child.path.clear();
        child.task.endMinute = 500;
        child.insideHome = false;
        PALADIN_CHECK(
            SettlementActivitySystem::activityLabel(child) == "Playing nearby"
        );
        child.child = false;
        PALADIN_CHECK(
            SettlementActivitySystem::activityLabel(child) == "Idling nearby"
        );
        child.task.endMinute = 0;
        PALADIN_CHECK(
            SettlementActivitySystem::activityLabel(child) ==
            "Waiting to go home"
        );
        EntityState visual;
        visual.tilePosition = {2, 3};
        visual.captureVisual(2, 3);
        PALADIN_CHECK(visual.renderX(3, .25) == 2.25);
        PALADIN_CHECK(visual.renderY(4, .75) == 3.75);
    }
    // Default-rate food economy: two producers must support six residents,
    // including travel, meals, sleep and hauling (not perfect attendance).
    for (bool livestock : {false, true})
    {
        auto map = land(50);
        SettlementCitizenState citizens;
        found(map, citizens, 6);
        map.activities.policy.dailyBirthChance = 0;
        completed(map, SettlementObjectTypes::House, {{8, 3}, 3, 3});
        completed(map, SettlementObjectTypes::House, {{12, 3}, 3, 3});
        completed(map, SettlementObjectTypes::House, {{16, 3}, 3, 3});
        SettlementObjectId producer;
        if (livestock)
        {
            producer = completed(
                map,
                SettlementObjectTypes::Pastureland,
                {{12, 12}, 4, 6}
            );
            for (int y = 12; y < 17; ++y)
            {
                for (int x : {12, 14})
                {
                    const auto animal = map.animals.spawn(map, "cow", {x, y});
                    PALADIN_CHECK(animal);
                    map.animals.find(animal)->pasture = producer;
                }
            }
        }
        else
        {
            for (int y = 0; y < 50; ++y)
            {
                for (int x = 22; x < 50; ++x)
                {
                    map.grid().tile({x, y})->terrain = TerrainType::Water;
                }
            }
            producer = completed(
                map,
                SettlementObjectTypes::FishingGrounds,
                {{18, 12}, 3, 3}
            );
        }
        map.employment().synchronize(map.objectState(), citizens);
        PALADIN_CHECK(map.employment().adjust(
            map.employment().forObject(producer),
            2,
            citizens
        ));
        advance(map, citizens, 360, 4 * 1440);
        const auto& totals =
            map.commerce.resourceTotals().at(livestock ? "meat" : "fish");
        std::cout << (livestock ? "Pasture" : "Fishery")
                  << " four-day production: " << totals.produced << '\n';
        PALADIN_CHECK(totals.produced >= 48);
        PALADIN_CHECK(citizens.citizens().size() == 6);
        for (const auto& citizen : citizens.citizens())
        {
            PALADIN_CHECK(citizen.health >= 70);
            PALADIN_CHECK(
                citizen.hunger < map.activities.policy.starvationThreshold
            );
        }
    }
    {
        auto map = land(50);
        SettlementCitizenState citizens;
        found(map, citizens, 2);
        map.activities.policy.dailyBirthChance = 0;
        const auto pasture = completed(
            map,
            SettlementObjectTypes::Pastureland,
            {{12, 12}, 4, 6}
        );
        const auto cow = map.animals.spawn(map, "cow", {13, 13});
        const auto pig = map.animals.spawn(map, "pig", {14, 15});
        map.animals.find(cow)->pasture = pasture;
        map.animals.find(pig)->pasture = pasture;
        map.employment().synchronize(map.objectState(), citizens);
        PALADIN_CHECK(map.employment().adjust(
            map.employment().forObject(pasture),
            2,
            citizens
        ));
        advance(map, citizens, 360, 150);
        PALADIN_CHECK(map.animals.find(cow)->lastTendedMinute >= 360);
        PALADIN_CHECK(map.animals.find(pig)->lastTendedMinute >= 360);
        PALADIN_CHECK(
            map.animals.find(cow)->tilePosition !=
            map.animals.find(pig)->tilePosition
        );
        // Reservations are released through the normal task lifecycle.
        advance(map, citizens, 1080, 5);
        PALADIN_CHECK(
            !map.animals.find(cow)->tender && !map.animals.find(pig)->tender
        );
    }
    {
        const FisheryJobPolicy policy;
        PALADIN_CHECK(fisheryReach({{0, 0}, 2, 2}) == 4);
        PALADIN_CHECK(fisheryReach({{0, 0}, 16, 16}) <= 12);
        PALADIN_CHECK(fisheryProductionPerMinute(120, 1, policy) * 720 == 18);
        PALADIN_CHECK(fisheryProductionPerMinute(6, 1, policy) * 720 == 9);
        PALADIN_CHECK(fisheryProductionPerMinute(0, 1, policy) == 0);
        PALADIN_CHECK(fisheryProductionPerMinute(120, 0, policy) == 0);
        const SceneProjection projection{0, 0, 10, 100, 100};
        const auto tall = projection.bounds({2, 3, 1, 1, 3, .5, 1});
        PALADIN_CHECK(tall.x == 65 && tall.y == 40 && tall.height == 30);
        SceneDrawItem behind, front;
        behind.groundDepth = 2;
        front.groundDepth = 3;
        PALADIN_CHECK(SceneDrawQueue::before(behind, front));
    }
    {
        auto map = land(80);
        SettlementCitizenState citizens;
        found(map, citizens, 2);
        map.activities.policy.dailyBirthChance = 0;
        const auto pasture = completed(
            map,
            SettlementObjectTypes::Pastureland,
            {{12, 12}, 4, 6}
        );
        const auto cow = map.animals.spawn(map, "cow", {25, 12});
        const auto mate = map.animals.spawn(map, "cow", {26, 12});
        const auto pig = map.animals.spawn(map, "pig", {27, 12});
        const auto chicken = map.animals.spawn(map, "chicken", {28, 12});
        PALADIN_CHECK(cow && mate && pig && chicken);
        PALADIN_CHECK(map.commandState().add(
            map,
            SettlementCommandTypes::Gather,
            {{24, 11}, 6, 3},
            citizens
        ));
        advance(map, citizens, 360, 200);
        PALADIN_CHECK(map.animals.containedCount(pasture) == 4);
        PALADIN_CHECK(map.animals.usedSpace(pasture) == 7);
        PALADIN_CHECK(map.animals.find(cow)->pasture == pasture);
        map.animals.find(cow)->female = true;
        map.animals.find(mate)->female = false;
        map.animals.find(cow)->breedingTarget = .000001;
        advance(map, citizens, 560, 1);
        PALADIN_CHECK(map.animals.containedCount(pasture) == 5);
        PALADIN_CHECK(map.animals.all().back().juvenile);
        const auto stock = map.logistics.forObject(pasture);
        map.animals.produce(map, pasture, 0, 561, 720);
        PALADIN_CHECK(map.logistics.available(stock, "meat") == 0);
        map.animals.produce(map, pasture, 2, 561, 720);
        PALADIN_CHECK(map.logistics.available(stock, "meat") > 0);
        PALADIN_CHECK(
            map.commerce.resourceTotals().at("meat").produced ==
            map.logistics.available(stock, "meat")
        );
        map.employment().synchronize(map.objectState(), citizens);
        PALADIN_CHECK(
            map.employment()
                .workplace(map.employment().forObject(pasture))
                ->maximumCapacity == 2
        );
        const auto position = map.animals.find(pig)->tilePosition;
        PALADIN_CHECK(map.commandState().add(
            map,
            SettlementCommandTypes::Hunt,
            {position, 1, 1},
            citizens
        ));
        advance(map, citizens, 561, 90);
        PALADIN_CHECK(map.animals.find(pig)->health == 0);
        const auto small = completed(
            map,
            SettlementObjectTypes::Pastureland,
            {{40, 40}, 2, 2}
        );
        const auto female = map.animals.spawn(map, "cow", {40, 40});
        const auto male = map.animals.spawn(map, "cow", {41, 40});
        map.animals.find(female)->pasture = small;
        map.animals.find(female)->female = true;
        map.animals.find(female)->breedingTarget = .000001;
        map.animals.find(male)->pasture = small;
        map.animals.find(male)->female = false;
        const auto stationary = map.animals.find(female)->tilePosition;
        map.animals.tick(map, citizens, 800, 1440, false);
        PALADIN_CHECK(map.animals.containedCount(small) == 2);
        PALADIN_CHECK(map.animals.usedSpace(small) == 4);
        PALADIN_CHECK(map.animals.find(female)->tilePosition == stationary);
    }
    {
        // All livestock species roam inside their pasture, including along
        // its edges, rather than remaining at the delivery tile.
        auto roaming = land();
        SettlementCitizenState observers;
        const SettlementObjectFootprint enclosure{{10, 10}, 4, 6};
        const auto pasture =
            completed(roaming, SettlementObjectTypes::Pastureland, enclosure);
        const std::array residents{
            roaming.animals.spawn(roaming, "cow", {10, 10}),
            roaming.animals.spawn(roaming, "pig", {11, 10}),
            roaming.animals.spawn(roaming, "chicken", {12, 10})
        };
        std::array<bool, 3> moved{};
        for (const auto id : residents)
        {
            PALADIN_CHECK(id);
            roaming.animals.find(id)->pasture = pasture;
        }
        for (int step = 0; step < 40; ++step)
        {
            roaming.animals.tick(roaming, observers, step * 10, 10);
            for (std::size_t i = 0; i < residents.size(); ++i)
            {
                const auto* animal = roaming.animals.find(residents[i]);
                PALADIN_CHECK(enclosure.contains(animal->tilePosition));
                PALADIN_CHECK(enclosure.contains(animal->previousTile));
                for (std::size_t j = 0; j < i; ++j)
                {
                    PALADIN_CHECK(
                        animal->tilePosition !=
                        roaming.animals.find(residents[j])->tilePosition
                    );
                }
                moved[i] =
                    moved[i] || animal->tilePosition != animal->previousTile;
            }
        }
        PALADIN_CHECK(
            std::all_of(
                moved.begin(),
                moved.end(),
                [](bool value) { return value; }
            )
        );

        auto first = land(80);
        auto second = land(80);
        first.animals.initialize(first, 123);
        second.animals.initialize(second, 123);
        PALADIN_CHECK(first.animals.all().size() == 36);
        PALADIN_CHECK(
            first.animals.all().size() == second.animals.all().size()
        );
        for (std::size_t i = 0; i < first.animals.all().size(); ++i)
        {
            PALADIN_CHECK(
                first.animals.all()[i].tilePosition ==
                second.animals.all()[i].tilePosition
            );
        }
        auto map = land();
        SettlementCitizenState citizens;
        found(map, citizens, 1);
        auto& c = SettlementActivityTestFixture::resident(citizens);
        c.tilePosition = {10, 10};
        c.path = {{11, 10}, {12, 10}};
        c.destination = {12, 10};
        c.pathIndex = 0;
        c.stepProgress = .4;
        c.stepDuration = 1;
        c.task.kind =
            CitizenTaskKind::Work; // Invalid employer cancels this task.
        c.explicitMovement = true;
        map.activities.tick(map, citizens, 400, .01);
        PALADIN_CHECK(c.visualX() > 10.39 && c.visualX() < 10.42);
        completed(map, SettlementObjectTypes::Road, {{10, 10}, 2, 1});
        citizens.tickMovement(map, .01);
        PALADIN_CHECK(c.visualX() > 10.40 && c.visualX() < 10.44);
    }
    {
        auto map = land();
        SettlementObjectPlacementController placement;
        PALADIN_CHECK(
            placement.beginPlacement(SettlementObjectTypes::CityKeep)
        );
        placement.pointerMoved(SettlementTilePosition{20, 20});
        PALADIN_CHECK(placement.visibleFootprint()->width == 3);
        placement.rotatePlacement();
        PALADIN_CHECK(placement.visibleFootprint()->width == 7);
        PALADIN_CHECK(placement.visibleFootprint()->height == 3);
        PALADIN_CHECK(
            placement.visibleDoor() == SettlementTilePosition(17, 20)
        );
        PALADIN_CHECK(placement.visibleFootprintIsValid(map));
        PALADIN_CHECK(
            placement.pointerPressed(SettlementTilePosition{20, 20}, map) ==
            SettlementPlacementCommitResult::CompletedObject
        );
        const auto& keep = map.objectState().completedObjects().back();
        PALADIN_CHECK(keep.door == SettlementTilePosition(17, 20));
        PALADIN_CHECK(
            outsideDoor(keep.footprint, *keep.door) ==
            SettlementTilePosition(16, 20)
        );
        map.logistics.synchronize(map.objectState(), 0);
        const auto stored = map.logistics.storedTotal("lumber");
        map.logistics.drop({10, 10}, "lumber", 12, 0);
        PALADIN_CHECK(map.logistics.storedTotal("lumber") == stored);
        PALADIN_CHECK(map.logistics.total("lumber") == stored + 12);
        PALADIN_CHECK(map.objectState().createConstructionSites(
            map.grid(),
            *SettlementObjectCatalog::definition(
                SettlementObjectTypes::Stockpile
            ),
            {{5, 5}, 2, 2}
        ));
        map.logistics.synchronize(map.objectState(), 0);
        const auto site = map.objectState().constructionSites().back().id;
        const auto source = map.logistics.forObject(keep.id);
        PALADIN_CHECK(map.logistics.reserve(
            CitizenId{999},
            source,
            map.logistics.forSite(site),
            "lumber",
            4
        ));
        PALADIN_CHECK(map.logistics.pickUp(CitizenId{999}));
        PALADIN_CHECK(map.logistics.storedTotal("lumber") == stored - 4);
        PALADIN_CHECK(map.logistics.deliver(CitizenId{999}));
        PALADIN_CHECK(map.logistics.storedTotal("lumber") == stored - 4);
    }
    {
        auto map = land();
        SettlementCitizenState people;
        PALADIN_CHECK(people.initialize(3, 1));
        const auto keep =
            completed(map, SettlementObjectTypes::CityKeep, {{2, 2}, 3, 7});
        const auto store =
            completed(map, SettlementObjectTypes::Stockpile, {{12, 12}, 2, 2});
        map.employment().synchronize(map.objectState(), people);
        PALADIN_CHECK(map.employment().adjust(
            map.employment().forObject(store),
            1,
            people
        ));
        auto& adult = SettlementActivityTestFixture::resident(people);
        adult.publicMealShare = 1;
        map.commerce.update(map, people, 0, 5 * 1440);
        PALADIN_CHECK(!map.commerce.usesMoney());
        PALADIN_CHECK(map.commerce.treasury->balance == 0);
        PALADIN_CHECK(
            map.commerce.householdTotal() == 0 &&
            map.commerce.businessTotal() == 0
        );
        PALADIN_CHECK(adult.publicFoodDissatisfaction == 0);
        const auto& source =
            *map.logistics.inventory(map.logistics.forObject(keep));
        const auto& destination =
            *map.logistics.inventory(map.logistics.forObject(store));
        PALADIN_CHECK(
            map.commerce.affordableTradeUnits(source, destination, 10) == 10
        );
        PALADIN_CHECK(map.commerce.buyGoods(source, destination, 10));
        PALADIN_CHECK(map.commerce.mealPrice(map, destination) == 0);
        map.commerce.treasury->balance = 10000;
        map.commerce.update(map, people, 7200, 0);
        PALADIN_CHECK(map.commerce.usesMoney());
        PALADIN_CHECK(map.commerce.businessCash(store) > 0);
        map.commerce.treasury->balance = 0;
        PALADIN_CHECK(map.commerce.usesMoney());
        PALADIN_CHECK(map.commerce.mealPrice(map, destination) > 0);
    }
    {
        auto map = land();
        SettlementCitizenState people;
        found(map, people, 4);
        map.commerce.policy.startingSavings = 60;
        auto& wife = SettlementActivityTestFixture::resident(people, 0);
        auto& husband = SettlementActivityTestFixture::resident(people, 1);
        auto& unrelated = SettlementActivityTestFixture::resident(people, 2);
        auto& child = SettlementActivityTestFixture::resident(people, 3);
        wife.spouseId = husband.id;
        husband.spouseId = wife.id;
        unrelated.spouseId = {};
        child.spouseId = {};
        child.child = true;
        child.motherId = wife.id;
        child.fatherId = husband.id;
        map.commerce.update(map, people, 0, 0);
        PALADIN_CHECK(map.commerce.spendingBalance(wife, people) == 120);
        PALADIN_CHECK(!map.commerce.canBuyMeal(unrelated, people, 100));
        PALADIN_CHECK(!map.commerce.canBuyMeal(child, people, 1));
        PALADIN_CHECK(map.commerce.buyMeal({}, wife, people, 100));
        PALADIN_CHECK(map.commerce.spendingBalance(husband, people) == 20);
        PALADIN_CHECK(map.commerce.savings(unrelated.id) == 60);
        PALADIN_CHECK(map.commerce.savings(child.id) == 0);
        PALADIN_CHECK(
            map.commerce.treasury->balance + map.commerce.householdTotal() ==
            100000
        );
    }
    {
        // Long-running construction may consume a distant pile without a
        // radius cutoff or a third path request at the pickup point.
        auto map = land(256);
        SettlementCitizenState citizens;
        found(map, citizens, 12);
        map.activities.policy.dailyBirthChance = 0;
        map.activities.policy.hungerPerDay = .001;
        map.activities.policy.awakeEnergyPerMinute = 0;
        map.activities.policy.workEnergyPerMinute = 0;
        map.logistics.drop({220, 20}, "lumber", 12000, 0);
        const auto groundLumber = [&]()
        {
            int total = 0;
            for (const auto& inventory : map.logistics.inventories())
            {
                if (inventory.kind == InventoryKind::Groundpile)
                {
                    total += inventory.amount("lumber");
                }
            }
            return total;
        };
        const auto* definition = SettlementObjectCatalog::definition(
            SettlementObjectTypes::Stockpile
        );
        PALADIN_CHECK(map.objectState().createConstructionSites(
            map.grid(),
            *definition,
            {{10, 60}, 100, 100}
        ));
        advance(map, citizens, 360, 8 * 1440);
        const auto before = groundLumber();
        PALADIN_CHECK(before < 12000);
        advance(map, citizens, 360 + 8 * 1440, 1440);
        PALADIN_CHECK(groundLumber() < before);
        const auto small = completed(
            map,
            SettlementObjectTypes::Stockpile,
            {{150, 150}, 2, 2}
        );
        const auto large = completed(
            map,
            SettlementObjectTypes::Stockpile,
            {{160, 150}, 4, 6}
        );
        PALADIN_CHECK(
            map.logistics.inventory(map.logistics.forObject(large))->capacity ==
            6 * map.logistics.inventory(map.logistics.forObject(small))
                    ->capacity
        );
    }
    {
        auto map = land();
        SettlementCitizenState people;
        found(map, people, 3);
        const auto producer =
            completed(map, SettlementObjectTypes::Bakery, {{8, 8}, 3, 2});
        const auto stock =
            completed(map, SettlementObjectTypes::Stockpile, {{16, 8}, 2, 2});
        const auto market =
            completed(map, SettlementObjectTypes::Market, {{24, 8}, 4, 6});
        map.employment().synchronize(map.objectState(), people);
        for (auto object : {producer, stock, market})
        {
            map.employment()
                .adjust(map.employment().forObject(object), 1, people);
        }
        map.commerce.update(map, people, 360, 1);
        const auto& a =
            *map.logistics.inventory(map.logistics.forObject(producer));
        const auto& b =
            *map.logistics.inventory(map.logistics.forObject(stock));
        const auto& c =
            *map.logistics.inventory(map.logistics.forObject(market));
        const auto producerCash = map.commerce.businessCash(producer);
        const auto stockCash = map.commerce.businessCash(stock);
        PALADIN_CHECK(map.commerce.buyGoods(a, b, 4));
        PALADIN_CHECK(
            map.commerce.businessCash(producer) == producerCash + 160
        );
        PALADIN_CHECK(map.commerce.businessCash(stock) == stockCash - 160);
        PALADIN_CHECK(
            map.commerce.tradePrice(a, c) > map.commerce.tradePrice(b, c)
        );
        PALADIN_CHECK(map.commerce.buyGoods(b, c, 4));
        PALADIN_CHECK(map.commerce.businessCash(stock) == stockCash + 80);
        const auto treasury = map.commerce.treasury->balance;
        map.commerce.update(map, people, 360, 0);
        PALADIN_CHECK(map.commerce.treasury->balance > treasury);
        PALADIN_CHECK(
            map.commerce.treasury->balance + map.commerce.businessTotal() +
                map.commerce.householdTotal() ==
            100000
        );
        // Inactive rates transfer actual stocked goods and cash without moving
        // citizens, and are bounded by the same storage capacities.
        map.logistics.add(a.id, "fish", 10);
        map.commerce.recordFlow({}, a.id, "fish", 10);
        map.commerce.recordFlow(a.id, b.id, "fish", 4);
        map.commerce.recordFlow(b.id, c.id, "fish", 2);
        const auto position = people.citizens().front().tilePosition;
        map.commerce.captureInactive(map, people);
        map.commerce.tickInactive(map, people, 361, 60);
        PALADIN_CHECK(people.citizens().front().tilePosition == position);
        PALADIN_CHECK(map.logistics.inventory(c.id)->amount("fish") > 0);
        PALADIN_CHECK(
            map.logistics.inventory(c.id)->used() <=
            map.logistics.inventory(c.id)->capacity
        );
        PALADIN_CHECK(
            map.commerce.treasury->balance + map.commerce.businessTotal() +
                map.commerce.householdTotal() ==
            100000
        );
        map.commerce.resumeActive();
    }
    {
        auto map = land();
        auto otherCity = land();
        otherCity.commerce.treasury = map.commerce.treasury;
        SettlementCitizenState citizens;
        found(map, citizens, 1);
        auto& worker = SettlementActivityTestFixture::resident(citizens, 0);
        worker.task.kind = CitizenTaskKind::Work;
        map.commerce.policy.dailyWage = 300;
        worker.happiness = 50;
        map.commerce.update(map, citizens, 720, 720);
        PALADIN_CHECK(map.commerce.realmTaxCollected == 30);
        PALADIN_CHECK(map.commerce.cityTaxCollected == 0);
        PALADIN_CHECK(map.commerce.savings(worker.id) == 870);
        PALADIN_CHECK(worker.happiness == 50);
        PALADIN_CHECK(worker.taxHappinessAdjustment == 0);
        PALADIN_CHECK(
            map.commerce.treasury->balance + map.commerce.householdTotal() ==
            100000
        );
        // Realm policy is shared; a city's additional rate is independent.
        map.commerce.treasury->incomeTax.setPercent(20);
        map.commerce.setCityTaxPercent(18);
        PALADIN_CHECK(otherCity.commerce.treasury->incomeTax.percent == 20);
        PALADIN_CHECK(otherCity.commerce.effectiveTaxPercent() == 20);
        PALADIN_CHECK(map.commerce.effectiveTaxPercent() == 18);
        map.commerce.update(map, citizens, 1440, 720);
        PALADIN_CHECK(
            worker.taxHappinessAdjustment < 0 && worker.happiness < 50
        );
        map.commerce.treasury->incomeTax.setPercent(0);
        map.commerce.setCityTaxPercent(0);
        map.commerce.update(map, citizens, 4320, 2880);
        PALADIN_CHECK(
            worker.taxHappinessAdjustment > 0 && worker.happiness > 50
        );
        map.commerce.setCityTaxPercent(999);
        PALADIN_CHECK(map.commerce.effectiveTaxPercent() == 0);
        map.commerce.setCityTaxPercent(-10);
        PALADIN_CHECK(map.commerce.cityIncomeTax.percent == 0);
    }
    {
        auto map = land();
        SettlementCitizenState citizens;
        found(map, citizens, 1);
        auto& person = SettlementActivityTestFixture::resident(citizens, 0);
        map.commerce.policy.startingSavings = 0;
        map.commerce.policy.dailyWage = 300;
        // Idle government workers do not receive wages. Care support is exempt.
        map.commerce.treasury->incomeTax.setPercent(40);
        map.commerce.cityIncomeTax.setPercent(40);
        map.commerce.update(map, citizens, 0, 1);
        PALADIN_CHECK(map.commerce.savings(person.id) == 0);
        person.youngDependents = 1;
        map.commerce.update(map, citizens, 720, 720);
        PALADIN_CHECK(map.commerce.savings(person.id) == 125);
        PALADIN_CHECK(
            map.commerce.realmTaxCollected == 0 &&
            map.commerce.cityTaxCollected == 0
        );
        // Small wages below the protected balance are exempt too.
        person.task.kind = CitizenTaskKind::Work;
        person.youngDependents = 0;
        map.commerce.update(map, citizens, 840, 120);
        PALADIN_CHECK(map.commerce.savings(person.id) == 175);
        PALADIN_CHECK(
            map.commerce.realmTaxCollected == 0 &&
            map.commerce.cityTaxCollected == 0
        );
        // Fractional cents accumulate rather than disappearing per tick.
        map.commerce.policy.taxProtectedBalance = 0;
        map.commerce.policy.dailyWage = 720;
        for (int step = 0; step < 100; ++step)
        {
            map.commerce.update(map, citizens, 841 + step, 1);
        }
        PALADIN_CHECK(
            map.commerce.realmTaxCollected == 40 &&
            map.commerce.cityTaxCollected == 0
        );
        PALADIN_CHECK(
            map.commerce.treasury->balance + map.commerce.householdTotal() ==
            100000
        );
    }
    {
        auto map = land();
        const auto* market =
            SettlementObjectCatalog::definition(SettlementObjectTypes::Market);
        const auto* stockpile = SettlementObjectCatalog::definition(
            SettlementObjectTypes::Stockpile
        );
        PALADIN_CHECK(map.objectState().createConstructionSites(
            map.grid(),
            *market,
            {{8, 8}, 4, 6}
        ));
        const auto& site = map.objectState().constructionSites().back();
        PALADIN_CHECK(site.resourceDeliveries.size() == 2);
        PALADIN_CHECK(site.resourceDeliveries[0].requiredAmount == 24);
        PALADIN_CHECK(site.resourceDeliveries[1].requiredAmount == 2);
        PALADIN_CHECK(marketStallCount(4, 6) == 2);
        PALADIN_CHECK(marketStallCount(6, 4) == 2);
        PALADIN_CHECK(map.objectState().createConstructionSites(
            map.grid(),
            *stockpile,
            {{20, 8}, 3, 5}
        ));
        PALADIN_CHECK(
            map.objectState()
                .constructionSites()
                .back()
                .resourceDeliveries[0]
                .requiredAmount == 15
        );
    }
    {
        auto map = land();
        SettlementCitizenState citizens;
        found(map, citizens, 2);
        map.activities.policy.dailyBirthChance = 0;
        const auto market =
            completed(map, SettlementObjectTypes::Market, {{10, 8}, 4, 6});
        const auto stockpile =
            completed(map, SettlementObjectTypes::Stockpile, {{6, 15}, 3, 3});
        map.employment().synchronize(map.objectState(), citizens);
        const auto job = map.employment().forObject(market);
        PALADIN_CHECK(map.employment().workplace(job)->maximumCapacity == 2);
        PALADIN_CHECK(map.employment().adjust(job, 1, citizens));
        map.commerce.update(map, citizens, 360, 1);
        const auto initialMoney = map.commerce.treasury->balance +
                                  map.commerce.householdTotal() +
                                  map.commerce.businessTotal();
        PALADIN_CHECK(initialMoney == 100000);
        PALADIN_CHECK(!map.commerce.keepFoodSalesEnabled);
        // Keep food may not be collected for sale with the default switch off.
        advance(map, citizens, 360, 50);
        PALADIN_CHECK(
            map.logistics.inventory(map.logistics.forObject(market))->used() ==
            0
        );
        PALADIN_CHECK(
            map.logistics.add(map.logistics.forObject(stockpile), "fish", 20)
        );
        advance(map, citizens, 410, 120);
        PALADIN_CHECK(
            map.logistics.inventory(map.logistics.forObject(market))
                ->amount("fish") > 0
        );
        PALADIN_CHECK(
            map.logistics.inventory(map.logistics.forObject(stockpile))
                ->amount("fish") < 20
        );
        PALADIN_CHECK(map.commerce.businessCash(market) < 3200);
        const auto cash = map.commerce.treasury->balance +
                          map.commerce.householdTotal() +
                          map.commerce.businessTotal();
        PALADIN_CHECK(cash == initialMoney);
        // Paid retail consumes a real unit and transfers exactly its price.
        auto& buyer = SettlementActivityTestFixture::resident(citizens, 1);
        auto& seller = SettlementActivityTestFixture::resident(citizens, 0);
        seller.path.clear();
        seller.task = {};
        seller.task.kind = CitizenTaskKind::Work;
        seller.task.object = market;
        seller.task.startedMinute = 530;
        seller.tilePosition = seller.destination = {11, 9};
        seller.hunger = 0;
        seller.energy = 100;
        buyer.path.clear();
        buyer.task = {};
        buyer.tilePosition = buyer.destination = {12, 9};
        buyer.task.kind = CitizenTaskKind::Eat;
        buyer.task.source = map.logistics.forObject(market);
        buyer.hunger = 70;
        buyer.energy = 100;
        map.logistics.release(buyer.id);
        PALADIN_CHECK(
            map.logistics.reserve(buyer.id, buyer.task.source, {}, "fish", 1)
        );
        const auto wallet = map.commerce.savings(buyer.id);
        const auto fish = map.logistics.total("fish");
        advance(map, citizens, 530, 1);
        PALADIN_CHECK(map.commerce.savings(buyer.id) < wallet - 98);
        PALADIN_CHECK(buyer.hunger < 25);
        PALADIN_CHECK(map.logistics.total("fish") == fish - 1);
        PALADIN_CHECK(
            map.commerce.treasury->balance + map.commerce.householdTotal() +
                map.commerce.businessTotal() ==
            initialMoney
        );
        Money empty = 0, receiver = 10;
        PALADIN_CHECK(!SettlementCommerce::transfer(empty, receiver, 1));
        PALADIN_CHECK(!SettlementCommerce::transfer(receiver, empty, -1));
        // Persistent reliance on free public meals limits contentment even
        // when ordinary needs are met; market meals let it recover.
        map.commerce.policy.publicFoodGraceDays = 0;
        buyer.publicMealShare = 1;
        map.commerce.update(map, citizens, 1971, 1440);
        PALADIN_CHECK(buyer.publicFoodDissatisfaction == 2);
        for (int meal = 0; meal < 12; ++meal)
        {
            map.commerce.recordMeal(buyer, false);
        }
        map.commerce.update(map, citizens, 3411, 1440);
        PALADIN_CHECK(buyer.publicFoodDissatisfaction == 0);
        PALADIN_CHECK(
            map.commerce.treasury->balance + map.commerce.householdTotal() +
                map.commerce.businessTotal() ==
            initialMoney
        );
    }
    {
        auto map = land();
        SettlementCitizenState citizens;
        found(map, citizens, 3);
        auto& mother = SettlementActivityTestFixture::resident(citizens, 0);
        auto& father = SettlementActivityTestFixture::resident(citizens, 1);
        auto& child = SettlementActivityTestFixture::resident(citizens, 2);
        mother.sex = CitizenSex::Female;
        father.sex = CitizenSex::Male;
        child.child = true;
        child.ageYears = 2;
        child.ageMinutes =
            map.activities.policy.childMaturationMinutes * 2 / 16;
        child.motherId = mother.id;
        child.fatherId = father.id;
        SettlementActivityTestFixture::rematch(citizens);
        const auto home =
            completed(map, SettlementObjectTypes::House, {{8, 8}, 3, 3});
        map.activities.synchronizeHomes(map, citizens);
        mother.tilePosition = mother.destination = {9, 9};
        child.tilePosition = child.destination = {9, 9};
        mother.insideHome = child.insideHome = true;
        mother.hunger = 10;
        child.hunger = 60;
        map.activities.policy.dailyBirthChance = 0;
        advance(map, citizens, 360, 5);
        PALADIN_CHECK(child.hunger < 40);
        PALADIN_CHECK(mother.hunger > 10);
        PALADIN_CHECK(mother.youngDependents == 1);
        PALADIN_CHECK(mother.task.kind == CitizenTaskKind::Care);
        PALADIN_CHECK(child.task.kind != CitizenTaskKind::Eat);
        child.ageMinutes =
            map.activities.policy.childMaturationMinutes * 5 / 16;
        advance(map, citizens, 365, 1);
        PALADIN_CHECK(mother.youngDependents == 0);
        mother.task = {};
        mother.path.clear();
        mother.exitingHomeId = {};
        mother.insideHome = false;
        mother.tilePosition = mother.destination = {15, 12};
        mother.hunger = 5;
        father.hunger = 60;
        child.task = {};
        child.path.clear();
        child.insideHome = false;
        child.tilePosition = child.destination = {20, 12};
        child.hunger = 70;
        const auto start = child.tilePosition;
        // The parent now brings the meal back to the child's neighborhood.
        advance(map, citizens, 366, 40);
        PALADIN_CHECK(child.tilePosition != start);
        PALADIN_CHECK(child.hunger < 30);
        PALADIN_CHECK(mother.hunger > 20);
        PALADIN_CHECK(map.commerce.savings(child.id) == 0);
        const auto motherId = mother.id;
        child.ageMinutes = map.activities.policy.childMaturationMinutes - 1;
        advance(map, citizens, 406, 1);
        PALADIN_CHECK(!child.child && child.ageYears == 16);
        PALADIN_CHECK(!child.motherId && !child.fatherId && !child.caregiverId);
        PALADIN_CHECK(child.birthMotherId == motherId);
    }
    // Exercise real family matching, housing, fertility and maturation over
    // an 88-day settlement lifetime. Healthy parents and spare homes isolate
    // demographics from food production and pathfinding.
    {
        std::size_t total = 0;
        std::size_t minimum = 100000;
        std::size_t maximum = 0;
        for (std::uint64_t seed = 1; seed <= 16; ++seed)
        {
            auto map = land(80);
            SettlementCitizenState citizens;
            PALADIN_CHECK(citizens.initialize(8, seed));
            for (int i = 0; i < 64; ++i)
            {
                completed(
                    map,
                    SettlementObjectTypes::House,
                    {{2 + (i % 8) * 8, 2 + (i / 8) * 8}, 3, 3}
                );
            }
            SettlementFamilySystem families;
            auto policy = map.activities.policy;
            for (int minute = 60; minute <= 88 * 1440; minute += 60)
            {
                families
                    .update(map, citizens, policy, map.activities, minute, 60);
            }
            const auto population = citizens.citizens().size();
            total += population;
            minimum = std::min(minimum, population);
            maximum = std::max(maximum, population);
        }
        PALADIN_CHECK(double(total) / 16 >= 60);
        PALADIN_CHECK(double(total) / 16 <= 70);
        std::cout << "Healthy, housed day-88 population: mean "
                  << double(total) / 16 << " range " << minimum << "-"
                  << maximum << '\n';
    }
    {
        auto map = land();
        SettlementCitizenState citizens;
        found(map, citizens, 4);
        for (std::size_t i = 0; i < 4; ++i)
        {
            auto& c = SettlementActivityTestFixture::resident(citizens, i);
            c.sex = i % 2 == 0 ? CitizenSex::Female : CitizenSex::Male;
        }
        SettlementActivityTestFixture::rematch(citizens);
        completed(map, SettlementObjectTypes::House, {{8, 8}, 3, 3});
        completed(map, SettlementObjectTypes::House, {{16, 8}, 3, 3});
        map.activities.synchronizeHomes(map, citizens);
        // Two families must not block one another's births by sharing a
        // full home while another completed home stands empty.
        const auto people = citizens.citizens();
        PALADIN_CHECK(people[0].homeId == people[1].homeId);
        PALADIN_CHECK(people[2].homeId == people[3].homeId);
        PALADIN_CHECK(people[0].homeId != people[2].homeId);
    }
    {
        auto map = land();
        SettlementCitizenState citizens;
        found(map, citizens, 1);
        auto& c = SettlementActivityTestFixture::resident(citizens);
        c.tilePosition = {10, 10};
        c.path = {{11, 10}};
        c.destination = {11, 10};
        c.stepProgress = .5;
        c.stepDuration = 1;
        const double before = c.visualX();
        PALADIN_CHECK(map.objectState().createConstructionSites(
            map.grid(),
            *SettlementObjectCatalog::definition(SettlementObjectTypes::Road),
            {{8, 10}, 1, 1}
        ));
        map.activities.tick(map, citizens, 360, .12);
        PALADIN_CHECK(c.task.kind == CitizenTaskKind::Build);
        PALADIN_CHECK(c.visualX() >= before);
        PALADIN_CHECK(c.visualX() - before < .1);
        const auto farm =
            completed(map, SettlementObjectTypes::WheatFarm, {{20, 20}, 2, 2});
        PALADIN_CHECK(!map.objectState().completedObject(farm)->door);
    }
    {
        CitizenSimulationPolicy policy;
        for (const auto [hours, start, end] :
             {std::array{10, 420, 1020},
              std::array{9, 450, 990},
              std::array{8, 480, 960},
              std::array{0, 720, 720},
              std::array{24, 300, 1140}})
        {
            policy.setWorkDayHours(hours);
            PALADIN_CHECK(policy.shiftStartMinute == start);
            PALADIN_CHECK(policy.shiftEndMinute == end);
            PALADIN_CHECK(policy.isWorkTime(720) == (hours > 0));
        }
    }
    {
        auto map = land();
        SettlementCitizenState citizens;
        found(map, citizens, 2);
        advance(map, citizens, 0, 239);
        PALADIN_CHECK(citizens.populationHistory().size() == 1);
        PALADIN_CHECK(citizens.spawn(1));
        advance(map, citizens, 239, 1);
        PALADIN_CHECK(citizens.populationHistory().size() == 2);
        PALADIN_CHECK(citizens.populationHistory().back().gameMinute == 240);
        PALADIN_CHECK(citizens.populationHistory().back().population == 3);
        advance(map, citizens, 240, 240);
        PALADIN_CHECK(citizens.populationHistory().size() == 3);
        PALADIN_CHECK(citizens.populationHistory().back().gameMinute == 480);
    }
    {
        auto map = land();
        SettlementCitizenState citizens;
        found(map, citizens, 8);
        advance(map, citizens, 0, 1);
        std::set<double> thresholds;
        for (const auto& c : citizens.citizens())
        {
            PALADIN_CHECK(c.foodSeekHunger >= 50 && c.foodSeekHunger < 70);
            PALADIN_CHECK(c.activity != CitizenActivity::Sleeping);
            thresholds.insert(c.foodSeekHunger);
        }
        PALADIN_CHECK(thresholds.size() > 1);
        advance(map, citizens, 1, 299);
        for (const auto& c : citizens.citizens())
        {
            PALADIN_CHECK(c.sleptMinutes == 0);
            PALADIN_CHECK(c.activity != CitizenActivity::Sleeping);
        }
        PALADIN_CHECK(seasonAtMinute(0) == Season::Summer);
        PALADIN_CHECK(seasonAtMinute(4320) == Season::Spring);
        PALADIN_CHECK(seasonAtMinute(8640) == Season::Autumn);
        PALADIN_CHECK(seasonAtMinute(12960) == Season::Winter);
        PALADIN_CHECK(seasonAtMinute(17280) == Season::Summer);
        PALADIN_CHECK(isNight(60));
        PALADIN_CHECK(!isNight(720));
    }
    {
        auto map = land();
        SettlementCitizenState citizens;
        found(map, citizens, 1);
        const auto stockpile =
            completed(map, SettlementObjectTypes::Stockpile, {{12, 12}, 2, 2});
        map.employment().synchronize(map.objectState(), citizens);
        PALADIN_CHECK(map.employment().adjust(
            map.employment().forObject(stockpile),
            1,
            citizens
        ));
        map.activities.policy.setWorkDayHours(14);
        advance(map, citizens, 9 * 1440 + 1080, 1);
        PALADIN_CHECK(
            citizens.citizens().front().task.kind == CitizenTaskKind::Work
        );
        completed(map, SettlementObjectTypes::House, {{8, 8}, 3, 3});
        SettlementActivityTestFixture::resident(citizens).energy = 65;
        advance(map, citizens, 9 * 1440 + 1140, 239);
        PALADIN_CHECK(citizens.citizens().front().sleptMinutes == 0);
        advance(map, citizens, 9 * 1440 + 1379, 361);
        PALADIN_CHECK(citizens.citizens().front().sleptMinutes > 0);
    }
    {
        std::array<double, 4> happiness{};
        for (int index = 0; index < 4; ++index)
        {
            auto map = land();
            SettlementCitizenState citizens;
            found(map, citizens, 1);
            const auto stockpile = completed(
                map,
                SettlementObjectTypes::Stockpile,
                {{12, 12}, 2, 2}
            );
            map.employment().synchronize(map.objectState(), citizens);
            PALADIN_CHECK(map.employment().adjust(
                map.employment().forObject(stockpile),
                1,
                citizens
            ));
            SettlementActivityTestFixture::resident(citizens).happiness = 50;
            map.activities.policy.setWorkDayHours(11 + index);
            advance(map, citizens, 720, 1);
            happiness[index] = citizens.citizens().front().happiness;
        }
        for (int i = 0; i < 3; ++i)
        {
            PALADIN_CHECK(
                std::abs(happiness[i] - happiness[i + 1] - 1.0 / 1440) < 1e-8
            );
        }
    }
    {
        auto map = land();
        SettlementCitizenState citizens;
        found(map, citizens, 8);
        completed(map, SettlementObjectTypes::House, {{8, 8}, 3, 3});
        completed(map, SettlementObjectTypes::House, {{12, 8}, 3, 3});
        advance(map, citizens, 720, 1);
        std::set<double> starts;
        for (const auto& c : citizens.citizens())
        {
            starts.insert(c.restThreshold);
            PALADIN_CHECK(c.homeId);
        }
        PALADIN_CHECK(starts.size() > 1);
        for (std::size_t i = 0; i < citizens.citizens().size(); ++i)
        {
            SettlementActivityTestFixture::resident(citizens, i).energy = 45;
        }
        bool indoorWander = false;
        bool sleptInside = false;
        for (int minute = 721; minute < 1740; ++minute)
        {
            std::vector<SettlementTilePosition> before;
            for (const auto& c : citizens.citizens())
            {
                before.push_back(c.tilePosition);
            }
            advance(map, citizens, minute, 1);
            for (std::size_t i = 0; i < citizens.citizens().size(); ++i)
            {
                const auto& c = citizens.citizens()[i];
                PALADIN_CHECK(std::abs(before[i].x - c.tilePosition.x) <= 1);
                PALADIN_CHECK(std::abs(before[i].y - c.tilePosition.y) <= 1);
                const auto* home = map.objectState().completedObject(c.homeId);
                if (home && home->footprint.contains(before[i]) !=
                                home->footprint.contains(c.tilePosition))
                {
                    PALADIN_CHECK(
                        before[i] == *home->door ||
                        c.tilePosition == *home->door
                    );
                    PALADIN_CHECK(
                        before[i] ==
                            outsideDoor(home->footprint, *home->door) ||
                        c.tilePosition ==
                            outsideDoor(home->footprint, *home->door)
                    );
                }
            }
            for (const auto& c : citizens.citizens())
            {
                if (c.activity == CitizenActivity::Sleeping)
                {
                    PALADIN_CHECK(c.insideHome);
                    PALADIN_CHECK(c.bedHomeId == c.homeId && c.bedSlot >= 0);
                    PALADIN_CHECK(
                        c.tilePosition ==
                        homeBedPosition(
                            *map.objectState().completedObject(c.homeId),
                            c.bedSlot
                        )
                    );
                    PALADIN_CHECK(map.objectState()
                                      .completedObject(c.homeId)
                                      ->footprint.contains(c.tilePosition));
                    for (const auto& other : citizens.citizens())
                    {
                        if (other.id != c.id &&
                            other.activity == CitizenActivity::Sleeping)
                        {
                            PALADIN_CHECK(other.tilePosition != c.tilePosition);
                        }
                    }
                    sleptInside = true;
                }
                indoorWander |= c.insideHome &&
                                c.activity == CitizenActivity::AtHome &&
                                !c.path.empty();
            }
        }
        PALADIN_CHECK(indoorWander);
        PALADIN_CHECK(sleptInside);
        for (const auto& c : citizens.citizens())
        {
            PALADIN_CHECK(c.sleptMinutes > 0);
            PALADIN_CHECK(c.task.kind != CitizenTaskKind::Sleep);
            PALADIN_CHECK(c.activity != CitizenActivity::Sleeping);
        }
    }
    {
        auto map = land();
        SettlementCitizenState citizens;
        found(map, citizens, 1);
        const auto house =
            completed(map, SettlementObjectTypes::House, {{8, 8}, 3, 3});
        map.activities.synchronizeHomes(map, citizens);
        auto& c = SettlementActivityTestFixture::resident(citizens);
        c.tilePosition = c.destination = homeBedPosition(
            *map.objectState().completedObject(house),
            c.bedSlot
        );
        c.insideHome = true;
        c.path.clear();
        c.task = {};
        c.task.kind = CitizenTaskKind::Sleep;
        c.task.object = house;
        c.task.target = c.tilePosition;
        c.activity = CitizenActivity::Sleeping;
        c.energy = 100 - 1.0 / 6;
        c.sleptMinutes = 299;
        // Finish the actual block across noon; no daily quota can renew it.
        map.activities.tick(map, citizens, 720, 1);
        PALADIN_CHECK(std::abs(c.energy - 100) < 1e-8);
        PALADIN_CHECK(c.task.kind != CitizenTaskKind::Sleep);
        PALADIN_CHECK(c.activity != CitizenActivity::Sleeping);
    }
    {
        auto map = land();
        SettlementCitizenState citizens;
        found(map, citizens, 1);
        map.activities.policy.decisionsPerMinute = 0;
        advance(map, citizens, 720, 120);
        const auto& c = citizens.citizens().front();
        PALADIN_CHECK(std::abs(c.energy - (100 - 25.0 / 8)) < 1e-8);
        PALADIN_CHECK(c.task.kind != CitizenTaskKind::Sleep);
    }
    {
        auto map = land();
        SettlementCitizenState citizens;
        found(map, citizens, 1);
        auto& c = SettlementActivityTestFixture::resident(citizens);
        c.energy = 25;
        c.hunger = 0;
        c.path.clear();
        c.destination = c.tilePosition;
        c.task = {};
        c.task.kind = CitizenTaskKind::Sleep;
        c.task.target = c.tilePosition;
        c.activity = CitizenActivity::Sleeping;
        advance(map, citizens, 1200, 100);
        PALADIN_CHECK(c.health < 100);
        advance(map, citizens, 1300, 199);
        PALADIN_CHECK(c.task.kind == CitizenTaskKind::Sleep);
        PALADIN_CHECK(std::abs(c.energy - (25 + 299.0 / 6)) < 1e-8);
        advance(map, citizens, 1499, 1);
        PALADIN_CHECK(c.task.kind == CitizenTaskKind::Sleep);
        PALADIN_CHECK(std::abs(c.energy - 75) < 1e-8);
        advance(map, citizens, 1500, 150);
        PALADIN_CHECK(c.task.kind != CitizenTaskKind::Sleep);
        PALADIN_CHECK(std::abs(c.energy - 100) < 1e-8);
        c.energy = 99;
        c.hunger = 0;
        c.sleptMinutes = 0;
        c.task.kind = CitizenTaskKind::Sleep;
        c.task.target = c.tilePosition;
        c.activity = CitizenActivity::Sleeping;
        advance(map, citizens, 1650, 6);
        PALADIN_CHECK(c.task.kind != CitizenTaskKind::Sleep);
        PALADIN_CHECK(std::abs(c.sleptMinutes - 6) < 1e-8);
    }
    {
        auto map = land();
        SettlementCitizenState citizens;
        found(map, citizens, 1);
        const auto house =
            completed(map, SettlementObjectTypes::House, {{8, 8}, 3, 3});
        map.activities.synchronizeHomes(map, citizens);
        const auto keep = map.logistics.forObject(
            map.objectState().completedObjects().front().id
        );
        PALADIN_CHECK(
            map.logistics.reserve(CitizenId{999999}, keep, {}, "lumber", 40)
        );
        PALADIN_CHECK(map.objectState().createConstructionSites(
            map.grid(),
            *SettlementObjectCatalog::definition(
                SettlementObjectTypes::Stockpile
            ),
            {{20, 20}, 3, 3}
        ));
        auto& c = SettlementActivityTestFixture::resident(citizens);
        c.tilePosition = c.destination = {9, 9};
        c.path.clear();
        c.insideHome = true;
        c.task = {};
        c.task.kind = CitizenTaskKind::Home;
        c.task.object = house;
        c.activity = CitizenActivity::AtHome;
        c.nextHomeWander = 10000;
        c.observedLogisticsVersion = map.logistics.version();
        // An unfunded site is queued work, not an executable assignment.
        // Failed polling must not interrupt resting or route out of the house.
        for (int i = 0; i < 100; ++i)
        {
            map.activities.tick(map, citizens, 800 + i * .12, .12);
            PALADIN_CHECK(c.visualX() == 9 && c.visualY() == 9);
            PALADIN_CHECK(c.path.empty());
        }
        PALADIN_CHECK((c.tilePosition == SettlementTilePosition{9, 9}));
        PALADIN_CHECK(c.path.empty());
        PALADIN_CHECK(c.activity == CitizenActivity::AtHome);
        map.logistics.release(CitizenId{999999});
        for (int i = 0; i < 60; ++i)
        {
            map.activities.tick(map, citizens, 812 + i * .12, .12);
        }
        PALADIN_CHECK(c.task.kind == CitizenTaskKind::Haul);
    }
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
    {
        auto map = land();
        SettlementCitizenState citizens;
        found(map, citizens, 4);
        for (std::size_t i = 0; i < citizens.citizens().size(); ++i)
        {
            SettlementActivityTestFixture::resident(citizens, i).energy = 30;
        }
        advance(map, citizens, 1200, 20);
        std::set<std::pair<int, int>> positions;
        for (const auto& c : citizens.citizens())
        {
            PALADIN_CHECK(c.activity == CitizenActivity::Sleeping);
            PALADIN_CHECK(!c.homeId);
            positions.insert({c.tilePosition.x, c.tilePosition.y});
        }
        PALADIN_CHECK(positions.size() == citizens.citizens().size());
    }
    {
        auto map = land();
        SettlementCitizenState citizens;
        found(map, citizens, 2);
        map.activities.policy.decisionsPerMinute = 0;
        // Isolate the conversation/need effects from city-wide shortages.
        map.immigration.policy.housingHappinessPenalty = 0;
        map.immigration.policy.foodHappinessPenalty = 0;
        for (std::size_t i = 0; i < 2; ++i)
        {
            auto& c = SettlementActivityTestFixture::resident(citizens, i);
            c.happiness = 50;
            c.tilePosition = c.destination = {6 + int(i), 10};
            c.path.clear();
            c.task = {};
            c.task.kind = CitizenTaskKind::Talk;
            c.task.partner = citizens.citizens()[1 - i].id;
            c.task.endMinute = 730;
        }
        advance(map, citizens, 720, 1);
        for (const auto& c : citizens.citizens())
        {
            // Actual conversation restores happiness; unemployment remains
            // a small independent pressure even during leisure.
            const double homelessPressure = .5 + 1.0 / (1440 * 1440);
            const double expected = 50 + .1 - (2.5 + homelessPressure) / 1440;
            PALADIN_CHECK(std::abs(c.happiness - expected) < 1e-8);
            PALADIN_CHECK(
                std::abs(
                    c.familiarityWith(c.task.partner) -
                    map.activities.policy.familiarityPerTalkMinute
                ) < 1e-8
            );
        }
    }
    {
        auto map = land();
        SettlementCitizenState citizens;
        found(map, citizens, 4);
        for (std::size_t i = 0; i < 4; ++i)
        {
            auto& c = SettlementActivityTestFixture::resident(citizens, i);
            c.sex = i == 0 ? CitizenSex::Female : CitizenSex::Male;
            c.ageYears = 30;
        }
        SettlementActivityTestFixture::rematch(citizens);
        const auto house =
            completed(map, SettlementObjectTypes::House, {{8, 8}, 3, 3});
        map.activities.synchronizeHomes(map, citizens);
        PALADIN_CHECK(
            citizens.citizens()[0].spouseId == citizens.citizens()[1].id
        );
        PALADIN_CHECK(
            citizens.citizens()[1].spouseId == citizens.citizens()[0].id
        );
        for (const auto& c : citizens.citizens())
        {
            PALADIN_CHECK(c.homeId == house);
        }
        map.activities.policy.dailyBirthChance = 1;
        map.activities.policy.healthRecoveryPerDay = 0;
        SettlementActivityTestFixture::resident(citizens).health = 80;
        advance(map, citizens, 720, 1);
        PALADIN_CHECK(citizens.citizens().size() == 4);
        SettlementActivityTestFixture::resident(citizens).health = 100;
        // An unmarried lodger is indoors when a newborn takes their place.
        for (std::size_t i = 2; i < 4; ++i)
        {
            auto& c = SettlementActivityTestFixture::resident(citizens, i);
            c.tilePosition = c.destination = {9, 9};
            c.path.clear();
            c.insideHome = true;
        }
        advance(map, citizens, 721, 1);
        PALADIN_CHECK(citizens.citizens().size() == 5);
        const auto babyId = citizens.citizens().back().id;
        PALADIN_CHECK(citizens.citizen(babyId)->child);
        PALADIN_CHECK(
            citizens.citizen(babyId)->motherId == citizens.citizens()[0].id
        );
        PALADIN_CHECK(citizens.citizen(babyId)->homeId == house);
        PALADIN_CHECK(
            std::count_if(
                citizens.citizens().begin(),
                citizens.citizens().end(),
                [&](const auto& c) { return c.homeId == house; }
            ) == 4
        );
        PALADIN_CHECK(map.employment().unemployed(citizens) == 4);
        map.activities.policy.dailyBirthChance = 0;
        advance(map, citizens, 722, 20);
        for (const auto& c : citizens.citizens())
        {
            if (!c.homeId)
            {
                PALADIN_CHECK(!map.objectState()
                                   .completedObject(house)
                                   ->footprint.contains(c.tilePosition));
            }
        }
        // Children cannot accept a direct command, construction or employment.
        auto& baby = SettlementActivityTestFixture::resident(citizens, 4);
        baby.activity = CitizenActivity::Idle;
        PALADIN_CHECK(!citizens.moveTo(baby.id, map, {10, 10}));
        const auto stock = completed(
            map,
            SettlementObjectTypes::Stockpile,
            {{15, 15}, 10, 10}
        );
        map.employment().synchronize(map.objectState(), citizens);
        const auto job = map.employment().forObject(stock);
        for (int i = 0; i < 4; ++i)
        {
            PALADIN_CHECK(map.employment().adjust(job, 1, citizens));
        }
        PALADIN_CHECK(!map.employment().adjust(job, 1, citizens));
        PALADIN_CHECK(!citizens.citizen(babyId)->workplaceId);
        PALADIN_CHECK(map.objectState().createConstructionSites(
            map.grid(),
            *SettlementObjectCatalog::definition(SettlementObjectTypes::Road),
            {{10, 12}, 2, 1}
        ));
        advance(map, citizens, 742, 5);
        PALADIN_CHECK(
            citizens.citizen(babyId)->task.kind != CitizenTaskKind::Build
        );
        PALADIN_CHECK(
            citizens.citizen(babyId)->task.kind != CitizenTaskKind::Haul
        );
        // Adult aging crosses the fertility cutoff before the birth check.
        auto& mother = SettlementActivityTestFixture::resident(citizens);
        mother.ageYears = 44;
        mother.ageMinutes = map.activities.policy.adultYearMinutes - 1;
        mother.health = 100;
        map.activities.policy.dailyBirthChance = 1;
        advance(map, citizens, 747, 1);
        PALADIN_CHECK(citizens.citizens()[0].ageYears == 45);
        PALADIN_CHECK(citizens.citizens().size() == 5);
        map.activities.policy.dailyBirthChance = 0;
        auto& growing = SettlementActivityTestFixture::resident(citizens, 4);
        growing.ageMinutes = map.activities.policy.childMaturationMinutes - 1;
        advance(map, citizens, 748, 1);
        PALADIN_CHECK(!citizens.citizen(babyId)->child);
        PALADIN_CHECK(citizens.citizen(babyId)->ageYears == 16);
        PALADIN_CHECK(map.employment().adjust(job, 1, citizens));
    }
    {
        auto map = land();
        SettlementCitizenState citizens;
        found(map, citizens, 1);
        const auto stock =
            completed(map, SettlementObjectTypes::Stockpile, {{12, 12}, 3, 3});
        map.employment().synchronize(map.objectState(), citizens);
        PALADIN_CHECK(map.employment().adjust(
            map.employment().forObject(stock),
            1,
            citizens
        ));
        auto& c = SettlementActivityTestFixture::resident(citizens);
        c.energy = 99;
        // The next-shift forecast must not trigger near-full sleep.
        advance(map, citizens, 120, 20);
        PALADIN_CHECK(c.sleptMinutes == 0);
        PALADIN_CHECK(c.task.kind != CitizenTaskKind::Sleep);
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
        advance(map, citizens, 360, 1);
        std::size_t worker = 0;
        while (worker < citizens.citizens().size() &&
               citizens.citizens()[worker].task.kind != CitizenTaskKind::Gather)
        {
            ++worker;
        }
        PALADIN_CHECK(worker < citizens.citizens().size());
        PALADIN_CHECK(!citizens.citizens()[worker].workplaceId);
        const double beforeEnergy = citizens.citizens()[worker].energy;
        advance(map, citizens, 361, 1);
        PALADIN_CHECK(
            std::abs(
                beforeEnergy - citizens.citizens()[worker].energy -
                (25.0 / 960 + 25.0 / 720)
            ) < 1e-8
        );
        advance(map, citizens, 362, 118);
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
        map.logistics.synchronize(map.objectState(), 480);
        for (const auto& site : map.objectState().constructionSites())
        {
            PALADIN_CHECK(!map.logistics.forSite(site.id));
        }
        advance(map, citizens, 480, 100);
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
        advance(map, citizens, 580, 240);
        PALADIN_CHECK(map.objectState().constructionSites().empty());
        PALADIN_CHECK(map.objectState().completedObjectAt({12, 5}));
        PALADIN_CHECK(
            allGoods(map, citizens, "lumber") + map.heating.lumberBurned() == 28
        );
        PALADIN_CHECK(allGoods(map, citizens, "stone") == 36);
        PALADIN_CHECK(
            std::count_if(
                citizens.citizens().begin(),
                citizens.citizens().end(),
                [](const auto& c) { return bool(c.homeId); }
            ) == 4
        );
        advance(map, citizens, 1080, 160);
        for (const auto& c : citizens.citizens())
        {
            if (c.homeId)
            {
                // Off-duty residents may now walk or talk near their home.
                const auto* home = map.objectState().completedObject(c.homeId);
                PALADIN_CHECK(home);
                PALADIN_CHECK(
                    c.insideHome == home->footprint.contains(c.tilePosition)
                );
            }
        }
        const auto lumber =
            allGoods(map, citizens, "lumber") + map.heating.lumberBurned();
        PALADIN_CHECK(map.commandState().add(
            map,
            SettlementCommandTypes::Demolish,
            {{7, 12}, 1, 1},
            citizens
        ));
        advance(map, citizens, 1240, 480);
        PALADIN_CHECK(!map.objectState().completedObjectAt({7, 12}));
        PALADIN_CHECK(
            allGoods(map, citizens, "lumber") + map.heating.lumberBurned() ==
            lumber
        );
    }
    // Equal on-site time must contribute equal labor for every builder.
    for (int workers : {1, 4})
    {
        auto map = land();
        SettlementCitizenState citizens;
        found(map, citizens, workers);
        PALADIN_CHECK(map.objectState().createConstructionSites(
            map.grid(),
            *SettlementObjectCatalog::definition(SettlementObjectTypes::House),
            {{12, 12}, 3, 3}
        ));
        const auto site = map.objectState().constructionSites().front().id;
        PALADIN_CHECK(map.objectState().deliverMaterials(site, "lumber", 16));
        PALADIN_CHECK(map.objectState().deliverMaterials(site, "stone", 8));
        map.logistics.synchronize(map.objectState(), 0);
        for (int i = 0; i < workers; ++i)
        {
            SettlementActivityTestFixture::resident(citizens, i)
                .tilePosition = {12, 12};
        }
        advance(map, citizens, 360, 5);
        PALADIN_CHECK(
            std::abs(
                map.objectState().constructionSite(site)->laborMinutes -
                workers * 5.0
            ) < 0.001
        );
        PALADIN_CHECK(
            std::count_if(
                citizens.citizens().begin(),
                citizens.citizens().end(),
                [](const auto& citizen)
                { return citizen.task.kind == CitizenTaskKind::Build; }
            ) == workers
        );
        advance(map, citizens, 365, 20);
        PALADIN_CHECK(
            bool(map.objectState().completedObjectAt({12, 12})) ==
            (workers == 4)
        );
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
        c.foodSeekHunger = 50;
        advance(map, citizens, 480, 1);
        PALADIN_CHECK(c.hunger >= 50);
        PALADIN_CHECK(map.logistics.total("fish") == 20);
        advance(map, citizens, 481, 180);
        PALADIN_CHECK(c.hunger < 20);
        PALADIN_CHECK(map.logistics.total("fish") == 19);
        emptyFood(map);
        c.hunger = 75;
        c.health = 100;
        advance(map, citizens, 700, 180);
        PALADIN_CHECK(std::abs(c.hunger - 87.5) < 1e-6);
        PALADIN_CHECK(std::abs(c.health - 75) < 1e-6);
        PALADIN_CHECK(c.happiness < 100);
        c.carriedResource = "lumber";
        c.carriedAmount = 4;
        const double lumber = allGoods(map, citizens, "lumber");
        advance(map, citizens, 880, 181);
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
        // This scenario tests hunger recovery, not the default balance rate.
        map.activities.policy.fishery.minutesPerFish = 80;
        map.activities.policy.fishery.waterTilesPerWorker = 4;
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
        advance(map, citizens, 1080, 2);
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
                .cancelIntersecting(map, {{15, 14}, 3, 3}, citizens, minute) == 1
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
    {
        auto map = land();
        SettlementCitizenState citizens;
        found(map, citizens, 2);
        const auto object =
            completed(map, SettlementObjectTypes::Stockpile, {{12, 12}, 2, 2});
        map.employment().synchronize(map.objectState(), citizens);
        const auto job = map.employment().forObject(object);
        PALADIN_CHECK(map.employment().adjust(job, 1, citizens));
        auto& laborer = SettlementActivityTestFixture::resident(citizens, 1);
        laborer.tilePosition = {11, 12};
        const auto pile = map.logistics.drop({11, 12}, "lumber", 4, 1200);
        advance(map, citizens, 1200, 20);
        PALADIN_CHECK(!map.logistics.inventory(pile));
        PALADIN_CHECK(
            map.logistics.inventory(map.logistics.forObject(object))
                ->amount("lumber") == 4
        );
        PALADIN_CHECK(
            SettlementActivityTestFixture::resident(citizens).task.kind !=
            CitizenTaskKind::Work
        );
        PALADIN_CHECK(!map.activities.policy.isWorkTime(359));
        PALADIN_CHECK(map.activities.policy.isWorkTime(360));
        PALADIN_CHECK(map.activities.policy.isWorkTime(1079));
        PALADIN_CHECK(!map.activities.policy.isWorkTime(1080));
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
        const auto pile = map.logistics.drop({53, 8}, "lumber", 9, 480);
        advance(map, citizens, 480, 90);
        PALADIN_CHECK(map.logistics.inventory(pile)->amount("lumber") == 9);
        PALADIN_CHECK(map.objectState().createConstructionSites(
            map.grid(),
            *SettlementObjectCatalog::definition(
                SettlementObjectTypes::Stockpile
            ),
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

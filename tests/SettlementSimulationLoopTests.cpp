#include "TestFramework.h"
#include "world/Season.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include "world/settlements/SettlementSimulationState.h"
#include "world/settlements/commands/SettlementCommandDefinition.h"
#include "world/settlements/objects/SettlementDoor.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include "world/settlements/objects/jobs/fishery/FisheryJob.h"
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
        c.tilePosition = c.destination = {9, 9};
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
            const double expected = 50 + .1 - (2 + homelessPressure) / 1440;
            PALADIN_CHECK(std::abs(c.happiness - expected) < 1e-8);
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
        PALADIN_CHECK(citizens.citizen(babyId)->ageYears == 18);
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
        PALADIN_CHECK(allGoods(map, citizens, "lumber") == 28);
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
                PALADIN_CHECK(c.insideHome);
                PALADIN_CHECK(map.objectState()
                                  .completedObject(c.homeId)
                                  ->footprint.contains(c.tilePosition));
            }
        }
        const auto lumber = allGoods(map, citizens, "lumber");
        PALADIN_CHECK(map.commandState().add(
            map,
            SettlementCommandTypes::Demolish,
            {{7, 12}, 1, 1},
            citizens
        ));
        advance(map, citizens, 1240, 480);
        PALADIN_CHECK(!map.objectState().completedObjectAt({7, 12}));
        PALADIN_CHECK(allGoods(map, citizens, "lumber") == lumber);
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
        const auto pile = map.logistics.drop({53, 8}, "lumber", 4, 480);
        advance(map, citizens, 480, 90);
        PALADIN_CHECK(map.logistics.inventory(pile)->amount("lumber") == 4);
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

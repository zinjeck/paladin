#include "TestFramework.h"
#include "simulation/systems/SettlementNavigation.h"
#include "world/settlements/SettlementHomeBeds.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include "world/settlements/commands/SettlementCommandDefinition.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include <cmath>
using namespace Paladin;
namespace
{
    SettlementMap makeMap(int side)
    {
        SettlementGrid grid(side, side);
        for (int y = 0; y < side; ++y)
        {
            for (int x = 0; x < side; ++x)
            {
                auto& tile = *grid.tile({x, y});
                tile.terrain = TerrainType::Land;
                tile.biome = BiomeType::Forest;
                tile.temperature = Temperature(.5F);
                tile.rainfall = Rainfall(.7F);
            }
        }
        return SettlementMap(std::move(grid), {0, 0}, 1, 1, side, 789);
    }
} // namespace
void runSettlementActivityTests()
{
    {
        auto map = makeMap(16);
        auto house =
            *SettlementObjectCatalog::definition(SettlementObjectTypes::House);
        house.bypassesConstruction = true;
        PALADIN_CHECK(map.objectState().placeCompletedObject(
            map.grid(),
            house,
            {{3, 3}, 3, 3}
        ));
        const auto home = map.objectState().completedObjects().back().id;
        std::array<SettlementCitizen, 4> residents;
        for (auto& c : residents)
        {
            c.health = 100;
            c.homeId = home;
        }
        assignHomeBeds(map, residents);
        for (int i = 0; i < 4; ++i)
        {
            PALADIN_CHECK(residents[i].bedSlot == i);
            PALADIN_CHECK(residents[i].bedHomeId == home);
        }
        std::swap(residents[0], residents[3]);
        assignHomeBeds(map, residents);
        PALADIN_CHECK(residents[0].bedSlot == 3 && residents[3].bedSlot == 0);
        residents[1].health = 0;
        assignHomeBeds(map, residents);
        PALADIN_CHECK(residents[1].bedSlot == -1 && !residents[1].bedHomeId);
        residents[1].health = 100;
        assignHomeBeds(map, residents);
        PALADIN_CHECK(residents[1].bedSlot == 1);
        residents[2].homeId = {};
        assignHomeBeds(map, residents);
        PALADIN_CHECK(residents[2].bedSlot == -1);
        PALADIN_CHECK(map.objectState().completedObject(home)->homeLevel == 1);
        // Household changes rebuild double/single beds without assigning
        // two citizens the same navigation destination.
        for (int i=0;i<4;++i) { residents[i].id = CitizenId{std::uint64_t(i+1)}; residents[i].homeId = home; }
        residents[0].spouseId=residents[2].id; residents[2].spouseId=residents[0].id;
        assignHomeBeds(map,residents);
        PALADIN_CHECK(residents[0].doubleBed && residents[2].doubleBed);
        PALADIN_CHECK(residents[0].bedSlot/2 == residents[2].bedSlot/2);
        PALADIN_CHECK(!residents[1].doubleBed && !residents[3].doubleBed);
        residents[1].spouseId=residents[3].id; residents[3].spouseId=residents[1].id;
        assignHomeBeds(map,residents);
        unsigned slots=0;
        for (const auto& c : residents) { PALADIN_CHECK(c.doubleBed); slots |= 1u << c.bedSlot; }
        PALADIN_CHECK(slots == 15);
        for (auto& c : residents) c.spouseId={};
        assignHomeBeds(map,residents);
        for (const auto& c : residents) PALADIN_CHECK(!c.doubleBed && c.bedVisualOffsetX == 0);
    }
    // One focused immigration/attribute scenario, independent of wall-clock
    // speed and UI. Reuses the normal keep, names, placement and food catalog.
    {
        EntityState entity;
        PALADIN_CHECK(entity.normalized(EntityAttribute::Happiness) == 1);
        PALADIN_CHECK(entity.normalized(EntityAttribute::Hunger) == 1);
        entity.modifyAttributes(
            {{AttributeEffect::Comfort, 4},
             {AttributeEffect::HungerDistress, -2}}
        );
        PALADIN_CHECK(entity.happiness == 100);
        PALADIN_CHECK(
            entity.attributeModifiers
                .pending[std::size_t(AttributeEffect::Comfort)] == 2
        );
        entity.modifyAttributes({{AttributeEffect::Metabolism, 25}});
        PALADIN_CHECK(
            entity.hunger == 25 &&
            entity.normalized(EntityAttribute::Hunger) == .75
        );

        auto city = makeMap(32);
        const auto& keep = *SettlementObjectCatalog::definition(
            SettlementObjectTypes::CityKeep
        );
        PALADIN_CHECK(city.objectState().placeCompletedObject(
            city.grid(),
            keep,
            {{12, 12}, keep.previewWidth, keep.previewHeight}
        ));
        city.logistics.synchronize(city.objectState(), 0);
        SettlementCitizenState residents;
        PALADIN_CHECK(residents.initialize(4, 418));
        for (const auto& person : residents.citizens())
        {
            auto& c = const_cast<SettlementCitizen&>(person);
            c.health = 80;
            c.happiness = 99.9;
            c.hunger = 20;
            c.energy = 60;
            c.traits.definitionIds.push_back("test-personality");
            c.stats.values.push_back({"test-skill", 12});
        }
        city.immigration.assess(city, residents);
        PALADIN_CHECK(city.immigration.conditions().applicantsPerDay > 0);
        SettlementImmigration batched = city.immigration,
                              split = city.immigration;
        batched.advance(city, residents, 0, 1440);
        for (int minute = 0; minute < 1440; ++minute)
        {
            split.advance(city, residents, minute, 1);
        }
        PALADIN_CHECK(batched.available() == split.available());
        city.immigration = batched;
        const auto available = city.immigration.available();
        PALADIN_CHECK(available > 0);
        PALADIN_CHECK(
            !city.immigration.admit(city, residents, available + 1, 1440)
        );
        PALADIN_CHECK(!city.immigration.admit(city, residents, 0, 1440));
        PALADIN_CHECK(city.immigration.admit(city, residents, available, 1440));
        PALADIN_CHECK(city.immigration.available() == 0);
        for (std::size_t i = 4; i < residents.citizens().size(); ++i)
        {
            const auto& c = residents.citizens()[i];
            PALADIN_CHECK(
                c.health == 80 && c.happiness == 99.9 && c.hunger == 20 &&
                c.energy == 60
            );
            PALADIN_CHECK(!c.child && c.ageYears >= 16 && !c.workplaceId);
            PALADIN_CHECK(
                c.traits.definitionIds.empty() && c.stats.values.empty() &&
                !c.spouseId
            );
            PALADIN_CHECK(city.grid().isValidPosition(c.tilePosition));
        }
        const double reserves = city.immigration.conditions().storedFood;
        city.logistics.drop({1, 1}, "meat", 10000, 1440);
        city.immigration.assess(city, residents);
        PALADIN_CHECK(city.immigration.conditions().storedFood == reserves);
        for (const auto& c : residents.citizens())
        {
            const_cast<SettlementCitizen&>(c).happiness = 0;
        }
        city.immigration.assess(city, residents);
        PALADIN_CHECK(city.immigration.conditions().applicantsPerDay == 0);
        auto& first = const_cast<SettlementCitizen&>(residents.citizens()[0]);
        first.modifyAttributes({{AttributeEffect::Meals, -10}});
        residents.recordAttributes(1440, 1);
        const auto tooltip = residents.attributeReport().tooltip(
            EntityAttribute::Hunger,
            residents.averageAttributes()
        );
        PALADIN_CHECK(tooltip.find("Meals eaten: -") != std::string::npos);
        first.modifyAttributes({{AttributeEffect::Socializing, .1}});
        residents.recordAttributes(1441, 1);
        auto happinessTooltip = residents.attributeReport().tooltip(
            EntityAttribute::Happiness,
            residents.averageAttributes()
        );
        PALADIN_CHECK(
            happinessTooltip.find("Conversation") != std::string::npos
        );
        PALADIN_CHECK(happinessTooltip.find("Tax policy") == std::string::npos);
        residents.recordAttributes(1442, 1);
        happinessTooltip = residents.attributeReport().tooltip(
            EntityAttribute::Happiness,
            residents.averageAttributes()
        );
        PALADIN_CHECK(
            happinessTooltip.find("Conversation") == std::string::npos
        );
        PALADIN_CHECK(
            happinessTooltip.find("No active modifiers") != std::string::npos
        );
    }
    auto map = makeMap(96);
    auto same = makeMap(96);
    map.naturalFeatures().generate(map.grid(), 789);
    same.naturalFeatures().generate(same.grid(), 789);
    std::size_t trees = 0, rocks = 0;
    for (int y = 0; y < 96; ++y)
    {
        for (int x = 0; x < 96; ++x)
        {
            const auto kind = map.naturalFeatures().at({x, y}).kind;
            PALADIN_CHECK(kind == same.naturalFeatures().at({x, y}).kind);
            trees += kind == NaturalFeatureKind::Tree;
            rocks += kind == NaturalFeatureKind::Rock;
        }
    }
    PALADIN_CHECK(trees > 100 && rocks > 0);
    PALADIN_CHECK(
        map.naturalFeatures().countIn({{0, 0}, 96, 96}) == trees + rocks
    );
    {
        SettlementNaturalFeatures sparse(1024, 1024);
        sparse.set({1023, 1023}, NaturalFeatureKind::Rock);
        sparse.mark({1023, 1023}, true);
        PALADIN_CHECK(sparse.countIn({{0, 0}, 1024, 1024}) == 1);
        PALADIN_CHECK(sparse.countIn({{0, 0}, 1023, 1023}) == 0);
        std::size_t cursor = 0, budget = 2048;
        const auto next = sparse.nextIn({{0, 0}, 1024, 1024}, cursor, budget);
        PALADIN_CHECK((next == SettlementTilePosition{1023, 1023}));
        sparse.clear({{1023, 1023}, 1, 1});
        PALADIN_CHECK(sparse.countIn({{0, 0}, 1024, 1024}) == 0);
    }
    map.grid().tile({0, 0})->terrain = TerrainType::Water;
    map.naturalFeatures().generate(map.grid(), 789);
    PALADIN_CHECK(
        map.naturalFeatures().at({0, 0}).kind == NaturalFeatureKind::None
    );

    auto commandsMap = makeMap(24);
    SettlementCitizenState citizens;
    PALADIN_CHECK(citizens.initialize(4, 123));
    commandsMap.naturalFeatures().set({1, 1}, NaturalFeatureKind::Tree);
    commandsMap.naturalFeatures().set({3, 3}, NaturalFeatureKind::Tree);
    commandsMap.naturalFeatures().set({5, 5}, NaturalFeatureKind::Rock);
    auto& commands = commandsMap.commandState();
    PALADIN_CHECK(!commands.add(
        commandsMap,
        SettlementCommandTypes::ChopTree,
        {{8, 8}, 2, 2},
        citizens
    ));
    PALADIN_CHECK(!commands.add(
        commandsMap,
        SettlementCommandTypes::Gather,
        {{0, 0}, 24, 24},
        citizens
    ));
    PALADIN_CHECK(commands.add(
        commandsMap,
        SettlementCommandTypes::ChopTree,
        {{0, 0}, 24, 24},
        citizens
    ));
    PALADIN_CHECK(commands.commands().front().targets.size() == 2);
    const auto commandId = commands.commands().front().id;
    PALADIN_CHECK(commands.contains(commandsMap, commandId, {1, 1}, {}, {}));
    PALADIN_CHECK(commandsMap.naturalFeatures().at({1, 1}).marked);
    PALADIN_CHECK(!commandsMap.naturalFeatures().at({5, 5}).marked);
    PALADIN_CHECK(!commands.add(
        commandsMap,
        SettlementCommandTypes::ChopTree,
        {{0, 0}, 24, 24},
        citizens
    ));
    PALADIN_CHECK(
        commands.cancelIntersecting(commandsMap, {{1, 1}, 1, 1}, citizens) == 1
    );
    PALADIN_CHECK(!commandsMap.naturalFeatures().at({1, 1}).marked);
    PALADIN_CHECK(!commands.contains(commandsMap, commandId, {1, 1}, {}, {}));
    PALADIN_CHECK(commands.contains(commandsMap, commandId, {3, 3}, {}, {}));
    PALADIN_CHECK(commandsMap.naturalFeatures().at({3, 3}).marked);
    commandsMap.naturalFeatures().clear({{3, 3}, 1, 1});
    // Stale work is rejected immediately, even before deferred cleanup.
    PALADIN_CHECK(!commands.contains(commandsMap, commandId, {3, 3}, {}, {}));
    commands.pruneInvalid(commandsMap, citizens);
    PALADIN_CHECK(commands.commands().empty());

    auto movementMap = makeMap(24);
    SettlementNavigation navigation;
    navigation.synchronize(movementMap);
    movementMap.grid().tile({1, 0})->terrain = TerrainType::Water;
    movementMap.grid().tile({0, 1})->terrain = TerrainType::Mountain;
    PALADIN_CHECK(!navigation.canStep(movementMap, {0, 0}, {1, 1}));
    PALADIN_CHECK(navigation.findPath(movementMap, {0, 0}, {2, 2}, {}).empty());
    movementMap.grid().tile({0, 1})->terrain = TerrainType::Land;
    const auto detour = navigation.findPath(movementMap, {0, 0}, {2, 2}, {});
    PALADIN_CHECK(!detour.empty());
    PALADIN_CHECK((detour.front() == SettlementTilePosition{0, 1}));
    CitizenMovementPolicy bounded;
    bounded.maximumExpandedNodes = 1;
    PALADIN_CHECK(
        navigation.findPath(movementMap, {0, 0}, {20, 20}, bounded).empty()
    );

    auto completedRoad =
        *SettlementObjectCatalog::definition(SettlementObjectTypes::Road);
    completedRoad.bypassesConstruction = true;
    const auto* road = &completedRoad;
    PALADIN_CHECK(movementMap.objectState().placeCompletedObject(
        movementMap.grid(),
        *road,
        {{4, 4}, 1, 1}
    ));
    navigation.synchronize(movementMap);
    PALADIN_CHECK(
        std::abs(navigation.stepCost(movementMap, {3, 4}, {4, 4}, {}) - .5) <
        1e-9
    );
    const auto* keep =
        SettlementObjectCatalog::definition(SettlementObjectTypes::CityKeep);
    PALADIN_CHECK(movementMap.objectState().placeCompletedObject(
        movementMap.grid(),
        *keep,
        {{10, 10}, keep->previewWidth, keep->previewHeight}
    ));
    citizens.placeUnpositionedCitizens(movementMap);
    citizens.idlePolicy.standProbability = 1;
    const auto id = citizens.citizens().front().id;
    PALADIN_CHECK(citizens.moveTo(id, movementMap, {2, 10}));
    const auto initial = citizens.citizen(id)->tilePosition;
    citizens.tickMovement(movementMap, 0);
    PALADIN_CHECK(citizens.citizen(id)->visualX() == initial.x);
    citizens.tickMovement(movementMap, .1);
    PALADIN_CHECK(citizens.citizen(id)->tilePosition == initial);
    PALADIN_CHECK(
        citizens.citizen(id)->visualX() != initial.x ||
        citizens.citizen(id)->visualY() != initial.y
    );
    for (int i = 0; i < 400; ++i)
    {
        citizens.tickMovement(movementMap, .1);
    }
    PALADIN_CHECK(
        (citizens.citizen(id)->tilePosition == SettlementTilePosition{2, 10})
    );
    PALADIN_CHECK(citizens.moveTo(id, movementMap, {8, 10}));
    auto obstacle =
        *SettlementObjectCatalog::definition(SettlementObjectTypes::House);
    obstacle.previewWidth = obstacle.previewHeight = 1;
    obstacle.minimumWidth = obstacle.minimumHeight = 1;
    const auto blocked = citizens.citizen(id)->path.front();
    obstacle.bypassesConstruction = true;
    PALADIN_CHECK(movementMap.objectState().placeCompletedObject(
        movementMap.grid(),
        obstacle,
        {blocked, 1, 1}
    ));
    for (int i = 0; i < 400; ++i)
    {
        citizens.tickMovement(movementMap, .1);
        PALADIN_CHECK(citizens.citizen(id)->tilePosition != blocked);
    }
    PALADIN_CHECK(
        (citizens.citizen(id)->tilePosition == SettlementTilePosition{8, 10})
    );

    SettlementCitizenState idleA, idleB;
    PALADIN_CHECK(idleA.initialize(4, 921));
    PALADIN_CHECK(idleB.initialize(4, 921));
    idleA.placeUnpositionedCitizens(movementMap);
    idleB.placeUnpositionedCitizens(movementMap);
    idleA.idlePolicy.minimumWaitMinutes = idleB.idlePolicy.minimumWaitMinutes =
        .1;
    idleA.idlePolicy.maximumWaitMinutes = idleB.idlePolicy.maximumWaitMinutes =
        .1;
    idleA.idlePolicy.standProbability = idleB.idlePolicy.standProbability = 0;
    bool moved = false;
    const auto first = idleA.citizens().front().tilePosition;
    for (int i = 0; i < 1000; ++i)
    {
        idleA.tickMovement(movementMap, .1);
        idleB.tickMovement(movementMap, .1);
        for (std::size_t j = 0; j < idleA.citizens().size(); ++j)
        {
            const auto& a = idleA.citizens()[j];
            const auto& b = idleB.citizens()[j];
            PALADIN_CHECK(a.tilePosition == b.tilePosition);
            PALADIN_CHECK(
                a.visualX() == b.visualX() && a.visualY() == b.visualY()
            );
            PALADIN_CHECK(navigation.walkable(movementMap, a.tilePosition));
        }
        moved = moved || idleA.citizens().front().tilePosition != first;
    }
    PALADIN_CHECK(moved);
}

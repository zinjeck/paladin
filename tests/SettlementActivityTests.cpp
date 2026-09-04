#include "TestFramework.h"
#include "simulation/systems/SettlementNavigation.h"
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
            for (int x = 0; x < side; ++x)
            {
                auto& tile = *grid.tile({x, y});
                tile.terrain = TerrainType::Land;
                tile.biome = BiomeType::Forest;
                tile.temperature = Temperature(.5F);
                tile.rainfall = Rainfall(.7F);
            }
        return SettlementMap(std::move(grid), {0, 0}, 1, 1, side, 789);
    }
}
void runSettlementActivityTests()
{
    auto map = makeMap(96);
    auto same = makeMap(96);
    map.naturalFeatures().generate(map.grid(), 789);
    same.naturalFeatures().generate(same.grid(), 789);
    std::size_t trees = 0, rocks = 0;
    for (int y = 0; y < 96; ++y)
        for (int x = 0; x < 96; ++x)
        {
            const auto kind = map.naturalFeatures().at({x, y}).kind;
            PALADIN_CHECK(kind == same.naturalFeatures().at({x, y}).kind);
            trees += kind == NaturalFeatureKind::Tree;
            rocks += kind == NaturalFeatureKind::Rock;
        }
    PALADIN_CHECK(trees > 100 && rocks > 0);
    map.grid().tile({0, 0})->terrain = TerrainType::Water;
    map.naturalFeatures().generate(map.grid(), 789);
    PALADIN_CHECK(map.naturalFeatures().at({0, 0}).kind == NaturalFeatureKind::None);

    auto commandsMap = makeMap(24);
    SettlementCitizenState citizens;
    PALADIN_CHECK(citizens.initialize(4, 123));
    commandsMap.naturalFeatures().set({1, 1}, NaturalFeatureKind::Tree);
    commandsMap.naturalFeatures().set({3, 3}, NaturalFeatureKind::Tree);
    commandsMap.naturalFeatures().set({5, 5}, NaturalFeatureKind::Rock);
    auto& commands = commandsMap.commandState();
    PALADIN_CHECK(!commands.add(commandsMap, SettlementCommandTypes::ChopTree, {{8, 8}, 2, 2}, citizens));
    PALADIN_CHECK(!commands.add(commandsMap, SettlementCommandTypes::Gather, {{0, 0}, 24, 24}, citizens));
    PALADIN_CHECK(commands.add(commandsMap, SettlementCommandTypes::ChopTree, {{0, 0}, 24, 24}, citizens));
    PALADIN_CHECK(commands.commands().front().targets.size() == 2);
    PALADIN_CHECK(commandsMap.naturalFeatures().at({1, 1}).marked);
    PALADIN_CHECK(!commandsMap.naturalFeatures().at({5, 5}).marked);
    PALADIN_CHECK(!commands.add(commandsMap, SettlementCommandTypes::ChopTree, {{0, 0}, 24, 24}, citizens));
    PALADIN_CHECK(commands.cancelIntersecting(commandsMap, {{1, 1}, 1, 1}, citizens) == 1);
    PALADIN_CHECK(!commandsMap.naturalFeatures().at({1, 1}).marked);
    PALADIN_CHECK(commandsMap.naturalFeatures().at({3, 3}).marked);
    commandsMap.naturalFeatures().clear({{3, 3}, 1, 1});
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
    PALADIN_CHECK(navigation.findPath(movementMap, {0, 0}, {20, 20}, bounded).empty());

    auto completedRoad = *SettlementObjectCatalog::definition(SettlementObjectTypes::Road);
    completedRoad.bypassesConstruction = true;
    const auto* road = &completedRoad;
    PALADIN_CHECK(movementMap.objectState().placeCompletedObject(movementMap.grid(), *road, {{4, 4}, 1, 1}));
    navigation.synchronize(movementMap);
    PALADIN_CHECK(std::abs(navigation.stepCost(movementMap, {3, 4}, {4, 4}, {}) - .5) < 1e-9);
    const auto* keep = SettlementObjectCatalog::definition(SettlementObjectTypes::CityKeep);
    PALADIN_CHECK(movementMap.objectState().placeCompletedObject(movementMap.grid(), *keep,
        {{10, 10}, keep->previewWidth, keep->previewHeight}));
    citizens.placeUnpositionedCitizens(movementMap);
    citizens.idlePolicy.standProbability = 1;
    const auto id = citizens.citizens().front().id;
    PALADIN_CHECK(citizens.moveTo(id, movementMap, {2, 10}));
    const auto initial = citizens.citizen(id)->tilePosition;
    citizens.tickMovement(movementMap, 0);
    PALADIN_CHECK(citizens.citizen(id)->visualX() == initial.x);
    citizens.tickMovement(movementMap, .1);
    PALADIN_CHECK(citizens.citizen(id)->tilePosition == initial);
    PALADIN_CHECK(citizens.citizen(id)->visualX() != initial.x
        || citizens.citizen(id)->visualY() != initial.y);
    for (int i = 0; i < 400; ++i) citizens.tickMovement(movementMap, .1);
    PALADIN_CHECK((citizens.citizen(id)->tilePosition == SettlementTilePosition{2, 10}));
    PALADIN_CHECK(citizens.moveTo(id, movementMap, {8, 10}));
    auto obstacle = *SettlementObjectCatalog::definition(SettlementObjectTypes::House);
    obstacle.previewWidth = obstacle.previewHeight = 1;
    obstacle.minimumWidth = obstacle.minimumHeight = 1;
    const auto blocked = citizens.citizen(id)->path.front();
    PALADIN_CHECK(movementMap.objectState().createConstructionSites(
        movementMap.grid(), obstacle, {blocked, 1, 1}));
    for (int i = 0; i < 400; ++i)
    {
        citizens.tickMovement(movementMap, .1);
        PALADIN_CHECK(citizens.citizen(id)->tilePosition != blocked);
    }
    PALADIN_CHECK((citizens.citizen(id)->tilePosition == SettlementTilePosition{8, 10}));

    SettlementCitizenState idleA, idleB;
    PALADIN_CHECK(idleA.initialize(4, 921));
    PALADIN_CHECK(idleB.initialize(4, 921));
    idleA.placeUnpositionedCitizens(movementMap);
    idleB.placeUnpositionedCitizens(movementMap);
    idleA.idlePolicy.minimumWaitMinutes = idleB.idlePolicy.minimumWaitMinutes = .1;
    idleA.idlePolicy.maximumWaitMinutes = idleB.idlePolicy.maximumWaitMinutes = .1;
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
            PALADIN_CHECK(a.visualX() == b.visualX() && a.visualY() == b.visualY());
            PALADIN_CHECK(navigation.walkable(movementMap, a.tilePosition));
        }
        moved = moved || idleA.citizens().front().tilePosition != first;
    }
    PALADIN_CHECK(moved);
}

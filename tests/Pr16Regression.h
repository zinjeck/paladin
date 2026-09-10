#pragma once

#include "TestFramework.h"
#include "interaction/GlobeCameraNavigation.h"
#include "rendering/LocalTangentWorldView.h"
#include "rendering/WorldObjectPresentation.h"
#include "world/World.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include "world/settlements/commands/SettlementCommandDefinition.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"

#include <cmath>
#include <vector>

using namespace Paladin;

inline void runPr16Regression()
{
    static_assert(WorldObjectPixelsPerTile == 32);
    static_assert(WorldObjectPixelsPerTile == WorldPixelsPerTile * 2);

    // A tangent chart must be an invertible close-world presentation around the
    // camera center, including a rolled globe. The simulation remains spherical;
    // only the close render/pick chart is planar.
    {
        WorldGrid grid(128, 64);
        Camera2D camera(64.0, 32.0);
        camera.setPlanetRotation(
            GlobeCameraNavigation::northUpOrientationAt(grid, {64, 32}),
            grid.width(),
            grid.height()
        );
        GlobeCameraNavigation::roll(camera, grid, 1280, 720, 0.37);
        const auto tangent = LocalTangentWorldView::from(
            camera,
            grid,
            1280,
            720,
            40.0
        );
        const auto projected = tangent.projectTiles(
            camera.tileX() + 2.25,
            camera.tileY() + 1.5
        );
        const auto picked = tangent.pick(projected.x, projected.y);
        PALADIN_CHECK(picked.has_value());
        const double pickedX = picked->u * grid.width();
        const double pickedY = picked->v * grid.height();
        PALADIN_CHECK(
            std::abs(pickedX - (camera.tileX() + 2.25)) < 1e-8
        );
        PALADIN_CHECK(
            std::abs(pickedY - (camera.tileY() + 1.5)) < 1e-8
        );
    }

    // Settlements and armies already have durable world identity. PR #16 adds
    // strategic roads to that same world-level model rather than faking them as
    // terrain or natural features.
    {
        WorldGenerationSettings settings;
        settings.width = 64;
        settings.height = 48;
        settings.seed = 0x16;
        World world(settings);
        const RealmId realm = world.createRealm();
        const ArmyId army = world.createArmy({10, 10});
        PALADIN_CHECK(army);
        PALADIN_CHECK(world.assignArmyToRealm(army, realm));
        PALADIN_CHECK(world.armies().size() == 1);
        PALADIN_CHECK(world.armies().front().ownerRealmId() == realm);

        const std::vector<WorldTilePosition> roadPoints{
            {10, 10}, {11, 10}, {12, 11}, {13, 11}
        };
        const WorldRoadId road = world.createWorldRoad(roadPoints, realm);
        PALADIN_CHECK(road);
        PALADIN_CHECK(world.worldRoadCount() == 1);
        PALADIN_CHECK(world.worldRoads().size() == 1);
        PALADIN_CHECK(world.worldRoad(road) != nullptr);
        PALADIN_CHECK(world.worldRoad(road)->ownerRealmId() == realm);
        PALADIN_CHECK(world.worldRoad(road)->points().size() == roadPoints.size());
        PALADIN_CHECK(!world.createWorldRoad(
            std::span<const WorldTilePosition>(roadPoints.data(), 1),
            realm
        ));
    }

    // A distant designation should mobilize an idle work cohort in the same
    // simulation minute. The old four-path burst cap made visibly idle workers
    // leak toward far commands a few at a time even though 24 paths/minute were
    // already budgeted. Active tasks are untouched by this test and by the
    // existing decision rules.
    {
        SettlementGrid grid(64, 64);
        for (int y = 0; y < 64; ++y)
        {
            for (int x = 0; x < 64; ++x)
            {
                auto& tile = *grid.tile({x, y});
                tile.terrain = TerrainType::Land;
                tile.biome = BiomeType::Forest;
                tile.temperature = Temperature(.5F);
                tile.rainfall = Rainfall(.7F);
            }
        }

        SettlementMap map(std::move(grid), {0, 0}, 1, 1, 64, 1601);
        auto keep =
            *SettlementObjectCatalog::definition(SettlementObjectTypes::CityKeep);
        keep.bypassesConstruction = true;
        PALADIN_CHECK(map.objectState().placeCompletedObject(
            map.grid(),
            keep,
            {{2, 2}, keep.previewWidth, keep.previewHeight}
        ));

        SettlementCitizenState citizens;
        PALADIN_CHECK(citizens.initialize(8, 1602));
        citizens.placeUnpositionedCitizens(map);
        for (std::size_t index = 0; index < citizens.citizens().size(); ++index)
        {
            auto& citizen = const_cast<SettlementCitizen&>(
                citizens.citizens()[index]
            );
            citizen.child = false;
            citizen.youngDependents = 0;
            citizen.health = 100;
            citizen.hunger = 0;
            citizen.energy = 100;
            citizen.workplaceId = {};
            citizen.task = {};
            citizen.path.clear();
            citizen.pathIndex = 0;
            citizen.explicitMovement = false;
            citizen.nextWorkCheckMinutes = 0;
            citizen.nextDecisionMinute = 0;
        }

        for (int x = 48; x < 56; ++x)
        {
            map.naturalFeatures().set({x, 50}, NaturalFeatureKind::Tree);
        }
        PALADIN_CHECK(map.commandState().add(
            map,
            SettlementCommandTypes::ChopTree,
            {{48, 50}, 8, 1},
            citizens
        ));

        map.activities.tick(map, citizens, 360, 1);
        std::size_t dispatched = 0;
        for (const auto& citizen : citizens.citizens())
        {
            if (citizen.task.kind == CitizenTaskKind::Gather &&
                citizen.assignedCommandId)
            {
                ++dispatched;
            }
        }
        PALADIN_CHECK(dispatched == 8);
    }
}

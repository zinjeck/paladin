#include "TestFramework.h"

#include "core/EntityRegistry.h"
#include "core/StrongId.h"
#include "debug/ConsoleCommand.h"
#include "interaction/GlobeCameraNavigation.h"
#include "rendering/Camera2D.h"
#include "rendering/GlobeView.h"
#include "rendering/SettlementWorldPresentation.h"
#include "rendering/WorldMapNavigation.h"
#include "rendering/WorldObjectPresentation.h"
#include "rendering/WorldPixelStability.h"
#include "rendering/WorldPresentation.h"
#include "world/WorldGrid.h"
#include "world/settlements/SettlementCommerce.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace
{
    struct TestEntityIdTag;

    using TestEntityId = Paladin::StrongId<TestEntityIdTag>;

    class TestEntity
    {
    public:
        TestEntity(TestEntityId id, int value) noexcept : id_(id), value_(value)
        {
        }

        [[nodiscard]]
        TestEntityId id() const noexcept
        {
            return id_;
        }

        [[nodiscard]]
        int value() const noexcept
        {
            return value_;
        }

    private:
        TestEntityId id_;
        int value_ = 0;
    };


    void testStrongIds()
    {
        static_assert(
            !std::is_assignable_v<Paladin::SettlementId&, Paladin::ArmyId>
        );

        const Paladin::SettlementId invalidId;

        PALADIN_CHECK(!invalidId.isValid());
        PALADIN_CHECK(invalidId.value() == 0);
    }


    void testEntityRegistry()
    {
        Paladin::EntityRegistry<TestEntity, TestEntityId> registry;

        const TestEntityId firstId = registry.create(10);

        const TestEntityId secondId = registry.create(20);

        const TestEntityId thirdId = registry.create(30);

        PALADIN_CHECK(firstId != secondId);
        PALADIN_CHECK(secondId != thirdId);
        PALADIN_CHECK(firstId != thirdId);

        PALADIN_CHECK(registry.size() == 3);

        TestEntity* second = registry.find(secondId);

        PALADIN_CHECK(second != nullptr);
        PALADIN_CHECK(second->value() == 20);

        PALADIN_CHECK(registry.erase(secondId));

        PALADIN_CHECK(registry.size() == 2);
        PALADIN_CHECK(registry.find(secondId) == nullptr);

        const TestEntity* first = registry.find(firstId);

        const TestEntity* third = registry.find(thirdId);

        PALADIN_CHECK(first != nullptr);
        PALADIN_CHECK(third != nullptr);

        PALADIN_CHECK(first->value() == 10);
        PALADIN_CHECK(third->value() == 30);
    }


    void testCameraZoomLimits()
    {
        Paladin::Camera2D camera;

        camera.setZoom(0.01);
        PALADIN_CHECK(camera.zoom() == 0.25);

        camera.setZoom(100.0);
        PALADIN_CHECK(camera.zoom() == 80.0);
    }

    void testGlobeRollNavigation()
    {
        constexpr double pi = 3.14159265358979323846;
        Paladin::WorldGrid grid(360, 180);
        Paladin::Camera2D camera(180.0, 90.0);

        const auto before = Paladin::GlobeView::from(camera, grid, 1000, 800);
        const auto worldRight = before.orientation().inverse().apply({1, 0, 0});
        const double centerX = camera.tileX();
        const double centerY = camera.tileY();

        Paladin::GlobeCameraNavigation::roll(
            camera,
            grid,
            1000,
            800,
            pi * 0.5
        );

        PALADIN_CHECK(std::abs(camera.tileX() - centerX) < 1e-9);
        PALADIN_CHECK(std::abs(camera.tileY() - centerY) < 1e-9);

        const auto after = Paladin::GlobeView::from(camera, grid, 1000, 800);
        const auto rotatedRight = after.orientation().apply(worldRight);
        PALADIN_CHECK(std::abs(rotatedRight.x) < 1e-9);
        PALADIN_CHECK(rotatedRight.y > 0.999999999);
        PALADIN_CHECK(std::abs(rotatedRight.z) < 1e-9);

        // Minimized/zero-radius surfaces must not contaminate camera state.
        const auto safeOrientation = after.orientation();
        Paladin::GlobeCameraNavigation::pan(
            camera,
            grid,
            0,
            0,
            1.0,
            0.0,
            10.0
        );
        const auto stillSafe = camera.planetRotation();
        PALADIN_CHECK(stillSafe.has_value());
        PALADIN_CHECK(std::abs(stillSafe->w - safeOrientation.w) < 1e-9);
        PALADIN_CHECK(std::abs(stillSafe->x - safeOrientation.x) < 1e-9);
        PALADIN_CHECK(std::abs(stillSafe->y - safeOrientation.y) < 1e-9);
        PALADIN_CHECK(std::abs(stillSafe->z - safeOrientation.z) < 1e-9);
    }

    void testGlobeNorthUpFocus()
    {
        Paladin::WorldGrid grid(360, 180);
        Paladin::Camera2D camera(0.0, 0.0);
        const Paladin::WorldTilePosition target{217, 64};

        PALADIN_CHECK(Paladin::GlobeCameraNavigation::focusNorthUp(
            camera,
            grid,
            target
        ));

        const auto view = Paladin::GlobeView::from(camera, grid, 1000, 800);
        const auto projected = view.project(
            (target.x + 0.5) / grid.width(),
            (target.y + 0.5) / grid.height()
        );
        PALADIN_CHECK(std::abs(projected.x - view.cx) < 1e-8);
        PALADIN_CHECK(std::abs(projected.y - view.cy) < 1e-8);
        PALADIN_CHECK(projected.z > 0.999999999);

        PALADIN_CHECK(!Paladin::GlobeCameraNavigation::focusNorthUp(
            camera,
            grid,
            {-1, 0}
        ));
    }

    void testSettlementWorldPresentationScale()
    {
        using Paladin::settlementWorldPresentation;

        const auto empty = settlementWorldPresentation(0);
        const auto hamletSized = settlementWorldPresentation(32);
        const auto growing = settlementWorldPresentation(512);
        const auto large = settlementWorldPresentation(4096);
        const auto enormous = settlementWorldPresentation(
            std::numeric_limits<std::uint64_t>::max()
        );

        PALADIN_CHECK(hamletSized.markerDiameterPixels >
                      empty.markerDiameterPixels);
        PALADIN_CHECK(growing.markerDiameterPixels >
                      hamletSized.markerDiameterPixels);
        PALADIN_CHECK(large.markerDiameterPixels >
                      growing.markerDiameterPixels);
        PALADIN_CHECK(std::abs(enormous.markerDiameterPixels -
                               large.markerDiameterPixels) < 1e-6F);
        PALADIN_CHECK(large.labelPixelSize >= growing.labelPixelSize);
        PALADIN_CHECK(empty.markerDiameterPixels == 10.0F);
        PALADIN_CHECK(large.markerDiameterPixels == 22.0F);
        PALADIN_CHECK(large.labelPixelSize <= 1.90F);
        PALADIN_CHECK(large.borderPixels > 0.0F);
    }

    void testWorldPresentationLayers()
    {
        using Paladin::presentationForMapMode;
        using Paladin::WorldMapMode;
        using Paladin::worldPresentationState;

        const auto realm = worldPresentationState(4.0);
        const auto firstTransition = worldPresentationState(7.75);
        const auto regional = worldPresentationState(16.0);
        const auto secondTransition = worldPresentationState(34.0);
        const auto local = worldPresentationState(60.0);

        PALADIN_CHECK(realm.realmFillWeight > 0.999F);
        PALADIN_CHECK(realm.realmLabelWeight > 0.999F);
        PALADIN_CHECK(realm.settlementMarkerWeight < 0.001F);
        PALADIN_CHECK(realm.localWorldWeight < 0.001F);

        PALADIN_CHECK(firstTransition.realmFillWeight > 0.18F);
        PALADIN_CHECK(firstTransition.realmFillWeight < 1.0F);
        PALADIN_CHECK(firstTransition.settlementMarkerWeight > 0.0F);
        PALADIN_CHECK(firstTransition.settlementMarkerWeight < 1.0F);

        PALADIN_CHECK(std::abs(regional.realmFillWeight - 0.18F) < 1e-5F);
        PALADIN_CHECK(regional.realmLabelWeight < 0.001F);
        PALADIN_CHECK(regional.settlementMarkerWeight > 0.999F);
        PALADIN_CHECK(regional.localWorldWeight < 0.001F);
        PALADIN_CHECK(regional.realmBorderWeight > 0.999F);

        PALADIN_CHECK(secondTransition.settlementMarkerWeight > 0.0F);
        PALADIN_CHECK(secondTransition.settlementMarkerWeight < 1.0F);
        PALADIN_CHECK(secondTransition.localWorldWeight > 0.0F);
        PALADIN_CHECK(secondTransition.localWorldWeight < 1.0F);
        PALADIN_CHECK(secondTransition.realmBorderWeight < 1.0F);

        PALADIN_CHECK(std::abs(local.realmFillWeight - 0.18F) < 1e-5F);
        PALADIN_CHECK(local.realmLabelWeight < 0.001F);
        PALADIN_CHECK(local.settlementMarkerWeight < 0.001F);
        PALADIN_CHECK(local.localWorldWeight > 0.999F);
        PALADIN_CHECK(std::abs(local.realmBorderWeight - 0.22F) < 1e-5F);

        const auto terrain = presentationForMapMode(local, WorldMapMode::Terrain);
        PALADIN_CHECK(terrain.realmFillWeight == 0.0F);
        PALADIN_CHECK(terrain.realmBorderWeight == 0.0F);
        PALADIN_CHECK(terrain.realmLabelWeight == local.realmLabelWeight);
        PALADIN_CHECK(terrain.localWorldWeight == local.localWorldWeight);

        const auto political =
            presentationForMapMode(local, WorldMapMode::Political);
        PALADIN_CHECK(political.realmFillWeight == local.realmFillWeight);
        PALADIN_CHECK(political.realmBorderWeight == local.realmBorderWeight);
    }

    void testWorldObjectPixelStability()
    {
        using Paladin::stableWorldObjectScreenCoordinate;
        using Paladin::worldObjectPixelPitch;

        PALADIN_CHECK(std::abs(worldObjectPixelPitch(64.0) - 2.0) < 1e-12);
        PALADIN_CHECK(
            stableWorldObjectScreenCoordinate(101.2F, 64.0) == 102.0F
        );
        PALADIN_CHECK(
            stableWorldObjectScreenCoordinate(101.8F, 64.0) == 102.0F
        );

        const double pitch = worldObjectPixelPitch(40.0);
        const float snapped = stableWorldObjectScreenCoordinate(100.6F, 40.0);
        PALADIN_CHECK(std::abs(pitch - 1.25) < 1e-12);
        PALADIN_CHECK(
            std::abs(double(snapped) / pitch -
                     std::round(double(snapped) / pitch)) < 1e-6
        );
    }

    void testWorldMapModeButtonLayout()
    {
        const auto map = Paladin::WorldMapNavigation::mapBounds(1280, 720);
        const auto strip = Paladin::WorldMapNavigation::buttonBounds(1280, 720);
        const auto political =
            Paladin::WorldMapNavigation::politicalModeButtonBounds(1280, 720);
        const auto terrain =
            Paladin::WorldMapNavigation::terrainModeButtonBounds(1280, 720);

        PALADIN_CHECK(political.y + political.height <= map.y);
        PALADIN_CHECK(terrain.y + terrain.height <= map.y);
        PALADIN_CHECK(political.x + political.width < terrain.x);
        PALADIN_CHECK(
            strip.contains(
                political.x + political.width * 0.5F,
                political.y + political.height * 0.5F
            )
        );
        PALADIN_CHECK(
            strip.contains(
                terrain.x + terrain.width * 0.5F,
                terrain.y + terrain.height * 0.5F
            )
        );
    }

    void testWorldPixelStability()
    {
        using Paladin::pixelStableWorldCamera;
        using Paladin::snapWorldArtCoordinate;
        using Paladin::worldPixelStabilityActive;

        PALADIN_CHECK(!worldPixelStabilityActive(false, 27.9));
        PALADIN_CHECK(worldPixelStabilityActive(false, 28.0));
        PALADIN_CHECK(worldPixelStabilityActive(true, 24.0));
        PALADIN_CHECK(!worldPixelStabilityActive(true, 23.9));

        PALADIN_CHECK(
            std::abs(snapWorldArtCoordinate(12.04) - 12.0625) < 1e-12
        );

        Paladin::WorldGrid grid(360, 180);
        Paladin::Camera2D flat(120.041, 70.027);
        flat.setZoom(5.0);
        const auto flatStable =
            pixelStableWorldCamera(flat, grid, 1000, 800, false);
        PALADIN_CHECK(
            std::abs(flatStable.tileX() * Paladin::WorldPixelsPerTile -
                     std::round(flatStable.tileX() *
                                Paladin::WorldPixelsPerTile)) < 1e-10
        );
        PALADIN_CHECK(
            std::abs(flatStable.tileY() * Paladin::WorldPixelsPerTile -
                     std::round(flatStable.tileY() *
                                Paladin::WorldPixelsPerTile)) < 1e-10
        );
        PALADIN_CHECK(std::abs(flatStable.zoom() - flat.zoom()) < 1e-12);
        PALADIN_CHECK(std::abs(flat.tileX() - 120.041) < 1e-12);
        PALADIN_CHECK(std::abs(flat.tileY() - 70.027) < 1e-12);

        Paladin::Camera2D globe(120.041, 70.027);
        globe.setZoom(5.0);
        Paladin::GlobeCameraNavigation::roll(globe, grid, 1000, 800, 0.37);
        const double realX = globe.tileX();
        const double realY = globe.tileY();
        const auto globeStable =
            pixelStableWorldCamera(globe, grid, 1000, 800, true);
        PALADIN_CHECK(std::abs(globeStable.zoom() - globe.zoom()) < 1e-12);
        PALADIN_CHECK(std::abs(globe.tileX() - realX) < 1e-12);
        PALADIN_CHECK(std::abs(globe.tileY() - realY) < 1e-12);

        const auto stableView =
            Paladin::GlobeView::from(globeStable, grid, 1000, 800);
        const auto stableCenter = stableView.project(
            globeStable.tileX() / grid.width(),
            globeStable.tileY() / grid.height()
        );
        PALADIN_CHECK(std::abs(stableCenter.x - stableView.cx) < 1e-8);
        PALADIN_CHECK(std::abs(stableCenter.y - stableView.cy) < 1e-8);
        PALADIN_CHECK(stableCenter.z > 0.999999999);
    }
} // namespace


void runCoreTests()
{
    using Paladin::ConsoleCommandKind;
    using Paladin::parseConsoleCommand;
    PALADIN_CHECK(
        parseConsoleCommand("money").kind == ConsoleCommandKind::Invalid
    );
    PALADIN_CHECK(parseConsoleCommand("money 20").amount == 2000);
    PALADIN_CHECK(
        parseConsoleCommand("money 0").kind == ConsoleCommandKind::Money
    );
    PALADIN_CHECK(parseConsoleCommand("money -2000").amount == -200000);
    for (const auto text :
         {"money 1.5",
          "money 1 extra",
          "money nonsense",
          "money 9223372036854775807"})
    {
        PALADIN_CHECK(
            parseConsoleCommand(text).kind == ConsoleCommandKind::Invalid
        );
    }
    PALADIN_CHECK(Paladin::goldText(-200000) == "-2000.00");
    PALADIN_CHECK(Paladin::goldText(-1) == "-0.01");
    testStrongIds();
    testEntityRegistry();
    testCameraZoomLimits();
    testGlobeRollNavigation();
    testGlobeNorthUpFocus();
    testSettlementWorldPresentationScale();
    testWorldPresentationLayers();
    testWorldObjectPixelStability();
    testWorldMapModeButtonLayout();
    testWorldPixelStability();
}

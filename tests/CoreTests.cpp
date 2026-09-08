#include "TestFramework.h"

#include "core/EntityRegistry.h"
#include "core/StrongId.h"
#include "debug/ConsoleCommand.h"
#include "interaction/GlobeCameraNavigation.h"
#include "rendering/Camera2D.h"
#include "rendering/GlobeView.h"
#include "world/WorldGrid.h"
#include "world/settlements/SettlementCommerce.h"

#include <cmath>
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
}

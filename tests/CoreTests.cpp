#include "TestFramework.h"

#include "core/EntityRegistry.h"
#include "core/StrongId.h"
#include "debug/ConsoleCommand.h"
#include "rendering/Camera2D.h"
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

    void testCameraQuarterTurns()
    {
        Paladin::Camera2D camera;
        PALADIN_CHECK(camera.cityQuarterTurns() == 0);

        camera.rotateCityQuarterTurns(1);
        PALADIN_CHECK(camera.cityQuarterTurns() == 1);
        PALADIN_CHECK(camera.cityHeadingDegrees() == 90.0);
        auto view = camera.cityWorldToViewOffset(3.0, 2.0);
        PALADIN_CHECK(view.first == 2.0 && view.second == -3.0);
        auto world = camera.cityViewToWorldOffset(view.first, view.second);
        PALADIN_CHECK(world.first == 3.0 && world.second == 2.0);

        camera.rotateCityQuarterTurns(-2);
        PALADIN_CHECK(camera.cityQuarterTurns() == 3);
        view = camera.cityWorldToViewOffset(3.0, 2.0);
        PALADIN_CHECK(view.first == -2.0 && view.second == 3.0);
        world = camera.cityViewToWorldOffset(view.first, view.second);
        PALADIN_CHECK(world.first == 3.0 && world.second == 2.0);

        camera.setCityQuarterTurns(10);
        PALADIN_CHECK(camera.cityQuarterTurns() == 2);
        camera.setCityQuarterTurns(-1);
        PALADIN_CHECK(camera.cityQuarterTurns() == 3);
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
    testCameraQuarterTurns();
}

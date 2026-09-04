#include "TestFramework.h"

#include "simulation/WorldSimulationPipeline.h"
#include "world/TerrainType.h"
#include "world/World.h"
#include "world/generation/WorldGenerationSettings.h"

void runWorldTests()
{
    Paladin::WorldGenerationSettings settings;
    settings.width = 96;
    settings.height = 72;

    Paladin::World world(settings);

    const Paladin::PolityId polityId =
        world.createPolity();

    Paladin::WorldPosition foundingPosition{};
    bool foundLandTile = false;

    for (
        std::int32_t y = 0;
        y < world.grid().height() && !foundLandTile;
        ++y
    )
    {
        for (
            std::int32_t x = 0;
            x < world.grid().width();
            ++x
        )
        {
            const Paladin::WorldTile* tile =
                world.grid().tile({x, y});

            if (tile->terrain == Paladin::TerrainType::Land)
            {
                foundingPosition = {x, y};
                foundLandTile = true;
                break;
            }
        }
    }

    PALADIN_CHECK(foundLandTile);

    const Paladin::SettlementId settlementId =
        world.foundSettlement(
            foundingPosition,
            polityId
        );

    PALADIN_CHECK(settlementId.isValid());

    PALADIN_CHECK(
        !world.foundSettlement(
            foundingPosition,
            polityId
        ).isValid()
    );

    Paladin::Settlement* settlement =
        world.settlement(settlementId);

    PALADIN_CHECK(settlement != nullptr);

    PALADIN_CHECK(
        settlement->hasOwnerPolity()
    );

    PALADIN_CHECK(
        settlement->ownerPolityId()
        == polityId
    );

    PALADIN_CHECK(
        settlement->simulationState().isActive()
    );

    PALADIN_CHECK(
        settlement->simulationState().population().residents()
        == 100
    );

    PALADIN_CHECK(
        settlement->simulationState().stockpile().amount("food")
        == 600.0
    );

    PALADIN_CHECK(
        settlement->simulationState().stockpile().amount("materials")
        == 120.0
    );

    PALADIN_CHECK(
        world.makeSettlementIndependent(
            settlementId
        )
    );

    PALADIN_CHECK(
        !settlement->hasOwnerPolity()
    );

    PALADIN_CHECK(
        world.assignSettlementToPolity(
            settlementId,
            polityId
        )
    );

    PALADIN_CHECK(
        settlement->ownerPolityId() == polityId
    );

    Paladin::WorldPosition capitalPosition{
        foundingPosition.x + 1,
        foundingPosition.y
    };

    if (!world.canFoundSettlementAt(capitalPosition))
    {
        for (
            std::int32_t y = 0;
            y < world.grid().height();
            ++y
        )
        {
            for (
                std::int32_t x = 0;
                x < world.grid().width();
                ++x
            )
            {
                if (world.canFoundSettlementAt({x, y}))
                {
                    capitalPosition = {x, y};
                    y = world.grid().height();
                    break;
                }
            }
        }
    }

    const Paladin::SettlementId capitalId =
        world.foundCapitalSettlement(
            capitalPosition,
            polityId,
            {
                "  Dawn Dominion  ",
                "  Dawnfolk  ",
                "  New Dawn  ",
                {42, 86, 190},
                "civic"
            }
        );

    PALADIN_CHECK(capitalId.isValid());

    const Paladin::Polity* polity = world.polity(polityId);
    const Paladin::Settlement* capital = world.settlement(capitalId);

    PALADIN_CHECK(polity != nullptr);
    PALADIN_CHECK(capital != nullptr);

    const Paladin::Culture* culture =
        world.culture(polity->primaryCultureId());

    PALADIN_CHECK(culture != nullptr);
    const Paladin::MapColor expectedMapColor{42, 86, 190};
    PALADIN_CHECK(polity->capitalSettlementId() == capitalId);
    PALADIN_CHECK(polity->mapColor() == expectedMapColor);
    PALADIN_CHECK(polity->name() == "Dawn Dominion");
    PALADIN_CHECK(polity->startingOriginId() == "civic");
    PALADIN_CHECK(capital->name() == "New Dawn");
    PALADIN_CHECK(capital->primaryCultureId() == culture->id());
    PALADIN_CHECK(culture->name() == "Dawnfolk");

    Paladin::WorldPosition aiCapitalPosition{};
    bool foundAiCapitalPosition = false;

    for (
        std::int32_t y = 0;
        y < world.grid().height() && !foundAiCapitalPosition;
        ++y
    )
    {
        for (
            std::int32_t x = 0;
            x < world.grid().width();
            ++x
        )
        {
            if (world.canFoundSettlementAt({x, y}))
            {
                aiCapitalPosition = {x, y};
                foundAiCapitalPosition = true;
                break;
            }
        }
    }

    PALADIN_CHECK(foundAiCapitalPosition);

    const Paladin::PolityId aiPolityId =
        world.createPolity();

    const Paladin::SettlementId aiCapitalId =
        world.foundCapitalSettlement(
            aiCapitalPosition,
            aiPolityId,
            {
                "River Confederacy",
                "Riverfolk",
                "Riverhold",
                {55, 145, 95},
                "tribal"
            }
        );

    PALADIN_CHECK(aiCapitalId.isValid());

    Paladin::Settlement* simulatedPlayerCapital =
        world.settlement(capitalId);

    Paladin::Settlement* simulatedAiCapital =
        world.settlement(aiCapitalId);

    PALADIN_CHECK(simulatedPlayerCapital != nullptr);
    PALADIN_CHECK(simulatedAiCapital != nullptr);

    PALADIN_CHECK(
        simulatedPlayerCapital->simulationState()
            .population().residents() == 100
    );

    PALADIN_CHECK(
        simulatedAiCapital->simulationState()
            .population().residents() == 100
    );

    simulatedAiCapital->simulationState()
        .population().setRates({0.0, 0.02, 0.0});

    Paladin::WorldSimulationPipeline simulationPipeline;

    constexpr double gameSecondsPerYear =
        365.0 * 24.0 * 60.0 * 60.0;

    simulationPipeline.tick(
        world,
        gameSecondsPerYear
    );

    PALADIN_CHECK(
        simulatedPlayerCapital->simulationState()
            .population().residents() == 101
    );

    PALADIN_CHECK(
        simulatedAiCapital->simulationState()
            .population().residents() == 98
    );
}

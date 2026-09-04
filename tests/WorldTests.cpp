#include "TestFramework.h"

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
}

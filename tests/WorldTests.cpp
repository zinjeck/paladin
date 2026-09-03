#include "TestFramework.h"

#include "world/World.h"

void runWorldTests()
{
    Paladin::World world;

    const Paladin::PolityId polityId =
        world.createPolity();

    const Paladin::SettlementId settlementId =
        world.createSettlement(
            Paladin::WorldPosition{
                12,
                34
            }
        );

    Paladin::Settlement* settlement =
        world.settlement(settlementId);

    PALADIN_CHECK(settlement != nullptr);

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
}

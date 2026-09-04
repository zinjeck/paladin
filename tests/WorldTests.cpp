#include "TestFramework.h"

#include "world/BiomeType.h"
#include "world/TerrainType.h"
#include "world/World.h"

void runWorldTests()
{
    Paladin::World world;

    const Paladin::WorldTile* oceanTile =
        world.grid().tile({0, 0});

    const Paladin::WorldTile* landTile =
        world.grid().tile({64, 64});

    const Paladin::WorldTile* mountainTile =
        world.grid().tile({128, 80});

    PALADIN_CHECK(oceanTile != nullptr);
    PALADIN_CHECK(landTile != nullptr);
    PALADIN_CHECK(mountainTile != nullptr);

    PALADIN_CHECK(
        oceanTile->terrain == Paladin::TerrainType::Water &&
        oceanTile->biome == Paladin::BiomeType::Ocean
    );

    PALADIN_CHECK(
        landTile->terrain == Paladin::TerrainType::Land &&
        landTile->biome == Paladin::BiomeType::Plain
    );

    PALADIN_CHECK(
        mountainTile->terrain == Paladin::TerrainType::Mountain &&
        mountainTile->biome == Paladin::BiomeType::Plain
    );

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

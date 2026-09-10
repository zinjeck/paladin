#include "TestFramework.h"

#include "world/BiomeType.h"
#include "world/FoundingIdentity.h"
#include "world/TerrainType.h"
#include "world/World.h"
#include "world/generation/WorldGenerationSettings.h"
#include "world/territory/TribalInfluenceMap.h"

#include <algorithm>
#include <cmath>

namespace
{
    void makeLand(Paladin::World& world)
    {
        for (int y = 0; y < world.grid().height(); ++y)
        {
            for (int x = 0; x < world.grid().width(); ++x)
            {
                auto* tile = world.grid().tile({x, y});
                tile->terrain = Paladin::TerrainType::Land;
                tile->biome = Paladin::BiomeType::Plain;
                tile->relief = Paladin::ReliefType::Lowland;
            }
        }
        world.grid().terrainChanged();
    }

    Paladin::FoundingIdentity identity(
        const char* realm,
        const char* culture,
        const char* capital,
        Paladin::MapColor color,
        const char* origin
    )
    {
        return {realm, culture, capital, color, origin, {}};
    }
}

void runTribalInfluenceTests()
{
    using namespace Paladin;

    const TribalInfluencePolicy policy;
    const TribalPowerCenterProfile hundred =
        tribalPowerCenterProfile(100, true, policy);
    const TribalPowerCenterProfile thousand =
        tribalPowerCenterProfile(1000, true, policy);
    PALADIN_CHECK(hundred.radiusTiles > 0.0);
    PALADIN_CHECK(thousand.radiusTiles > hundred.radiusTiles);
    PALADIN_CHECK(thousand.amplitude > hundred.amplitude);
    PALADIN_CHECK(tribalPowerCenterProfile(0, true, policy).radiusTiles == 0.0);

    PALADIN_CHECK(std::abs(tribalInfluenceKernel(0.0) - 1.0) < 1e-12);
    PALADIN_CHECK(tribalInfluenceKernel(0.25) > tribalInfluenceKernel(0.5));
    PALADIN_CHECK(tribalInfluenceKernel(0.5) > tribalInfluenceKernel(0.75));
    PALADIN_CHECK(tribalInfluenceKernel(1.0) == 0.0);
    PALADIN_CHECK(tribalInfluenceKernel(2.0) == 0.0);

    const double weakExponent = tribalCompetitionExponent(0.26, policy);
    const double strongExponent = tribalCompetitionExponent(0.80, policy);
    PALADIN_CHECK(weakExponent >= 1.0);
    PALADIN_CHECK(strongExponent > weakExponent);
    PALADIN_CHECK(strongExponent <= policy.maximumCompetitionExponent + 1e-12);

    // Two equal-strength weak fields remain a true overlap; making both power
    // centers much stronger narrows their color transition mathematically
    // without creating or storing a border segment.
    WorldGenerationSettings settings;
    settings.width = 64;
    settings.height = 40;
    settings.seed = 4001;
    World world(settings);
    makeLand(world);

    const RealmId firstRealm = world.createRealm();
    const RealmId secondRealm = world.createRealm();
    const SettlementId firstCapital = world.foundCapitalSettlement(
        {16, 20},
        firstRealm,
        identity(
            "Ash Confederacy",
            "Ashfolk",
            "Ashhome",
            {145, 72, 142},
            "tribal"
        )
    );
    const SettlementId secondCapital = world.foundCapitalSettlement(
        {36, 20},
        secondRealm,
        identity(
            "River Confederacy",
            "Riverfolk",
            "Riverhome",
            {52, 126, 155},
            "tribal"
        )
    );
    PALADIN_CHECK(firstCapital.isValid());
    PALADIN_CHECK(secondCapital.isValid());

    PALADIN_CHECK(world.realm(firstRealm)->usesTribalInfluence());
    PALADIN_CHECK(world.realm(secondRealm)->usesTribalInfluence());
    PALADIN_CHECK(world.territory().controlledTileCount() == 0);
    PALADIN_CHECK(!world.territory().controllerAt({16, 20}).isValid());
    PALADIN_CHECK(!world.territory().controllerAt({36, 20}).isValid());

    const TribalInfluenceMap& weakField = world.tribalInfluence();
    PALADIN_CHECK(weakField.hasInfluence());
    PALADIN_CHECK(weakField.influenceAt({16, 20}, firstRealm) > 0.60F);
    PALADIN_CHECK(weakField.influenceAt({20, 20}, firstRealm) <
                  weakField.influenceAt({16, 20}, firstRealm));
    PALADIN_CHECK(weakField.influenceAt({48, 20}, firstRealm) == 0.0F);

    const TribalInfluenceSample weakContact = weakField.sampleAt({26, 20});
    PALADIN_CHECK(weakContact.primaryRealm.isValid());
    PALADIN_CHECK(weakContact.secondaryRealm.isValid());
    PALADIN_CHECK(weakContact.primaryInfluence > policy.visibleInfluenceThreshold);
    PALADIN_CHECK(weakContact.secondaryInfluence > policy.visibleInfluenceThreshold);
    const double fieldWeakExponent = tribalCompetitionExponent(
        weakContact.secondaryInfluence,
        policy
    );
    const std::uint64_t weakRevision = weakField.revision();

    PALADIN_CHECK(
        world.settlement(firstCapital)->simulationState().spawnCitizens(1900)
    );
    PALADIN_CHECK(
        world.settlement(secondCapital)->simulationState().spawnCitizens(1900)
    );

    const TribalInfluenceMap& strongField = world.tribalInfluence();
    PALADIN_CHECK(strongField.revision() > weakRevision);
    PALADIN_CHECK(strongField.influenceAt({48, 20}, firstRealm) > 0.0F);
    PALADIN_CHECK(strongField.influenceAt({63, 20}, firstRealm) > 0.0F);

    const TribalInfluenceSample strongContact = strongField.sampleAt({26, 20});
    PALADIN_CHECK(strongContact.secondaryRealm.isValid());
    PALADIN_CHECK(strongContact.secondaryInfluence > weakContact.secondaryInfluence);
    const double fieldStrongExponent = tribalCompetitionExponent(
        strongContact.secondaryInfluence,
        policy
    );
    PALADIN_CHECK(fieldStrongExponent > fieldWeakExponent);

    // Unequal signals should become much more decisive under strong contact,
    // while equal signals remain centered at 50/50 by symmetry.
    const double weakBlend = tribalPrimaryBlendWeight(0.34, 0.26, policy);
    const double strongBlend = tribalPrimaryBlendWeight(0.84, 0.76, policy);
    PALADIN_CHECK(weakBlend > 0.5 && weakBlend < 0.70);
    PALADIN_CHECK(strongBlend > weakBlend);
    PALADIN_CHECK(
        std::abs(tribalPrimaryBlendWeight(0.8, 0.8, policy) - 0.5) < 1e-12
    );

    // Water is not merely visually cut out. It is an actual propagation
    // barrier. A closed water ring prevents authority from leaking through.
    WorldGenerationSettings barrierSettings;
    barrierSettings.width = 48;
    barrierSettings.height = 32;
    barrierSettings.seed = 4002;
    World barrierWorld(barrierSettings);
    makeLand(barrierWorld);
    for (int x = 18; x <= 30; ++x)
    {
        barrierWorld.grid().tile({x, 10})->terrain = TerrainType::Water;
        barrierWorld.grid().tile({x, 22})->terrain = TerrainType::Water;
    }
    for (int y = 10; y <= 22; ++y)
    {
        barrierWorld.grid().tile({18, y})->terrain = TerrainType::Water;
        barrierWorld.grid().tile({30, y})->terrain = TerrainType::Water;
    }
    barrierWorld.grid().terrainChanged();

    const RealmId barrierRealm = barrierWorld.createRealm();
    const SettlementId barrierCapital = barrierWorld.foundCapitalSettlement(
        {24, 16},
        barrierRealm,
        identity(
            "Ring Tribe",
            "Ringfolk",
            "Ringhome",
            {176, 104, 65},
            "tribal"
        )
    );
    PALADIN_CHECK(barrierCapital.isValid());
    PALADIN_CHECK(
        barrierWorld.tribalInfluence().influenceAt({24, 16}, barrierRealm) > 0.0F
    );
    PALADIN_CHECK(
        barrierWorld.tribalInfluence().influenceAt({17, 16}, barrierRealm) == 0.0F
    );
    PALADIN_CHECK(
        barrierWorld.tribalInfluence().influenceAt({18, 16}, barrierRealm) == 0.0F
    );

    // Switching political organization changes the underlying territorial
    // model, not just its rendering. Civic materializes discrete sovereignty;
    // tribal removes it and returns to an overlapping influence field.
    WorldGenerationSettings conversionSettings;
    conversionSettings.width = 40;
    conversionSettings.height = 32;
    conversionSettings.seed = 4003;
    World conversionWorld(conversionSettings);
    makeLand(conversionWorld);
    const RealmId convertingRealm = conversionWorld.createRealm();
    const SettlementId convertingCapital = conversionWorld.foundCapitalSettlement(
        {20, 16},
        convertingRealm,
        identity(
            "Changing Realm",
            "Changing Folk",
            "First Camp",
            {128, 88, 170},
            "tribal"
        )
    );
    PALADIN_CHECK(convertingCapital.isValid());
    PALADIN_CHECK(
        !conversionWorld.territory().controllerAt({20, 16}).isValid()
    );
    PALADIN_CHECK(
        conversionWorld.tribalInfluence().influenceAt(
            {20, 16},
            convertingRealm
        ) > 0.0F
    );

    PALADIN_CHECK(conversionWorld.editRealmIdentity(
        convertingRealm,
        identity(
            "Changing State",
            "Changing Folk",
            "unused",
            {128, 88, 170},
            "civic"
        )
    ));
    PALADIN_CHECK(
        conversionWorld.territory().controllerAt({20, 16}) == convertingRealm
    );
    PALADIN_CHECK(
        conversionWorld.tribalInfluence().influenceAt(
            {20, 16},
            convertingRealm
        ) == 0.0F
    );

    PALADIN_CHECK(conversionWorld.editRealmIdentity(
        convertingRealm,
        identity(
            "Changing Tribe",
            "Changing Folk",
            "unused",
            {128, 88, 170},
            "tribal"
        )
    ));
    PALADIN_CHECK(
        !conversionWorld.territory().controllerAt({20, 16}).isValid()
    );
    PALADIN_CHECK(
        conversionWorld.tribalInfluence().influenceAt(
            {20, 16},
            convertingRealm
        ) > 0.0F
    );
}

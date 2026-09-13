#include "TestFramework.h"
#include "rendering/CityPixelView.h"
#include "simulation/RealmRulerSystem.h"
#include "simulation/WorldSimulationPipeline.h"
#include "world/World.h"
#include "world/generation/AiRealmGenerator.h"
#include "world/generation/SettlementMapGenerator.h"
#include "world/territory/RealmTerritoryConnections.h"
#include <cmath>
#include <iostream>
#include <set>

namespace
{
    using namespace Paladin;
    void flatten(World& world)
    {
        for (int y = 0; y < world.grid().height(); ++y)
        {
            for (int x = 0; x < world.grid().width(); ++x)
            {
                auto* tile = world.grid().tile({x, y});
                tile->terrain = TerrainType::Land;
                tile->biome = BiomeType::Plain;
            }
        }
    }
    SettlementCitizen& citizen(SettlementCitizenState& state, std::size_t i)
    {
        return const_cast<SettlementCitizen&>(state.citizens()[i]);
    }
    void checkPersonality(const RulerPersonality& p)
    {
        for (const auto axis :
             {p.militarism, p.isolationism, p.authoritarianism, p.elitism})
        {
            PALADIN_CHECK(axis.first() >= 0 && axis.first() <= 1);
            PALADIN_CHECK(std::abs(axis.first() + axis.opposite() - 1) < 1e-12);
        }
    }
} // namespace

void runRealmFeatureTests()
{
    using namespace Paladin;
    // Civic and tribal settlements share bounded, terrain-aware connections.
    for (const auto* origin : {"civic", "tribal"})
    {
        WorldGenerationSettings linkSettings;
        linkSettings.width = 128;
        linkSettings.height = 80;
        linkSettings.seed = 30;
        World linked(linkSettings);
        flatten(linked);
        for (int y = 0; y < 80; ++y)
        {
            for (int x = 0; x < 128; ++x)
            {
                linked.grid().tile({x, y})->relief = ReliefType::Lowland;
            }
        }
        auto owner = linked.createRealm();
        auto identity = FoundingIdentity{"Links", "Folk", "Home", {}, origin};
        auto home = linked.foundCapitalSettlement({30, 40}, owner, identity);
        auto near = linked.foundSettlement({50, 40}, owner);
        auto far = linked.foundSettlement({90, 40}, owner);
        PALADIN_CHECK(home && near && far);
        if (std::string_view(origin) == "civic")
        {
            PALADIN_CHECK(linked.territory().controllerAt({40, 40}) == owner);
            PALADIN_CHECK(!linked.territory().controllerAt({70, 40}));
            int core = 0;
            for (int y = 38; y <= 42; ++y)
            {
                for (int x = 88; x <= 92; ++x)
                {
                    core += linked.territory().controllerAt({x, y}) == owner;
                }
            }
            PALADIN_CHECK(core >= 10 && core < 25);
        }
        else
        {
            PALADIN_CHECK(
                linked.tribalInfluence().influenceAt({40, 40}, owner) > .3F
            );
            PALADIN_CHECK(
                linked.tribalInfluence().influenceAt({70, 40}, owner) < .025F
            );
            const auto revision = linked.tribalInfluence().revision();
            PALADIN_CHECK(linked.tribalInfluence().revision() == revision);
        }
        const auto& a = *linked.settlement(home);
        const auto& b = *linked.settlement(near);
        PALADIN_CHECK(
            !realmTerritoryConnection(linked.grid(), a, b, linked.settlements())
                 .empty()
        );
        PALADIN_CHECK(realmTerritoryConnection(
                          linked.grid(),
                          b,
                          *linked.settlement(far),
                          linked.settlements()
        )
                          .empty());
        for (int y = 0; y < 80; ++y)
        {
            linked.grid().tile({40, y})->terrain = TerrainType::Water;
        }
        linked.grid().terrainChanged();
        PALADIN_CHECK(
            realmTerritoryConnection(linked.grid(), a, b, linked.settlements())
                .empty()
        );
        if (std::string_view(origin) == "tribal")
        {
            PALADIN_CHECK(
                linked.tribalInfluence().influenceAt({40, 40}, owner) == 0
            );
        }
    }
    {
        WorldGenerationSettings settings;
        settings.width = 128;
        settings.height = 80;
        settings.seed = 72;
        World frontier(settings);
        flatten(frontier);
        for (int y = 0; y < 80; ++y)
        {
            for (int x = 0; x < 128; ++x)
            {
                frontier.grid().tile({x, y})->relief = ReliefType::Lowland;
            }
        }
        const auto rival = frontier.createRealm(),
                   owner = frontier.createRealm();
        PALADIN_CHECK(frontier.foundCapitalSettlement(
            {40, 40},
            rival,
            {"Rival", "Folk", "Rival Home", {}, "civic"}
        ));
        const auto rivalTiles = frontier.territory().controlledTileCount(rival);
        auto a = frontier.foundCapitalSettlement(
            {30, 40},
            owner,
            {"Owner", "Folk", "Home", {}, "civic"}
        );
        auto b = frontier.foundSettlement({50, 40}, owner);
        PALADIN_CHECK(a && b);
        PALADIN_CHECK(
            frontier.territory().controlledTileCount(rival) == rivalTiles
        );
        for (const auto& cell : realmTerritoryConnection(
                 frontier.grid(),
                 *frontier.settlement(a),
                 *frontier.settlement(b),
                 frontier.settlements(),
                 &frontier.territory()
             ))
        {
            PALADIN_CHECK(
                frontier.territory().controllerAt(cell.position) != rival
            );
        }
        auto west = frontier.foundSettlement({5, 60}, owner);
        auto east = frontier.foundSettlement({122, 60}, owner);
        PALADIN_CHECK(west && east);
        PALADIN_CHECK(frontier.territory().controllerAt({0, 60}) == owner);
    }
    for (std::uint64_t seed = 0; seed < 128; ++seed)
    {
        SettlementCitizenState state;
        PALADIN_CHECK(state.initialize(8, seed));
        int males = 0;
        for (const auto& c : state.citizens())
        {
            males += c.sex == CitizenSex::Male;
        }
        PALADIN_CHECK(males == 4);
    }
    WorldGenerationSettings settings;
    settings.width = 160;
    settings.height = 100;
    settings.seed = 51;
    World world(settings);
    flatten(world);
    const auto player = world.createRealm();
    FoundingIdentity identity{
        "Test Realm",
        "Test Folk",
        "Test Capital",
        {},
        "civic",
        {},
        "Ronan"
    };
    auto profile = playerSettlementFoundationProfile(51);
    const auto capital =
        world.foundCapitalSettlement({40, 50}, player, identity, profile);
    PALADIN_CHECK(capital);
    RealmRulerSystem::establishPlayer(world, player, identity.rulerName);
    auto& ruler = world.realm(player)->ruler;
    auto& state = world.settlement(capital)->simulationState().citizens();
    PALADIN_CHECK(state.citizens().size() == 8);
    const auto kingId = ruler.citizen.citizen;
    PALADIN_CHECK(state.citizen(kingId)->sex == CitizenSex::Male);
    PALADIN_CHECK(ruler.name == "Ronan");
    checkPersonality(ruler.personality);
    auto* king = const_cast<SettlementCitizen*>(state.citizen(kingId));
    std::vector<SettlementCitizen*> others;
    for (const auto& c : state.citizens())
    {
        if (c.id != kingId)
        {
            others.push_back(const_cast<SettlementCitizen*>(&c));
        }
    }
    // Direct living sons outrank an older cousin. IDs remain settlement-scoped.
    others[0]->sex = CitizenSex::Male;
    others[0]->ageYears = 19;
    others[0]->birthFatherId = kingId;
    others[1]->sex = CitizenSex::Male;
    others[1]->ageYears = 24;
    others[1]->birthFatherId = kingId;
    others[2]->sex = CitizenSex::Male;
    others[2]->ageYears = 42;
    king->health = 0;
    state.rememberAncestry(*king);
    RealmRulerSystem::updatePlayer(world, player);
    PALADIN_CHECK(ruler.citizen.citizen == others[1]->id);
    PALADIN_CHECK(ruler.dynasty == 1);
    // A grandson through a deceased son must remain reachable via stored
    // ancestry.
    const auto secondKing = others[1]->id;
    others[3]->sex = CitizenSex::Male;
    others[3]->ageYears = 7;
    others[3]->birthFatherId = others[0]->id;
    others[0]->health = 0;
    state.rememberAncestry(*others[0]);
    others[1]->health = 0;
    state.rememberAncestry(*others[1]);
    RealmRulerSystem::updatePlayer(world, player);
    PALADIN_CHECK(
        ruler.citizen.citizen == others[3]->id
    ); // nephew before unrelated citizens
    PALADIN_CHECK(ruler.dynasty == 1 && ruler.reign == 3);
    for (auto* c : others)
    {
        c->health = 0;
    }
    RealmRulerSystem::updatePlayer(world, player);
    PALADIN_CHECK(ruler.vacant);
    others[2]->health = 100;
    others[2]->birthFatherId = {};
    others[2]->birthMotherId = {};
    RealmRulerSystem::updatePlayer(world, player);
    PALADIN_CHECK(
        !ruler.vacant && ruler.citizen.citizen == others[2]->id &&
        ruler.dynasty == 2
    );

    // Fortress local size, placement, and civic authority use independent
    // dimensions.
    auto fortressProfile = playerSettlementFoundationProfile(123);
    fortressProfile.kind = SettlementKind::Fortress;
    const auto fortress =
        world.foundSettlement({70, 50}, player, fortressProfile);
    const auto city = world.foundSettlement({105, 50}, player, profile);
    PALADIN_CHECK(fortress && city && world.settlement(fortress)->isFortress());
    int fortControl = 0, cityControl = 0;
    for (int dy = -14; dy <= 14; ++dy)
    {
        for (int dx = -14; dx <= 14; ++dx)
        {
            fortControl +=
                world.territory().controllerAt({70 + dx, 50 + dy}) == player;
            cityControl +=
                world.territory().controllerAt({105 + dx, 50 + dy}) == player;
        }
    }
    PALADIN_CHECK(fortControl > cityControl * 2);
    const auto& policy = world.territoryFoundationPolicy();
    const int fw = settlementRegionDimension(
        policy.settlementRegionWidth,
        SettlementKind::Fortress
    );
    const int fh = settlementRegionDimension(
        policy.settlementRegionHeight,
        SettlementKind::Fortress
    );
    SettlementMapGenerationSettings local;
    local.localTilesPerWorldTile = 4;
    const auto fortressMap =
        SettlementMapGenerator{}
            .generate(world.grid(), {70, 50}, fw, fh, 51, local);
    const auto cityMap =
        SettlementMapGenerator{}
            .generate(world.grid(), {105, 50}, 9, 9, 51, local);
    PALADIN_CHECK(fortressMap && cityMap);
    PALADIN_CHECK(fortressMap->grid().width() * 3 == cityMap->grid().width());
    PALADIN_CHECK(fortressMap->grid().height() * 3 == cityMap->grid().height());

    // Two separated continents: seeded geography-dependent distribution,
    // bounded empires, varied populations and zero detailed AI simulation
    // allocations.
    settings.width = 300;
    settings.height = 180;
    settings.seed = 725;
    World ai(settings);
    flatten(ai);
    for (int y = 0; y < 180; ++y)
    {
        for (int x = 0; x < 300; ++x)
        {
            if (x < 10 || x >= 290 || (x >= 140 && x < 160))
            {
                ai.grid().tile({x, y})->terrain = TerrainType::Water;
            }
        }
    }
    AiRealmGenerator{}.generate(ai);
    int empires = 0;
    std::array<int, 2> perContinent{};
    std::set<std::uint64_t> populations;
    for (const auto& realm : ai.realms())
    {
        PALADIN_CHECK(realm.aiControlled && !realm.ruler.vacant);
        empires += realm.scale == RealmScale::Empire;
        const auto* home = ai.settlement(realm.capitalSettlementId());
        PALADIN_CHECK(home);
        ++perContinent[home->position().x >= 160];
        checkPersonality(realm.ruler.personality);
    }
    PALADIN_CHECK(empires <= 2);
    PALADIN_CHECK(
        perContinent[0] >= 5 && perContinent[0] <= 7 && perContinent[1] >= 5 &&
        perContinent[1] <= 7
    );
    for (const auto& settlement : ai.settlements())
    {
        const auto& simulation = settlement.simulationState();
        PALADIN_CHECK(!simulation.hasLocalMap());
        PALADIN_CHECK(simulation.citizens().citizens().empty());
        PALADIN_CHECK(
            simulation.simulationTier() == SettlementSimulationTier::Strategic
        );
        PALADIN_CHECK(
            settlement.population() >= 24 && settlement.population() < 650
        );
        populations.insert(settlement.population());
    }
    PALADIN_CHECK(populations.size() > 12);
    PALADIN_CHECK(ai.armyCount() == 0 && ai.worldRoadCount() == 0);
    WorldSimulationPipeline pipeline;
    const auto opening =
        ai.settlements().front().simulationState().stockpile().amount("food");
    pipeline.tick(ai, 1440 * 30);
    PALADIN_CHECK(
        ai.settlements().front().simulationState().stockpile().amount("food") !=
        opening
    );
    // Identical total elapsed time yields identical reigns and dynasty choices.
    Realm whole = *ai.realm(ai.realms().front().id()), divided = whole;
    RealmRulerSystem::updateAi(whole, 18.0 * 1440 * 180);
    for (int day = 0; day < 18 * 180; ++day)
    {
        RealmRulerSystem::updateAi(divided, 1440);
    }
    PALADIN_CHECK(
        whole.ruler.reign > 2 && whole.ruler.reign == divided.ruler.reign
    );
    PALADIN_CHECK(
        whole.ruler.dynasty == divided.ruler.dynasty &&
        whole.ruler.name == divided.ruler.name
    );
    PALADIN_CHECK(whole.ruler.family.size() < 200);
    checkPersonality(whole.ruler.personality);
    Realm extinct = whole;
    const auto previousDynasty = extinct.ruler.dynasty;
    for (auto& person : extinct.ruler.family)
    {
        person.alive = false;
    }
    RealmRulerSystem::updateAi(extinct, 1440);
    PALADIN_CHECK(
        extinct.ruler.dynasty == previousDynasty + 1 && !extinct.ruler.vacant
    );
    PALADIN_CHECK(
        std::any_of(
            extinct.ruler.family.begin(),
            extinct.ruler.family.end(),
            [&](const auto& person)
            { return person.alive && person.id == extinct.ruler.aiRuler; }
        )
    );
    checkPersonality(extinct.ruler.personality);
    // Founding on an otherwise empty, traversable continent adds nearby realms
    // once, without creating local city simulations or later expansion.
    settings.width = 200;
    settings.height = 160;
    World neighborhood(settings);
    flatten(neighborhood);
    const auto founder = neighborhood.createRealm();
    const auto home =
        neighborhood
            .foundCapitalSettlement({100, 80}, founder, identity, profile);
    PALADIN_CHECK(home);
    AiRealmGenerator{}.ensurePlayerNeighbors(neighborhood, {100, 80});
    PALADIN_CHECK(neighborhood.realmCount() == 4);
    const auto originalCount = neighborhood.settlementCount();
    AiRealmGenerator{}.ensurePlayerNeighbors(neighborhood, {100, 80});
    PALADIN_CHECK(
        neighborhood.realmCount() == 4 &&
        neighborhood.settlementCount() == originalCount
    );
    std::cout << "Realm pass: " << ai.realmCount() << " AI realms, "
              << ai.settlementCount() << " settlements, " << empires
              << " empires; succession and lightweight simulation passed\n";

    for (double pixels : {32.0, 63.7, 96.0, 128.0})
    {
        for (int i = 0; i < 80; ++i)
        {
            Camera2D camera;
            camera.setPosition(30 + i * .003, 40 - i * .002);
            CityPixelView view(camera, pixels, 1280, 720);
            const double sx =
                640 + (31 - view.source.tileX()) * pixels + view.offsetX;
            const double sy =
                360 + (41 - view.source.tileY()) * pixels + view.offsetY;
            PALADIN_CHECK(std::abs(view.pickX(sx, 1280) - 31) < 1e-10);
            PALADIN_CHECK(std::abs(view.pickY(sy, 720) - 41) < 1e-10);
            PALADIN_CHECK(
                std::abs(view.offsetX - std::round(view.offsetX)) < 1e-10
            );
        }
    }
}

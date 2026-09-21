#include "TestFramework.h"
#include "rendering/WorldThematicPalette.h"
#include "simulation/AiRealmSystem.h"
#include "simulation/DiplomacySystem.h"
#include "simulation/MilitarySystem.h"
#include "simulation/WorldShipmentSystem.h"
#include "simulation/systems/SettlementPopulationSystem.h"
#include "world/World.h"
#include "world/WorldPopulationField.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <set>

namespace
{
    using namespace Paladin;
    struct Fixture
    {
        static WorldGenerationSettings settings()
        {
            WorldGenerationSettings s;
            s.width = 128;
            s.height = 64;
            s.seed = 303032;
            s.populateAiRealms = false;
            return s;
        }
        World world{settings()};
        RealmId owner, neighbor, far, player;
        SettlementId home, fort, other, remote, playerCity;
        Fixture(std::uint64_t startingPopulation = 200)
        {
            for (int y = 0; y < 64; ++y)
            {
                for (int x = 0; x < 128; ++x)
                {
                    auto& tile = *world.grid().tile({x, y});
                    tile.terrain = TerrainType::Land;
                    tile.biome = BiomeType::Plain;
                }
            }
            world.grid().terrainChanged();
            owner = world.createRealm();
            neighbor = world.createRealm();
            far = world.createRealm();
            player = world.createRealm();
            auto p = defaultSettlementFoundationProfile();
            p.initialPopulation = startingPopulation;
            p.initialDetailedCitizenCount = 0;
            p.initialResources = {{"bread", 2000}, {"lumber", 1000}};
            p.citizenSeed = 303032;
            home = world.foundCapitalSettlement(
                {32, 32},
                owner,
                {"Cadre Realm", "Cadre Folk", "Cadre City", {}, "civic"},
                p
            );
            other = world.foundCapitalSettlement(
                {46, 32},
                neighbor,
                {"Friend Realm", "Friend Folk", "Friend City", {}, "civic"},
                p
            );
            remote = world.foundCapitalSettlement(
                {100, 32},
                far,
                {"Remote Realm", "Remote Folk", "Remote City", {}, "civic"},
                p
            );
            playerCity = world.foundCapitalSettlement(
                {16, 32},
                player,
                {"Player", "Player Folk", "Player City", {}, "tribal"},
                p
            );
            p.kind = SettlementKind::Fortress;
            p.initialPopulation = 80;
            p.initialResources = {{"bread", 40}};
            fort = world.foundSettlement({32, 44}, owner, p);
            PALADIN_CHECK(home && fort && other && remote && playerCity);
            for (auto id : {owner, neighbor, far})
            {
                world.realm(id)->aiControlled = true;
                world.realm(id)->treasury->balance = 100000;
            }
            world.realm(owner)->ruler.personality.militarism = RulerAxis{1};
        }
        double food() const
        {
            double total = 0;
            for (const auto& city : world.settlements())
            {
                total += WorldShipmentSystem::total(city, "bread") +
                         WorldShipmentSystem::total(city, "rations");
            }
            for (const auto& unit : world.armies())
            {
                total += unit.rations();
            }
            for (const auto& route : world.shipments())
            {
                if (route.resource == "bread" || route.resource == "rations")
                {
                    total += route.cargo;
                }
            }
            return total;
        }
        std::uint64_t people() const
        {
            std::uint64_t n = world.soldiers().size();
            for (const auto& city : world.settlements())
            {
                n += city.population();
            }
            return n;
        }
    };
    void guardsAndDemobilization()
    {
        Fixture f;
        auto& world = f.world;
        auto* realm = world.realm(f.owner);
        auto* city = world.settlement(f.home);
        const auto count = f.people();
        const double supplies = f.food();
        PALADIN_CHECK(!MilitarySystem::maintainStrategicGarrison(
            world,
            f.player,
            f.playerCity,
            10
        ));
        const int high = AiRealmSystem::garrisonTarget(world, *realm, *city);
        realm->ruler.personality.militarism = RulerAxis{0};
        PALADIN_CHECK(
            high > AiRealmSystem::garrisonTarget(world, *realm, *city)
        );
        realm->ruler.personality.militarism = RulerAxis{1};
        const auto id = MilitarySystem::maintainStrategicGarrison(
            world,
            f.owner,
            f.home,
            18
        );
        PALADIN_CHECK(
            id && world.army(id)->soldierCount() == 8 &&
            city->population() == 192
        );
        PALADIN_CHECK(
            f.people() == count && f.food() == supplies &&
            !city->simulationState().localMap()
        );
        PALADIN_CHECK(
            world.army(id)->rations() == 8 * MilitarySystem::RationsPerSoldier
        );
        std::set<std::uint64_t> people;
        for (const auto sid : world.army(id)->soldiers())
        {
            const auto* soldier = world.soldier(sid);
            PALADIN_CHECK(soldier);
            const auto* person = city->simulationState().citizens().citizen(
                soldier->sourceCitizenId()
            );
            PALADIN_CHECK(
                person && person->militaryDeployed &&
                person->militaryUnitId == id && person->sex == CitizenSex::Male
            );
            PALADIN_CHECK(people.insert(person->id.value()).second);
        }
        MilitarySystem::synchronize(world, 360);
        PALADIN_CHECK(world.army(id)->soldierCount() == 8);
        PALADIN_CHECK(
            MilitarySystem::disbandUnit(world, f.owner, id) ==
            MilitaryResult::Success
        );
        PALADIN_CHECK(
            !world.army(id) && f.people() == count && f.food() == supplies &&
            city->population() == 200
        );
        for (auto person : people)
        {
            const auto* c =
                city->simulationState().citizens().citizen(CitizenId{person});
            PALADIN_CHECK(
                c && !c->militaryDeployed && !c->soldierId && !c->militaryUnitId
            );
        }
        realm->laws.gender = GenderRights::FemaleDominated;
        const auto women = MilitarySystem::maintainStrategicGarrison(
            world,
            f.owner,
            f.home,
            3
        );
        PALADIN_CHECK(women && world.army(women)->soldierCount() == 3);
        for (auto sid : world.army(women)->soldiers())
        {
            PALADIN_CHECK(
                city->simulationState()
                    .citizens()
                    .citizen(world.soldier(sid)->sourceCitizenId())
                    ->sex == CitizenSex::Female
            );
        }
        PALADIN_CHECK(
            MilitarySystem::disbandUnit(world, f.owner, women) ==
            MilitaryResult::Success
        );
        PALADIN_CHECK(
            city->simulationState().stockpile().setAmount("bread", 400)
        );
        PALADIN_CHECK(
            city->simulationState().stockpile().setAmount("rations", 0)
        );
        PALADIN_CHECK(AiRealmSystem::garrisonTarget(world, *realm, *city) == 0);
        std::cout << "[pr30/strategy] bounded real AI soldiers, trait/gender "
                     "eligibility, civilian and supply conservation, canonical "
                     "demobilization passed\n";
    }
    void demographicFraction()
    {
        Fixture f;
        auto& city = *f.world.settlement(f.home);
        auto& state = city.simulationState();
        DemographicRates rates;
        rates.annualBirthsPerPerson = 0;
        rates.annualDeathsPerPerson = 0;
        rates.annualDeathsAtZeroNeedFulfillmentPerPerson = 0;
        rates.maximumAnnualSurplusBirthsPerPerson = 0;
        rates.annualNetMigration = -.75;
        state.population().setRates(rates);
        const SettlementSimulationStep step{
            f.home,
            SettlementSimulationTier::Strategic,
            SettlementSimulationResolution::InactiveLocalAggregate,
            525600
        };
        const WorldSimulationStep year{525600, std::span(&step, 1)};
        SettlementPopulationSystem{}.tick(f.world, year);
        PALADIN_CHECK(city.population() == 200);
        const auto id = MilitarySystem::maintainStrategicGarrison(
            f.world,
            f.owner,
            f.home,
            1
        );
        PALADIN_CHECK(city.population() == 199);
        PALADIN_CHECK(
            MilitarySystem::disbandUnit(f.world, f.owner, id) ==
            MilitaryResult::Success
        );
        PALADIN_CHECK(city.population() == 200);
        SettlementPopulationSystem{}.tick(f.world, year);
        PALADIN_CHECK(
            city.population() == 199
        ); // -1.5 demographic people, not reset/swallowed by +/-1 transfers.
    }
    void decisionsAndSupply()
    {
        Fixture f;
        const auto count = f.people();
        const double supplies = f.food();
        auto& world = f.world;
        for (auto id : {f.owner, f.neighbor, f.far})
        {
            world.realm(id)->ruler.personality.isolationism = RulerAxis{0};
        }
        const auto treasury = world.realm(f.owner)->treasury->balance +
                              world.realm(f.neighbor)->treasury->balance +
                              world.realm(f.far)->treasury->balance;
        AiRealmSystem::tick(world, 360, 0);
        AiRealmSystem::tick(
            world,
            360,
            std::numeric_limits<double>::quiet_NaN()
        );
        PALADIN_CHECK(world.armies().empty() && world.shipments().empty());
        AiRealmSystem::tick(world, 360, 1);
        PALADIN_CHECK(
            f.people() == count && f.food() == supplies &&
            !world.armies().empty()
        );
        PALADIN_CHECK(
            std::any_of(
                world.armies().begin(),
                world.armies().end(),
                [](const auto& unit)
                { return !unit.garrisoned() && unit.soldierCount() > 0; }
            )
        );
        PALADIN_CHECK(
            world.realm(f.player)->strategyDecisions == 0 &&
            world.settlement(f.playerCity)->population() == 200
        );
        PALADIN_CHECK(
            world.shipments().size() == 1 &&
            world.shipments().front().aiManaged &&
            world.shipments().front().destination == f.fort
        );
        const auto route = world.shipments().front().id;
        PALADIN_CHECK(world.shipment(route)->cargo > 0);
        const auto* relation = world.diplomacy().between(f.owner, f.neighbor);
        PALADIN_CHECK(
            relation && relation->allied && relation->trading &&
            !relation->atWar
        );
        PALADIN_CHECK(
            !world.diplomacy().between(f.owner, f.far) &&
            !world.diplomacy().between(f.owner, f.player)
        );
        PALADIN_CHECK(
            world.realm(f.owner)->treasury->balance +
                world.realm(f.neighbor)->treasury->balance +
                world.realm(f.far)->treasury->balance ==
            treasury
        );
        const auto decisions = world.realm(f.owner)->strategyDecisions;
        AiRealmSystem::tick(world, 361, 1);
        PALADIN_CHECK(world.realm(f.owner)->strategyDecisions == decisions);
        WorldShipmentSystem::tick(world, 361, 120);
        PALADIN_CHECK(
            world.shipment(route)->deliveries == 1 && f.food() == supplies
        );
        // A standing AI route cannot drain protected civilian food on rapid
        // repeats.
        WorldShipmentSystem::tick(world, 481, 100000);
        PALADIN_CHECK(
            WorldShipmentSystem::available(
                *world.settlement(f.home),
                "bread"
            ) >= 2 * world.settlement(f.home)->population()
        );
        PALADIN_CHECK(f.food() == supplies);
        PALADIN_CHECK(world.assignSettlementToRealm(f.fort, f.player));
        AiRealmSystem::tick(world, 14400, 1);
        PALADIN_CHECK(!world.shipment(route)->repeating);
        PALADIN_CHECK(f.people() == count && f.food() == supplies);
        std::cout << "[pr30/strategy] pause/cadence, range-gated AI alliances "
                     "and real intercity supply with protected food passed\n";
    }
    void longRunMilitaryScheduling()
    {
        Fixture f{20200};
        auto& world = f.world;
        auto& state = world.settlement(f.home)->simulationState();
        PALADIN_CHECK(state.stockpile().setAmount("bread", 1000000));
        ArmyId guard;
        for (int day = 0; day < 68; ++day)
        {
            guard = MilitarySystem::maintainStrategicGarrison(
                world,
                f.owner,
                f.home,
                384
            );
        }
        PALADIN_CHECK(guard && world.army(guard)->soldierCount() == 384);
        PALADIN_CHECK(state.citizens().citizens().size() == 384);
        MilitarySystem::synchronize(world, 68 * 1440);
        const auto reconciliations = MilitarySystem::rosterRebuilds(world);
        const auto updates = MilitarySystem::personnelUpdates(world);
        for (int frame = 0; frame < 600; ++frame)
        {
            MilitarySystem::tick(world, 68 * 1440 + frame * .08, .08);
        }
        PALADIN_CHECK(MilitarySystem::rosterRebuilds(world) == reconciliations);
        PALADIN_CHECK(MilitarySystem::personnelUpdates(world) - updates > 0);
        PALADIN_CHECK(
            MilitarySystem::personnelUpdates(world) - updates <= 4 * 384
        );
        // Repeated real recruitment/retirement cannot leave thousands of
        // rejected demographic samples in an otherwise aggregate city.
        for (int cycle = 0; cycle < 1000; ++cycle)
        {
            static_cast<void>(MilitarySystem::maintainStrategicGarrison(
                world,
                f.owner,
                f.home,
                376
            ));
            static_cast<void>(MilitarySystem::maintainStrategicGarrison(
                world,
                f.owner,
                f.home,
                384
            ));
        }
        PALADIN_CHECK(state.citizens().citizens().size() == 384);
        PALADIN_CHECK(world.army(guard)->soldierCount() == 384);
        PALADIN_CHECK(
            MilitarySystem::orderMove(world, f.owner, guard, {32, 44}) ==
            MilitaryResult::Success
        );
        MilitarySystem::synchronize(world, 69 * 1440, false);
        const auto departureRebuilds = MilitarySystem::rosterRebuilds(world);
        for (int minute = 0; minute < 30; ++minute)
        {
            MilitarySystem::tick(world, 69 * 1440 + minute, 1);
        }
        PALADIN_CHECK(world.army(guard)->moving());
        PALADIN_CHECK(
            (world.army(guard)->position() != WorldTilePosition{32, 32})
        );
        PALADIN_CHECK(
            MilitarySystem::rosterRebuilds(world) == departureRebuilds
        );
        std::cout << "[stability] day-68 garrison: unchanged roster reused; "
                     "staggered real needs, 1000 conserved recruitment cycles "
                     "passed\n";
    }
    void populationField()
    {
        Fixture f;
        auto& world = f.world;
        const WorldPopulationField first{world};
        double census = 0;
        for (const auto& city : world.settlements())
        {
            census += city.population();
        }
        PALADIN_CHECK(std::abs(first.total() - census) < .001);
        PALADIN_CHECK(
            first.at({32, 32}) > first.at({30, 32}) && first.at({64, 32}) == 0
        );
        // Water removes footprint cells, never loses a person's census mass.
        world.grid().tile({33, 32})->terrain = TerrainType::Water;
        world.grid().terrainChanged();
        const WorldPopulationField coast{world};
        PALADIN_CHECK(
            coast.at({33, 32}) == 0 && std::abs(coast.total() - census) < .001
        );
        const auto fingerprint = WorldPopulationField::fingerprint(world);
        PALADIN_CHECK(world.assignSettlementToRealm(f.home, f.player));
        const WorldPopulationField conquered{world};
        PALADIN_CHECK(WorldPopulationField::fingerprint(world) == fingerprint);
        for (int y = 0; y < 64; ++y)
        {
            for (int x = 0; x < 128; ++x)
            {
                PALADIN_CHECK(conquered.at({x, y}) == coast.at({x, y}));
            }
        }
        auto p = defaultSettlementFoundationProfile();
        p.initialPopulation = 200;
        p.initialDetailedCitizenCount = 0;
        const auto seam = world.createSettlement({0, 16});
        PALADIN_CHECK(seam);
        PALADIN_CHECK(world.settlement(seam)->simulationState().bootstrap(p));
        const double seamPeople = world.settlement(seam)->population();
        const WorldPopulationField wrapped{world};
        PALADIN_CHECK(
            wrapped.at({127, 16}) > 0 &&
            wrapped.at({-1, 16}) == wrapped.at({127, 16})
        );
        PALADIN_CHECK(std::abs(wrapped.total() - (census + seamPeople)) < .001);
        for (double n : {0., 1., 4., 16., 64., 256., 1024.})
        {
            PALADIN_CHECK(populationDensityColor(n).alpha < 255);
        }
        PALADIN_CHECK(
            GovernmentCivic.alpha == 255 && GovernmentTribal.alpha == 255
        );
        std::cout << "[pr30/population] geographic census mass, coast and "
                     "longitude seam, ownership independence and "
                     "terrain-preserving ink passed\n";
    }
} // namespace
void runPr30StrategyTests()
{
    guardsAndDemobilization();
    demographicFraction();
    decisionsAndSupply();
    longRunMilitaryScheduling();
    populationField();
}

#pragma once

#include "TestFramework.h"
#include "simulation/AiRealmSystem.h"
#include "simulation/MilitarySystem.h"
#include "simulation/WorldLandNavigation.h"
#include "simulation/WorldMarketSystem.h"
#include "simulation/WorldShipmentSystem.h"
#include "world/settlements/SettlementSimulationPolicy.h"
#include "world/settlements/StrategicFood.h"
#include <set>

namespace Paladin::Test::Pr32
{
    inline WorldGenerationSettings settings()
    {
        WorldGenerationSettings result;
        result.width = 128;
        result.height = 64;
        result.seed = 320021;
        result.populateAiRealms = false;
        return result;
    }

    struct Fixture
    {
        World world{settings()};
        RealmId buyer, seller;
        SettlementId destination, source;
        Fixture()
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
            buyer = world.createRealm();
            seller = world.createRealm();
            auto profile = defaultSettlementFoundationProfile();
            profile.initialPopulation = 200;
            profile.initialDetailedCitizenCount = 0;
            profile.initialResources = {};
            profile.citizenSeed = 320021;
            destination = world.foundCapitalSettlement(
                {32, 32}, buyer, {"Buyer", "Buyer Folk", "Market", {}, "civic"}, profile);
            source = world.foundCapitalSettlement(
                {44, 32}, seller, {"Seller", "Seller Folk", "Supplier", {}, "civic"}, profile);
            PALADIN_CHECK(destination && source);
            for (const auto realmId : {buyer, seller})
            {
                auto* realm = world.realm(realmId);
                realm->aiControlled = true;
                realm->nextMarketMinute = 100000;
                realm->nextDiplomacyMinute = 100000;
            }
            world.realm(buyer)->nextMarketMinute = 0;
            world.realm(buyer)->treasury->balance = 100000;
            world.realm(seller)->treasury->balance = 0; // Exporters need goods, not gold.
            world.diplomacy().relations.push_back({buyer, seller, false, true, false});
            for (int i = 1; i <= 8; ++i)
            {
                const auto empty = world.foundSettlement({32 + i, 32}, seller, profile);
                PALADIN_CHECK(empty);
                PALADIN_CHECK(world.settlement(empty)->simulationState().economy().configure({}));
            }
        }
        double goods(std::string_view resource) const
        {
            double result = 0;
            for (const auto& city : world.settlements())
                result += WorldShipmentSystem::total(city, resource);
            for (const auto& route : world.shipments())
                if (route.resource == resource) result += route.cargo;
            return result;
        }
    };

    inline void namedGoodsBeyondEight()
    {
        for (const auto resource : {"lumber", "stone", "coal", "iron", "bread", "fish"})
        {
            Fixture f;
            auto& supply = f.world.settlement(f.source)->simulationState();
            auto& demand = f.world.settlement(f.destination)->simulationState();
            PALADIN_CHECK(supply.economy().configure({{resource, 2, .1, 0}}));
            PALADIN_CHECK(supply.stockpile().setAmount(resource, 1000));
            PALADIN_CHECK(demand.economy().configure({{resource, 0, 1, 0}}));
            const auto before = f.goods(resource);
            const auto money = f.world.realm(f.buyer)->treasury->balance;
            WorldMarketSystem::tick(f.world);
            PALADIN_CHECK(f.world.shipments().size() == 1);
            const auto routeId = f.world.shipments().front().id;
            const auto& route = *f.world.shipment(routeId);
            PALADIN_CHECK(route.source == f.source && route.destination == f.destination);
            PALADIN_CHECK(route.resource == resource && route.cargo > 0);
            PALADIN_CHECK(f.goods(resource) == before);
            // Housekeeping must not mistake paid foreign cargo for invalid own-city logistics.
            AiRealmSystem::tick(f.world, 0, 1);
            PALADIN_CHECK(f.world.shipment(routeId)->active());
            WorldShipmentSystem::tick(f.world, 1, 121);
            PALADIN_CHECK(f.world.shipment(routeId)->deliveries == 1);
            PALADIN_CHECK(demand.stockpile().amount(resource) > 0);
            // Avoid food accounting here: AI garrisons legitimately convert food into rations.
            if (!SettlementResourceCatalog::definition(resource)->edible)
                PALADIN_CHECK(f.goods(resource) == before);
            PALADIN_CHECK(f.world.realm(f.buyer)->treasury->balance < money);
            PALADIN_CHECK(f.world.realm(f.buyer)->treasury->balance +
                          f.world.realm(f.seller)->treasury->balance == money);
        }
    }

    inline void patrolNavigation(bool seam)
    {
        Fixture f;
        // No commerce or other realm decisions should obscure patrol conservation.
        f.world.diplomacy().relations.clear();
        f.world.realm(f.seller)->aiControlled = false;
        auto* realm = f.world.realm(f.buyer);
        realm->ruler.personality.militarism = RulerAxis{1};
        realm->nextStrategyMinute = 0;
        const WorldTilePosition home = seam ? WorldTilePosition{0, 32} : WorldTilePosition{32, 32};
        PALADIN_CHECK(f.world.setSettlementPosition(f.destination, home));
        auto& state = f.world.settlement(f.destination)->simulationState();
        PALADIN_CHECK(state.stockpile().setAmount("bread", 2000));
        // A nearby owned settlement across impassable water used to permanently
        // consume the patrol's only order. It must fall back to a dry local walk.
        if (!seam)
        {
            auto profile = defaultSettlementFoundationProfile();
            profile.initialPopulation = 0;
            profile.initialDetailedCitizenCount = 0;
            profile.initialResources = {};
            const auto isolated = f.world.foundSettlement({36, 32}, f.buyer, profile);
            PALADIN_CHECK(isolated);
            for (int y = 31; y <= 33; ++y)
                for (int x = 35; x <= 37; ++x)
                    if (x != 36 || y != 32)
                        f.world.grid().tile({x, y})->terrain = TerrainType::Water;
        }
        else
        {
            // The only traversable exit is west through the longitude seam.
            f.world.grid().tile({1, 32})->terrain = TerrainType::Water;
            f.world.grid().tile({0, 31})->terrain = TerrainType::Water;
            f.world.grid().tile({0, 33})->terrain = TerrainType::Water;
        }
        f.world.grid().terrainChanged();
        const auto foodBefore = civilianFood(state.stockpile());
        const auto peopleBefore = f.world.settlement(f.destination)->population();
        AiRealmSystem::tick(f.world, 0, 1);
        const Army* patrol = nullptr;
        std::set<SoldierId> roster;
        double supplies = civilianFood(state.stockpile()) + state.stockpile().amount("rations");
        for (const auto& unit : f.world.armies())
        {
            if (unit.ownerRealmId() != f.buyer) continue;
            supplies += unit.rations();
            for (const auto id : unit.soldiers())
            {
                PALADIN_CHECK(roster.insert(id).second);
                PALADIN_CHECK(f.world.soldier(id)->unitId() == unit.id());
            }
            if (!unit.garrisoned()) patrol = &unit;
        }
        PALADIN_CHECK(patrol && patrol->soldierCount() == 4 && patrol->moving());
        PALADIN_CHECK(f.world.settlement(f.destination)->population() + roster.size() == peopleBefore);
        PALADIN_CHECK(supplies == foodBefore);
        const auto id = patrol->id();
        MilitarySystem::tick(f.world, 1, 16);
        patrol = f.world.army(id);
        PALADIN_CHECK(patrol && patrol->position() != home);
        PALADIN_CHECK(f.world.grid().tile(patrol->position())->terrain == TerrainType::Land);
        if (seam) PALADIN_CHECK(patrol->position().x >= 125);
    }

    inline void run()
    {
        PALADIN_CHECK(!SettlementResourceCatalog::definition("food"));
        PALADIN_CHECK(!SettlementResourceCatalog::definition("materials"));
        const auto policies = defaultSettlementSimulationPolicies();
        PALADIN_CHECK(policies.inactive.minimumStepMinutes == 60);
        PALADIN_CHECK(policies.strategic.minimumStepMinutes == 60);
        namedGoodsBeyondEight();
        patrolNavigation(false);
        patrolNavigation(true);
    }
}

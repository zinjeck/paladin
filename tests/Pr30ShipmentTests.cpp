#include "TestFramework.h"
#include "simulation/AiRealmSystem.h"
#include "simulation/DiplomacySystem.h"
#include "simulation/Simulation.h"
#include "simulation/WorldLandNavigation.h"
#include "simulation/WorldMarketSystem.h"
#include "simulation/WorldShipmentSystem.h"
#include "simulation/systems/SettlementEconomySystem.h"
#include "world/World.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include <cmath>
#include <iostream>
#include <limits>

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
            s.seed = 300031;
            s.populateAiRealms = false;
            return s;
        }
        Simulation sim{settings()};
        World& world = sim.world();
        RealmId owner = sim.playerRealmId(), foreign;
        SettlementId source, destination;
        Fixture()
        {
            for (int y = 0; y < world.grid().height(); ++y)
            {
                for (int x = 0; x < world.grid().width(); ++x)
                {
                    auto& t = *world.grid().tile({x, y});
                    t.terrain = TerrainType::Land;
                    t.biome = BiomeType::Plain;
                }
            }
            world.grid().terrainChanged();
            source = sim.foundPlayerCapital(
                {24, 32},
                {"Logistics", "Cargo Folk", "Origin", {}, "civic"}
            );
            destination = world.foundSettlement(
                {48, 32},
                owner,
                playerSettlementFoundationProfile(300032)
            );
            foreign = world.createRealm();
            PALADIN_CHECK(source && destination);
            PALADIN_CHECK(world.settlement(source)
                              ->simulationState()
                              .stockpile()
                              .setAmount("lumber", 1000));
        }
        double goods() const
        {
            double total = 0;
            for (const auto& city : world.settlements())
            {
                total += WorldShipmentSystem::total(city, "lumber");
            }
            for (const auto& route : world.shipments())
            {
                if (route.resource == "lumber")
                {
                    total += route.cargo;
                }
            }
            return total;
        }
        ShipmentId send(bool repeat = false, int amount = 20)
        {
            ShipmentId id;
            PALADIN_CHECK(
                WorldShipmentSystem::create(
                    world,
                    owner,
                    source,
                    destination,
                    "lumber",
                    amount,
                    repeat,
                    &id
                ) == ShipmentResult::Success
            );
            PALADIN_CHECK(id);
            return id;
        }
        SettlementMap& physical(SettlementId id)
        {
            SettlementMapGenerationSettings options;
            options.localTilesPerWorldTile = 4;
            PALADIN_CHECK(sim.prepareSettlementMap(id, options));
            auto& map = *sim.settlementMap(id);
            for (int y = 0; y < map.grid().height(); ++y)
            {
                for (int x = 0; x < map.grid().width(); ++x)
                {
                    map.grid().tile({x, y})->terrain = TerrainType::Land;
                }
            }
            auto definition = *SettlementObjectCatalog::definition("city_keep");
            definition.bypassesConstruction = true;
            PALADIN_CHECK(map.objectState().placeCompletedObject(
                map.grid(),
                definition,
                {{2, 2}, 5, 7}
            ));
            map.naturalFeatures().clear({{2, 2}, 5, 7});
            map.logistics.synchronize(map.objectState(), 360);
            return map;
        }
        InventoryId depot(SettlementMap& map)
        {
            auto definition =
                *SettlementObjectCatalog::definition("trade_depot");
            definition.bypassesConstruction = true;
            PALADIN_CHECK(map.objectState().placeCompletedObject(
                map.grid(),
                definition,
                {{10, 10}, 8, 5}
            ));
            map.naturalFeatures().clear({{10, 10}, 8, 5});
            map.logistics.synchronize(map.objectState(), 360);
            return map.logistics.forObject(
                map.objectState().completedObjects().back().id
            );
        }
    };
    void oneOffAndRepeat()
    {
        Fixture f;
        const auto total = f.goods();
        const auto id = f.send();
        PALADIN_CHECK(f.goods() == total && f.world.shipment(id)->cargo == 20);
        PALADIN_CHECK(
            f.world.settlement(f.destination)
                ->simulationState()
                .stockpile()
                .amount("lumber") == 0
        );
        WorldShipmentSystem::tick(f.world, 360, 0);
        WorldShipmentSystem::tick(
            f.world,
            360,
            std::numeric_limits<double>::quiet_NaN()
        );
        PALADIN_CHECK(f.world.shipment(id)->tileIndex == 0);
        WorldShipmentSystem::tick(f.world, 360, 5);
        PALADIN_CHECK(
            f.world.shipment(id)->tileIndex == 0 &&
            f.world.shipment(id)->visualX() > 24 &&
            f.world.shipment(id)->visualX() < 25
        );
        WorldShipmentSystem::tick(f.world, 365, 235);
        PALADIN_CHECK(
            f.world.shipment(id)->deliveries == 1 &&
            f.world.shipment(id)->cargo == 0
        );
        PALADIN_CHECK(
            f.world.shipment(id)->phase == ShipmentPhase::Returning &&
            f.goods() == total
        );
        WorldShipmentSystem::tick(f.world, 600, 240);
        PALADIN_CHECK(
            !f.world.shipment(id)->active() &&
            f.world.shipment(id)->position() ==
                f.world.settlement(f.source)->position()
        );
        const auto repeat = f.send(true, 30);
        WorldShipmentSystem::tick(f.world, 840, 480 * 3 + 240);
        PALADIN_CHECK(
            f.world.shipment(repeat)->deliveries == 4 && f.goods() == total
        );
        PALADIN_CHECK(
            WorldShipmentSystem::stop(f.world, f.owner, repeat) ==
            ShipmentResult::Success
        );
        WorldShipmentSystem::tick(f.world, 2520, 10000);
        PALADIN_CHECK(
            !f.world.shipment(repeat)->active() &&
            f.world.shipment(repeat)->deliveries == 4 && f.goods() == total
        );
        std::cout << "[shipments] physical cargo, interpolation, pause, "
                     "one-off return and sustained trips conserve goods\n";
    }
    void authorizationAndFailures()
    {
        Fixture f;
        const auto total = f.goods();
        ShipmentId id;
        using R = ShipmentResult;
        PALADIN_CHECK(
            WorldShipmentSystem::create(
                f.world,
                f.foreign,
                f.source,
                f.destination,
                "lumber",
                1,
                false
            ) == R::NotOwned
        );
        PALADIN_CHECK(
            WorldShipmentSystem::create(
                f.world,
                f.owner,
                f.source,
                f.source,
                "lumber",
                1,
                false
            ) == R::SameSettlement
        );
        PALADIN_CHECK(
            WorldShipmentSystem::create(
                f.world,
                f.owner,
                f.source,
                f.destination,
                "unknown",
                1,
                false
            ) == R::InvalidResource
        );
        for (int amount : {0, -1, std::numeric_limits<int>::max()})
        {
            PALADIN_CHECK(
                WorldShipmentSystem::create(
                    f.world,
                    f.owner,
                    f.source,
                    f.destination,
                    "lumber",
                    amount,
                    false
                ) == R::InvalidAmount
            );
        }
        PALADIN_CHECK(
            WorldShipmentSystem::create(
                f.world,
                f.owner,
                f.source,
                f.destination,
                "lumber",
                1001,
                false
            ) == R::InsufficientGoods
        );
        PALADIN_CHECK(f.goods() == total && f.world.shipments().empty());
        id = f.send(true);
        PALADIN_CHECK(
            WorldShipmentSystem::stop(f.world, f.foreign, id) == R::NotOwned
        );
        PALADIN_CHECK(
            f.world.assignSettlementToRealm(f.destination, f.foreign)
        );
        WorldShipmentSystem::tick(f.world, 360, 1000);
        PALADIN_CHECK(
            !f.world.shipment(id)->active() &&
            f.world.shipment(id)->deliveries == 0 && f.goods() == total
        );
        PALADIN_CHECK(
            f.world.settlement(f.source)->simulationState().stockpile().amount(
                "lumber"
            ) == 1000
        );
        PALADIN_CHECK(f.world.assignSettlementToRealm(f.destination, f.owner));
        id = f.send();
        f.world.grid().tile({25, 32})->terrain = TerrainType::Water;
        f.world.grid().terrainChanged();
        WorldShipmentSystem::tick(f.world, 1360, 100);
        PALADIN_CHECK(
            f.world.shipment(id)->phase == ShipmentPhase::Blocked &&
            f.world.shipment(id)->cargo == 20 && f.goods() == total
        );
        PALADIN_CHECK(
            WorldShipmentSystem::stop(f.world, f.owner, id) == R::Success
        );
        PALADIN_CHECK(!f.world.shipment(id)->active() && f.goods() == total);
        for (const auto p :
             {WorldTilePosition{47, 32}, {49, 32}, {48, 31}, {48, 33}})
        {
            f.world.grid().tile(p)->terrain = TerrainType::Water;
        }
        PALADIN_CHECK(
            WorldShipmentSystem::create(
                f.world,
                f.owner,
                f.source,
                f.destination,
                "lumber",
                1,
                false
            ) == R::NoLandRoute
        );
        PALADIN_CHECK(f.goods() == total);
        // Seam-neighbouring tiles are one step apart, not a tour around the
        // planet.
        const auto path = worldLandRoute(f.world.grid(), {0, 16}, {127, 16});
        PALADIN_CHECK(path && path->size() == 2);
        std::cout << "[shipments] ownership, quantities, blocked routes, "
                     "captured targets and longitude seam passed\n";
    }
    void crossRealmTrade()
    {
        Fixture f;
        // Keep the foreign partner within the real diplomatic range.
        PALADIN_CHECK(f.world.setSettlementPosition(f.destination, {36, 32}));
        PALADIN_CHECK(
            f.world.assignSettlementToRealm(f.destination, f.foreign)
        );
        auto& relations = const_cast<DiplomacyState&>(f.world.diplomacy());
        relations.relations.push_back({f.owner, f.foreign, false, true, false});
        f.world.realm(f.foreign)->treasury->balance = 10000;
        // An exporter needs cargo, never startup gold. Only the buyer pays.
        f.world.realm(f.owner)->treasury->balance = 0;
        const auto sellerCash = f.world.realm(f.owner)->treasury->balance;
        const auto total = f.goods();
        ShipmentId id;
        const auto send = [&]()
        {
            return WorldShipmentSystem::create(
                f.world,
                f.owner,
                f.source,
                f.destination,
                "lumber",
                20,
                false,
                &id,
                false,
                f.foreign,
                25
            );
        };
        PALADIN_CHECK(send() == ShipmentResult::Success);
        PALADIN_CHECK(f.world.realm(f.owner)->treasury->balance == 0);
        PALADIN_CHECK(f.world.realm(f.foreign)->treasury->balance == 9500);
        PALADIN_CHECK(
            f.world.shipment(id)->escrow == 500 && f.goods() == total
        );
        WorldShipmentSystem::tick(f.world, 360, 120);
        PALADIN_CHECK(f.world.shipment(id)->deliveries == 1);
        PALADIN_CHECK(f.world.shipment(id)->escrow == 0);
        PALADIN_CHECK(
            f.world.realm(f.owner)->treasury->balance == sellerCash + 500
        );
        PALADIN_CHECK(f.goods() == total);
        WorldShipmentSystem::tick(f.world, 480, 120);
        PALADIN_CHECK(send() == ShipmentResult::Success);
        relations.relations.back().trading = false;
        WorldShipmentSystem::tick(f.world, 600, 240);
        PALADIN_CHECK(
            f.world.shipment(id)->deliveries == 0 && f.goods() == total
        );
        PALADIN_CHECK(f.world.realm(f.foreign)->treasury->balance == 9500);
        PALADIN_CHECK(f.world.shipment(id)->escrow == 0);
        PALADIN_CHECK(send() == ShipmentResult::TradeAgreementRequired);
        relations.relations.back().trading = true;
        f.world.realm(f.foreign)->treasury->balance = 499;
        PALADIN_CHECK(send() == ShipmentResult::InsufficientMoney);
        PALADIN_CHECK(
            f.world.realm(f.foreign)->treasury->balance == 499 &&
            f.goods() == total
        );
        // Quotes leave a food reserve and limit individual purchases/sales.
        SettlementEconomy economy;
        PALADIN_CHECK(
            economy.configure(std::vector<ResourceFlowRate>{{"bread", 2, 2, 1}})
        );
        ResourceStockpile stock;
        PALADIN_CHECK(stock.setAmount("bread", 2000));
        const auto offer = economy.quote(stock, 100, "bread");
        PALADIN_CHECK(
            offer.reserve == 1200 && offer.offered == 200 && offer.wanted == 0
        );
        PALADIN_CHECK(stock.setAmount("bread", 0));
        const auto demand = economy.quote(stock, 100, "bread");
        PALADIN_CHECK(
            demand.offered == 0 && demand.wanted == 300 &&
            demand.unitPrice > offer.unitPrice
        );
        std::cout << "[trade] escrow, payment, treaty loss, affordability and "
                     "protected reserves passed\n";
    }
    void localInventoriesAndOverflow()
    {
        Fixture f;
        auto& from = f.physical(f.source);
        auto& to = f.physical(f.destination);
        const auto keep = from.logistics.forObject(
            from.objectState().completedObjects().front().id
        );
        PALADIN_CHECK(
            WorldShipmentSystem::create(
                f.world,
                f.owner,
                f.source,
                f.destination,
                "lumber",
                1,
                false
            ) == ShipmentResult::MissingTradeDepot
        );
        const auto sourceDepot = f.depot(from), destinationDepot = f.depot(to);
        PALADIN_CHECK(
            from.logistics.moveAvailable(keep, sourceDepot, "lumber", 40) == 40
        );
        // Emptying the founding cache must never make it a receiving container.
        PALADIN_CHECK(from.logistics.freeSpace(keep) == 0);
        PALADIN_CHECK(!from.logistics.add(keep, "lumber", 1));
        PALADIN_CHECK(
            from.logistics.moveAvailable(sourceDepot, keep, "lumber", 1) == 0
        );
        const auto reserve = from.logistics.drop({8, 8}, "lumber", 5, 360);
        PALADIN_CHECK(
            !from.logistics
                 .reserve(CitizenId{998}, sourceDepot, keep, "lumber", 1)
        );
        PALADIN_CHECK(
            from.logistics
                .reserve(CitizenId{999}, sourceDepot, reserve, "lumber", 10)
        );
        PALADIN_CHECK(
            WorldShipmentSystem::available(
                *f.world.settlement(f.source),
                "lumber"
            ) == 30
        );
        const auto destinationImports = to.logistics.importsForObject(
            to.logistics.inventory(destinationDepot)->objectId
        );
        PALADIN_CHECK(
            destinationImports && destinationImports != destinationDepot
        );
        PALADIN_CHECK(to.logistics.add(
            destinationImports,
            "stone",
            to.logistics.freeSpace(destinationImports)
        ));
        // Owned transfers may be relayed directly from their public counter,
        // even when the separate foreign-import counter is completely full.
        PALADIN_CHECK(to.logistics.add(
            destinationDepot,
            "stone",
            to.logistics.freeSpace(destinationDepot) - 7
        ));
        const auto total = f.goods();
        const auto id = f.send(false, 30);
        PALADIN_CHECK(
            from.logistics.inventory(sourceDepot)->amount("lumber") == 10
        ); // claimed goods stay put
        WorldShipmentSystem::tick(f.world, 360, 240);
        PALADIN_CHECK(
            f.goods() == total && f.world.shipment(id)->deliveries == 1
        );
        int piles = 0;
        for (const auto& inventory : to.logistics.inventories())
        {
            if (inventory.kind == InventoryKind::Groundpile)
            {
                piles += inventory.amount("lumber");
                PALADIN_CHECK(
                    inventory.footprint.topLeft.x >= 9
                ); // near the receiving depot
            }
        }
        PALADIN_CHECK(piles == 23); // overflow remains physical
        PALADIN_CHECK(
            to.logistics.inventory(destinationDepot)->amount("lumber") == 7
        );
        PALADIN_CHECK(
            to.logistics.inventory(destinationImports)->amount("lumber") == 0
        );
        PALADIN_CHECK(
            WorldShipmentSystem::available(
                *f.world.settlement(f.destination),
                "lumber"
            ) == 7
        ); // received own-city goods remain available to relay
        PALADIN_CHECK(
            WorldShipmentSystem::create(
                f.world,
                f.owner,
                f.source,
                f.destination,
                "lumber",
                1,
                true
            ) == ShipmentResult::InsufficientGoods
        ); // reserved 10 cannot be taken
    }
    void depotOrdersAndImports()
    {
        Fixture f;
        PALADIN_CHECK(f.world.setSettlementPosition(f.destination, {36, 32}));
        PALADIN_CHECK(
            f.world.assignSettlementToRealm(f.destination, f.foreign)
        );
        auto& map = f.physical(f.source);
        const auto exports = f.depot(map);
        const auto depot = map.logistics.inventory(exports)->objectId;
        const auto imports = map.logistics.importsForObject(depot);
        PALADIN_CHECK(exports && imports && exports != imports);
        PALADIN_CHECK(f.world.settlement(f.destination)
                          ->simulationState()
                          .stockpile()
                          .setAmount("lumber", 1000));
        f.world.realm(f.owner)->treasury->balance = 10000;
        const auto original = f.goods();
        const auto cash = f.world.realm(f.owner)->treasury->balance +
                          f.world.realm(f.foreign)->treasury->balance;
        PALADIN_CHECK(
            WorldMarketSystem::dispatch(
                f.world,
                f.owner,
                f.source,
                depot,
                "lumber",
                TradeDirection::Import,
                5
            ) == ShipmentResult::TradeAgreementRequired
        );
        auto& diplomacy = const_cast<DiplomacyState&>(f.world.diplomacy());
        diplomacy.relations.push_back({f.owner, f.foreign, false, true, false});
        const auto offer = WorldMarketSystem::depotOffer(
            f.world,
            f.source,
            depot,
            "lumber",
            TradeDirection::Import
        );
        PALADIN_CHECK(
            offer.partner == f.destination && offer.unitPrice > 0 &&
            offer.available >= 5
        );
        ShipmentId shipment;
        PALADIN_CHECK(
            WorldMarketSystem::dispatch(
                f.world,
                f.owner,
                f.source,
                depot,
                "lumber",
                TradeDirection::Import,
                5,
                &shipment
            ) == ShipmentResult::Success
        );
        const auto* route = f.world.shipment(shipment);
        const auto duration =
            route->minutesPerTile * double(route->path.size() - 1);
        PALADIN_CHECK(duration >= 20 && duration <= 60);
        PALADIN_CHECK(f.goods() == original);
        PALADIN_CHECK(
            f.world.realm(f.owner)->treasury->balance +
                f.world.realm(f.foreign)->treasury->balance + route->escrow ==
            cash
        );
        WorldShipmentSystem::tick(f.world, 360, duration * 2);
        PALADIN_CHECK(map.logistics.inventory(imports)->amount("lumber") == 5);
        PALADIN_CHECK(map.logistics.inventory(exports)->amount("lumber") == 0);
        PALADIN_CHECK(
            WorldShipmentSystem::available(
                *f.world.settlement(f.source),
                "lumber"
            ) == 0
        );
        PALADIN_CHECK(f.goods() == original);
        PALADIN_CHECK(
            f.world.realm(f.owner)->treasury->balance +
                f.world.realm(f.foreign)->treasury->balance ==
            cash
        );
        PALADIN_CHECK(!map.trade.visits.empty());
        for (const auto& visit : map.trade.visits)
        {
            PALADIN_CHECK(
                visit.path.size() > 1 && visit.quantity == 5 && visit.importing
            );
            for (std::size_t index = 1; index < visit.path.size(); ++index)
            {
                SettlementNavigation navigation;
                PALADIN_CHECK(navigation.canStep(
                    map,
                    visit.path[index - 1],
                    visit.path[index]
                ));
            }
        }
        PALADIN_CHECK(
            WorldMarketSystem::setOrder(
                f.world,
                f.owner,
                f.source,
                depot,
                "lumber",
                TradeDirection::Import,
                7,
                true
            )
        );
        PALADIN_CHECK(
            map.trade.orders.size() == 1 && map.trade.orders.front().enabled
        );
        WorldMarketSystem::tickOrders(f.world, 1000);
        PALADIN_CHECK(map.trade.orders.front().shipment);
        const auto orderRoute = map.trade.orders.front().shipment;
        PALADIN_CHECK(f.world.shipment(orderRoute)->cargo == 7);
        diplomacy.relations.back().trading = false;
        WorldShipmentSystem::tick(f.world, 1000, 1000);
        PALADIN_CHECK(f.world.shipment(orderRoute)->deliveries == 0);
        PALADIN_CHECK(f.goods() == original);
        PALADIN_CHECK(
            f.world.realm(f.owner)->treasury->balance +
                f.world.realm(f.foreign)->treasury->balance ==
            cash
        );
        PALADIN_CHECK(
            WorldMarketSystem::setOrder(
                f.world,
                f.owner,
                f.source,
                depot,
                "lumber",
                TradeDirection::Import,
                7,
                false
            )
        );
        PALADIN_CHECK(!map.trade.orders.front().enabled);
        map.trade.expire(1000000);
        PALADIN_CHECK(map.trade.visits.empty());
        std::cout << "[trade] live treaties, separate import stock, standing "
                     "orders, caravan paths and escrow conserve goods/cash\n";
    }
    void nearestEligiblePartnerBeyondEight()
    {
        Fixture f;
        PALADIN_CHECK(f.world.setSettlementPosition(f.destination, {36, 32}));
        PALADIN_CHECK(
            f.world.assignSettlementToRealm(f.destination, f.foreign)
        );
        f.world.diplomacy().relations.push_back(
            {f.owner, f.foreign, false, true, false}
        );
        std::vector<SettlementId> nearer;
        for (int i = 0; i < 8; ++i)
        {
            const auto id = f.world.createSettlement({25 + i, 32});
            PALADIN_CHECK(f.world.settlement(id)->simulationState().bootstrap(
                defaultSettlementFoundationProfile()
            ));
            PALADIN_CHECK(f.world.assignSettlementToRealm(id, f.foreign));
            PALADIN_CHECK(
                f.world.settlement(id)->simulationState().stockpile().setAmount(
                    "lumber",
                    0
                )
            );
            nearer.push_back(id);
        }
        auto& destination =
            f.world.settlement(f.destination)->simulationState();
        PALADIN_CHECK(destination.stockpile().setAmount("lumber", 1000));
        auto offer = WorldMarketSystem::depotOffer(
            f.world,
            f.source,
            {},
            "lumber",
            TradeDirection::Import,
            5
        );
        PALADIN_CHECK(
            offer.partner == f.destination && offer.treatyPartners == 9
        );
        for (const auto id : nearer)
        {
            PALADIN_CHECK(
                f.world.settlement(id)->simulationState().stockpile().setAmount(
                    "lumber",
                    100000
                )
            );
        }
        PALADIN_CHECK(destination.stockpile().setAmount("lumber", 0));
        PALADIN_CHECK(destination.economy().configure({{"lumber", 0, 1, 0}}));
        f.world.realm(f.foreign)->treasury->balance = 10000;
        offer = WorldMarketSystem::depotOffer(
            f.world,
            f.source,
            {},
            "lumber",
            TradeDirection::Export,
            5
        );
        PALADIN_CHECK(
            offer.partner == f.destination && offer.treatyPartners == 9
        );
        std::cout
            << "[trade] ninth eligible supplier and buyer remain reachable\n";
    }
    void cadenceAndWaiting()
    {
        Fixture a, b;
        const auto ia = a.send(true), ib = b.send(true);
        WorldShipmentSystem::tick(a.world, 360, 1300);
        for (int i = 0; i < 1300; ++i)
        {
            WorldShipmentSystem::tick(b.world, 360 + i, 1);
        }
        PALADIN_CHECK(a.goods() == b.goods());
        PALADIN_CHECK(
            a.world.shipment(ia)->deliveries == b.world.shipment(ib)->deliveries
        );
        PALADIN_CHECK(
            a.world.shipment(ia)->tileIndex == b.world.shipment(ib)->tileIndex
        );
        PALADIN_CHECK(
            a.world.shipment(ia)->cargo == b.world.shipment(ib)->cargo
        );
        PALADIN_CHECK(a.world.settlement(a.source)
                          ->simulationState()
                          .stockpile()
                          .setAmount("lumber", 0));
        WorldShipmentSystem::tick(a.world, 1660, 2000);
        PALADIN_CHECK(a.world.shipment(ia)->phase == ShipmentPhase::Waiting);
        PALADIN_CHECK(a.world.settlement(a.source)
                          ->simulationState()
                          .stockpile()
                          .setAmount("lumber", 20));
        WorldShipmentSystem::tick(a.world, 3660, 1);
        PALADIN_CHECK(
            a.world.shipment(ia)->phase == ShipmentPhase::Outbound &&
            a.world.shipment(ia)->cargo == 20
        );
        std::cout
            << "[shipments] partitioned time and supply-wait restart passed\n";
    }
} // namespace
void runPr30ShipmentTests()
{
    {
        PALADIN_CHECK(SettlementResourceCatalog::definition("food") == nullptr);
        PALADIN_CHECK(
            SettlementResourceCatalog::definition("materials") == nullptr
        );
        Fixture f;
        auto* buyer = f.world.realm(f.owner);
        buyer->aiControlled = true;
        const std::array<SettlementSimulationStep, 1> cities{
            {{f.destination,
              SettlementSimulationTier::Inactive,
              SettlementSimulationResolution::InactiveLocalAggregate,
              1}}
        };
        SettlementEconomySystem{}.tick(f.world, {1, cities});
        const auto* city = f.world.settlement(f.destination);
        const auto bread = WorldMarketSystem::quote(f.world, *city, "bread");
        const auto fish = WorldMarketSystem::quote(f.world, *city, "fish");
        PALADIN_CHECK(bread.dailyNeed > 0 && bread.wanted > 0);
        PALADIN_CHECK(fish.dailyNeed > 0 && fish.wanted > 0);
        for (int i = 0; i < 4; ++i)
        {
            WorldMarketSystem::updateHistory(f.world);
        }
        PALADIN_CHECK(
            f.world.marketHistory.resources.size() ==
            SettlementResourceCatalog::definitions().size()
        );
        PALADIN_CHECK(!f.world.marketHistory.resources.front().history.empty());
        PALADIN_CHECK(
            DiplomacySystem::apply(
                f.world,
                f.owner,
                f.foreign,
                DiplomaticAction::Trade
            ) == DiplomaticResult::OutOfRange
        );
    }

    {
        Fixture f;
        PALADIN_CHECK(f.world.setSettlementPosition(f.destination, {36, 32}));
        PALADIN_CHECK(
            f.world.assignSettlementToRealm(f.destination, f.foreign)
        );
        f.world.realm(f.owner)->aiControlled = true;
        f.world.realm(f.foreign)->aiControlled = true;
        f.world.realm(f.owner)->nextMarketMinute = 100000;
        f.world.realm(f.foreign)->nextMarketMinute = 0;
        f.world.realm(f.owner)->nextDiplomacyMinute = 100000;
        f.world.realm(f.foreign)->nextDiplomacyMinute = 100000;
        f.world.realm(f.foreign)->treasury->balance = 100000;
        f.world.diplomacy().relations.push_back(
            {f.owner, f.foreign, false, true, false}
        );
        auto& source = f.world.settlement(f.source)->simulationState();
        auto& destination =
            f.world.settlement(f.destination)->simulationState();
        PALADIN_CHECK(source.economy().configure({{"fish", 2, .1, 1}}));
        PALADIN_CHECK(source.stockpile().setAmount("fish", 1000));
        PALADIN_CHECK(destination.economy().configure({{"fish", 0, 1, 1}}));
        PALADIN_CHECK(destination.stockpile().setAmount("fish", 0));
        const auto before = f.world.realm(f.foreign)->treasury->balance;
        WorldMarketSystem::tick(f.world);
        PALADIN_CHECK(!f.world.shipments().empty());
        const auto route = f.world.shipments().back().id;
        PALADIN_CHECK(f.world.shipment(route)->resource == "fish");
        PALADIN_CHECK(f.world.shipment(route)->cargo > 0);
        // AI housekeeping must retain an authorized cross-realm shipment.
        AiRealmSystem::tick(f.world, 360, 1);
        WorldShipmentSystem::tick(f.world, 360, 120);
        PALADIN_CHECK(f.world.shipment(route)->deliveries == 1);
        PALADIN_CHECK(destination.stockpile().amount("fish") > 0);
        PALADIN_CHECK(f.world.realm(f.foreign)->treasury->balance < before);
    }
    oneOffAndRepeat();
    authorizationAndFailures();
    crossRealmTrade();
    depotOrdersAndImports();
    nearestEligiblePartnerBeyondEight();
    localInventoriesAndOverflow();
    cadenceAndWaiting();
}

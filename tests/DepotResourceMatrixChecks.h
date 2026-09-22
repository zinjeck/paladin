#pragma once
#include "CityCorrectionsChecks.h"
#include "world/settlements/DepotCollection.h"

namespace Paladin::Test::DepotRegression
{
    inline void allResources()
    {
        for (const auto& resource : SettlementResourceCatalog::definitions())
        {
            CityCorrections::TradeFixture f;
            auto& world = f.sim.world();
            auto& map = *f.map;
            auto& people = world.settlement(f.home)->simulationState().citizens();
            const std::string goods(resource.id);
            const auto original = map.logistics.forObject(f.storage);
            const auto previousGoods = map.logistics.inventory(original)->goods;
            for (const auto& item : previousGoods)
            { map.logistics.consumeAvailable(original, item.resource, item.amount); }
            const SettlementTilePosition far{map.grid().width() - 10, 15};
            PALADIN_CHECK(far.x - 20 > map.activities.policy.stockpile.collectionRadius);
            const auto storage = CityCorrections::complete(map,
                SettlementObjectTypes::Stockpile, {far, 5, 5});
            const auto source = map.logistics.forObject(storage);
            const auto counter = map.logistics.forObject(f.depot);
            PALADIN_CHECK(map.logistics.add(source, goods, 40, 0));
            PALADIN_CHECK(world.settlement(f.foreign)->simulationState().economy().configure({{goods,0,1,0}}));
            people.placeUnpositionedCitizens(map);
            map.employment().synchronize(map.objectState(), people);
            const auto job = map.employment().forObject(f.depot);
            PALADIN_CHECK(map.employment().adjust(job, 1, people));
            map.activities.policy.shiftStartMinute = 0;
            map.activities.policy.shiftEndMinute = 1440;
            map.activities.policy.hungerPerDay = 0;
            map.activities.policy.awakeEnergyPerMinute = 0;
            map.activities.policy.workEnergyPerMinute = 0;
            for (const auto& record : people.citizens())
            {
                auto& citizen = const_cast<SettlementCitizen&>(record);
                citizen.hunger = 0; citizen.energy = citizen.health = 100;
            }
            const auto carried = [&]()
            {
                int result = 0;
                for (const auto& c : people.citizens())
                    if (c.carriedResource == goods) { result += c.carriedAmount; }
                return result;
            };
            const auto total = [&]() { return f.goods(goods) + carried(); };
            const auto received = [&]()
            { return world.settlement(f.foreign)->simulationState().stockpile().amount(goods); };
            const auto step = [&]()
            {
                const auto minute = double(world.time().totalGameMinutes());
                WorldMarketSystem::tickOrders(world, minute);
                map.activities.tick(map, people, minute, 1);
                WorldShipmentSystem::tick(world, minute, 1);
                world.time().advanceMinutes(1);
            };
            for (int i = 0; i < 30; ++i) { step(); }
            PALADIN_CHECK(map.logistics.inventory(source)->amount(goods) == 40);
            PALADIN_CHECK(map.logistics.inventory(counter)->amount(goods) == 0);
            world.realm(f.buyer)->treasury->balance = 0;
            PALADIN_CHECK(WorldMarketSystem::placeOrder(world, f.seller, f.home, f.depot,
                goods, TradeDirection::Export, 2, false));
            for (int i = 0; i < 40; ++i) { step(); }
            PALADIN_CHECK(!map.trade.orders.front().collectionAuthorized);
            PALADIN_CHECK(carried() == 0 && map.logistics.inventory(source)->amount(goods) == 40);
            world.realm(f.buyer)->treasury->balance = 100000;
            const auto cash = f.money();
            const auto stock = total();
            bool sawCarry = false;
            for (int i = 0; i < 1000 && received() < 2; ++i)
            { step(); sawCarry |= carried() == 2; PALADIN_CHECK(total() == stock); }
            PALADIN_CHECK(sawCarry && received() == 2);
            step();
            PALADIN_CHECK(map.trade.orders.empty());
            PALADIN_CHECK(map.logistics.inventory(source)->amount(goods) == 38);
            PALADIN_CHECK(f.money() == cash && total() == stock);
            PALADIN_CHECK(WorldMarketSystem::placeOrder(world, f.seller, f.home, f.depot,
                goods, TradeDirection::Export, 2, true));
            for (int i = 0; i < 1800 && received() < 6; ++i) { step(); }
            PALADIN_CHECK(received() == 6);
            const auto standing = map.trade.orders.front().id;
            PALADIN_CHECK(WorldMarketSystem::cancelOrder(world,f.seller,f.home,f.depot,standing));
            for (int i = 0; i < 150; ++i) { step(); }
            PALADIN_CHECK(received() == 6 && f.money() == cash && total() == stock);
            PALADIN_CHECK(WorldMarketSystem::placeOrder(world, f.seller, f.home, f.depot,
                goods, TradeDirection::Export, 3, false));
            for (int i = 0; i < 1000 && !carried(); ++i) { step(); }
            PALADIN_CHECK(carried() > 0);
            PALADIN_CHECK(WorldMarketSystem::cancelOrder(world,f.seller,f.home,f.depot,map.trade.orders.front().id));
            PALADIN_CHECK(carried() == 0 && total() == stock && f.money() == cash);
            for (int i = 0; i < 30; ++i) { step(); }
            PALADIN_CHECK(received() == 6 && map.trade.orders.empty());
            // Imported goods land at the independent counter, not export stock.
            PALADIN_CHECK(world.settlement(f.foreign)->simulationState().stockpile().setAmount(goods, 2000));
            world.realm(f.seller)->treasury->balance = 100000;
            const auto importCash = f.money();
            const auto importGoods = total();
            PALADIN_CHECK(WorldMarketSystem::placeOrder(world,f.seller,f.home,f.depot,
                goods,TradeDirection::Import,2,false));
            for (int i = 0; i < 300 && !map.trade.orders.empty(); ++i)
            {
                const double minute = double(world.time().totalGameMinutes());
                WorldMarketSystem::tickOrders(world,minute);
                WorldShipmentSystem::tick(world,minute,1);
                world.time().advanceMinutes(1);
            }
            const auto* imports = map.logistics.inventory(map.logistics.importsForObject(f.depot));
            PALADIN_CHECK(imports && imports->amount(goods) == 2);
            PALADIN_CHECK(map.trade.orders.empty());
            PALADIN_CHECK(f.money() == importCash && total() == importGoods);
            PALADIN_CHECK(map.logistics.importsMaySupply(*imports,InventoryKind::Stockpile));
            PALADIN_CHECK(!map.logistics.importsMaySupply(*imports,InventoryKind::Market));
            std::cout << "[depot-matrix] " << goods
                      << ": no blind pickup, funded physical collection, one-shot, repeat, carried cancellation, separate imports and conservation passed\n";
        }
    }
}

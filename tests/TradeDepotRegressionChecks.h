#pragma once
#include "CityCorrectionsChecks.h"

namespace Paladin::Test::DepotRegression
{
    inline void lumberAcrossCity()
    {
        CityCorrections::TradeFixture f(64);
        auto& map = *f.map;
        auto& world = f.sim.world();
        const SettlementTilePosition far{map.grid().width() - 10, 15};
        PALADIN_CHECK(far.x - 20 > map.activities.policy.stockpile.collectionRadius);
        const auto storage = CityCorrections::complete(map,
            SettlementObjectTypes::Stockpile, {far, 5, 5});
        const auto source = map.logistics.forObject(storage);
        const auto depot = map.logistics.forObject(f.depot);
        PALADIN_CHECK(map.logistics.add(source, "lumber", 40, 0));
        PALADIN_CHECK(world.settlement(f.foreign)->simulationState().economy().configure({{"lumber",0,1,0}}));
        auto& people = world.settlement(f.home)->simulationState().citizens();
        people.placeUnpositionedCitizens(map);
        map.employment().synchronize(map.objectState(), people);
        PALADIN_CHECK(map.employment().adjust(map.employment().forObject(f.depot),1,people));
        map.activities.policy.shiftStartMinute=0;
        map.activities.policy.shiftEndMinute=1440;
        map.activities.policy.hungerPerDay=0;
        map.activities.policy.awakeEnergyPerMinute=0;
        map.activities.policy.workEnergyPerMinute=0;
        for (const auto& record : people.citizens())
        {
            auto& person = const_cast<SettlementCitizen&>(record);
            person.hunger=0; person.energy=person.health=100;
        }
        const auto step = [&]()
        {
            const auto minute=double(world.time().totalGameMinutes());
            WorldMarketSystem::tickOrders(world,minute);
            map.activities.tick(map,people,minute,1);
            WorldShipmentSystem::tick(world,minute,1);
            world.time().advanceMinutes(1);
        };
        for (int i=0;i<20;++i) step();
        PALADIN_CHECK(map.logistics.inventory(depot)->amount("lumber")==0);
        PALADIN_CHECK(WorldMarketSystem::placeOrder(world,f.seller,f.home,f.depot,
            "lumber",TradeDirection::Export,2,false));
        bool carried=false;
        for (int i=0;i<2200;++i)
        {
            step();
            for (const auto& citizen:people.citizens())
                carried |= citizen.carriedResource=="lumber" && citizen.carriedAmount==2;
            if (world.settlement(f.foreign)->simulationState().stockpile().amount("lumber")==2) break;
        }
        if (!carried)
        {
            for (const auto& o : map.trade.orders)
                std::cerr << "order " << o.status << " auth=" << o.collectionAuthorized
                          << " issue=" << int(o.collectionIssue) << '\n';
            for (const auto& c : people.citizens()) if (c.workplaceId)
                std::cerr << "worker task=" << int(c.task.kind) << " path=" << c.path.size()
                          << " tile=" << c.tilePosition.x << ',' << c.tilePosition.y
                          << " target=" << c.destination.x << ',' << c.destination.y
                          << " cargo=" << c.carriedAmount << " next=" << c.nextWorkCheckMinutes
                          << " shift=" << map.activities.policy.isWorkTime(double(world.time().totalGameMinutes()))
                          << " break=" << c.breakUntil << " hunger=" << c.hunger << '\n';
            std::cerr << "stock=" << map.logistics.inventory(source)->amount("lumber")
                      << " target=" << map.logistics.inventory(depot)->amount("lumber") << '\n';
            SettlementNavigation nav; nav.synchronize(map);
            for (const auto& c : people.citizens()) if (c.workplaceId)
            {
                for (const auto goal : {far, SettlementTilePosition{far.x - 1, far.y}, SettlementTilePosition{12,3}, SettlementTilePosition{19,7}})
                {
                    const auto path = nav.findPath(map, c.tilePosition, goal, people.movementPolicy);
                    std::cerr << "route goal=" << goal.x << ',' << goal.y << " walkable=" << nav.walkable(map,goal)
                              << " length=" << path.size() << " expanded=" << nav.expandedNodes << '\n';
                }
                std::cerr << "grid=" << map.grid().width() << "x" << map.grid().height() << '\n';
                const auto& f = map.logistics.inventory(depot)->footprint;
                for (const auto goal : {f.topLeft, SettlementTilePosition{f.topLeft.x+f.width-1,f.topLeft.y+f.height-1}})
                {
                    const auto path = nav.findPath(map, far, goal, people.movementPolicy);
                    std::cerr << "delivery " << goal.x << ',' << goal.y << " length=" << path.size()
                              << " expanded=" << nav.expandedNodes << '\n';
                }
                std::cerr << "room=" << map.logistics.receivable(depot,"lumber")
                          << " reservation=" << bool(map.logistics.reservation(c.id)) << '\n';
            }
        }
        PALADIN_CHECK(carried);
        PALADIN_CHECK(world.settlement(f.foreign)->simulationState().stockpile().amount("lumber")==2);
        PALADIN_CHECK(map.logistics.inventory(source)->amount("lumber")==38);
        PALADIN_CHECK(map.logistics.inventory(depot)->amount("stone")==0);
        std::cout << "[depot-regression] funded lumber batch fetched across city beyond ordinary hauling radius\n";
    }
    inline void run() { lumberAcrossCity(); }
}

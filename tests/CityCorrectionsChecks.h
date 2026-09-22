#pragma once
#include "TestFramework.h"
#include "simulation/Simulation.h"
#include "simulation/SimulationReports.h"
#include "simulation/WorldMarketSystem.h"
#include "world/generation/SettlementMapGenerator.h"
#include <iostream>
#include <queue>

namespace Paladin::Test::CityCorrections
{
    inline SettlementMap flatMap()
    {
        SettlementGrid grid(48, 48);
        for (int y = 0; y < 48; ++y) for (int x = 0; x < 48; ++x)
        {
            auto& tile = *grid.tile({x,y});
            tile.terrain = TerrainType::Land;
            tile.biome = BiomeType::Plain;
            tile.temperature = Temperature{.6};
        }
        return SettlementMap(std::move(grid), {0,0}, 1, 1, 48, 220926);
    }
    inline SettlementObjectId complete(SettlementMap& map, std::string_view type,
                                       SettlementObjectFootprint footprint)
    {
        auto definition = *SettlementObjectCatalog::definition(type);
        definition.bypassesConstruction = true;
        PALADIN_CHECK(map.objectState().placeCompletedObject(map.grid(), definition, footprint));
        const auto id = map.objectState().completedObjects().back().id;
        map.naturalFeatures().clear(footprint);
        map.logistics.synchronize(map.objectState(), 0);
        return id;
    }
    inline void refunds()
    {
        for (bool warm : {false, true})
        {
            auto map = flatMap();
            complete(map, SettlementObjectTypes::CityKeep, {{2,2},5,7});
            SettlementCitizenState people;
            PALADIN_CHECK(people.initialize(1,42));
            people.placeUnpositionedCitizens(map);
            auto& worker = const_cast<SettlementCitizen&>(people.citizens().front());
            const SettlementObjectFootprint f{{15,4},5,5};
            PALADIN_CHECK(map.objectState().createConstructionSites(map.grid(),
                *SettlementObjectCatalog::definition(SettlementObjectTypes::House), f));
            const auto site = map.objectState().constructionSites().back().id;
            if (warm) { map.logistics.synchronize(map.objectState(), 0); }
            PALADIN_CHECK(map.objectState().deliverMaterials(site,"lumber",5));
            PALADIN_CHECK(map.objectState().deliverMaterials(site,"stone",3));
            if (warm)
            {
                PALADIN_CHECK(map.logistics.add(map.logistics.forSite(site),"lumber",5,0));
                PALADIN_CHECK(map.logistics.add(map.logistics.forSite(site),"stone",3,0));
            }
            else { map.logistics.synchronize(map.objectState(), 0); }
            const auto source = map.logistics.drop({14,4},"lumber",2,0);
            PALADIN_CHECK(map.logistics.reserve(worker.id,source,map.logistics.forSite(site),"lumber",2));
            PALADIN_CHECK(map.logistics.pickUp(worker.id));
            worker.task.kind = CitizenTaskKind::Haul;
            worker.task.source = source;
            worker.task.destination = map.logistics.forSite(site);
            worker.task.delivering = true;
            worker.carriedResource = "lumber";
            worker.carriedAmount = 2;
            const auto wood = map.logistics.total("lumber") + worker.carriedAmount;
            const auto stone = map.logistics.total("stone");
            PALADIN_CHECK(map.commandState().cancelIntersecting(map,f,people,10) > 0);
            PALADIN_CHECK(!map.objectState().constructionSite(site));
            PALADIN_CHECK(worker.carriedAmount == 0 && !map.logistics.reservation(worker.id));
            PALADIN_CHECK(map.logistics.total("lumber") == wood);
            PALADIN_CHECK(map.logistics.total("stone") == stone);
            int refundedWood = 0, refundedStone = 0;
            for (const auto& pile : map.logistics.inventories()) if (pile.kind == InventoryKind::Groundpile)
            { refundedWood += pile.amount("lumber"); refundedStone += pile.amount("stone"); }
            PALADIN_CHECK(refundedWood == 7 && refundedStone == 3);
            PALADIN_CHECK(map.commandState().cancelIntersecting(map,f,people,10) == 0);
            PALADIN_CHECK(map.logistics.total("lumber") == wood);
        }
        std::cout << "[city-corrections] delivered and carried construction refunds conserved, including pause\n";
    }
    inline void hunger()
    {
        auto map = flatMap();
        complete(map, SettlementObjectTypes::CityKeep, {{2,2},5,7});
        SettlementCitizenState people;
        PALADIN_CHECK(people.initialize(4,66));
        people.placeUnpositionedCitizens(map);
        map.activities.policy.hungerPerDay = 0;
        map.activities.policy.decisionsPerMinute = 0;
        for (std::size_t i = 0; i < people.citizens().size(); ++i)
        {
            auto& c = const_cast<SettlementCitizen&>(people.citizens()[i]);
            c.health = c.energy = 100; c.happiness = 70;
            c.hunger = i == 0 ? 0 : i == 1 ? 49.99 : i == 2 ? 50 : 80;
            c.homeId = {}; c.workplaceId = {}; c.child = false;
            c.homelessMinutes = 0;
        }
        map.activities.tick(map,people,600,1);
        const auto a = people.citizens()[0].happiness;
        PALADIN_CHECK(std::abs(a - people.citizens()[1].happiness) < 1e-7);
        PALADIN_CHECK(std::abs(a - people.citizens()[2].happiness) < 1e-7);
        PALADIN_CHECK(people.citizens()[3].happiness < a);
        std::cout << "[city-corrections] no hunger distress below 50; distress above threshold\n";
    }
    inline void relief()
    {
        constexpr SettlementTilePosition directions[]{{1,0},{-1,0},{0,1},{0,-1}};
        for (bool hill : {false,true}) for (std::uint64_t seed : {17,42,119})
        {
            WorldGrid source(5,5);
            for (int y=0;y<5;++y) for (int x=0;x<5;++x)
            {
                auto& t=*source.tile({x,y});
                t.terrain = hill ? TerrainType::Land : TerrainType::Mountain;
                t.relief = hill ? ReliefType::Hills : ReliefType::Mountain;
                t.biome = hill ? BiomeType::Hills : BiomeType::Plain;
                t.temperature = Temperature{.5};
            }
            SettlementMapGenerationSettings settings;
            settings.localTilesPerWorldTile=128;
            auto map=SettlementMapGenerator{}.generate(source,{2,2},3,3,seed,settings);
            PALADIN_CHECK(map);
            const int w=map->grid().width(), h=map->grid().height();
            std::vector<bool> seen(std::size_t(w)*h);
            int total=0, components=0, tiny=0, largest=0;
            for(int y=0;y<h;++y) for(int x=0;x<w;++x)
            {
                if(seen[y*w+x] || map->grid().tile({x,y})->terrain!=TerrainType::Mountain) continue;
                std::vector<SettlementTilePosition> queue{{x,y}}; seen[y*w+x]=true;
                for(std::size_t i=0;i<queue.size();++i) for(auto d:directions)
                {
                    const SettlementTilePosition p{queue[i].x+d.x,queue[i].y+d.y};
                    const auto* t=map->grid().tile(p);
                    if(t && !seen[p.y*w+p.x] && t->terrain==TerrainType::Mountain)
                    { seen[p.y*w+p.x]=true; queue.push_back(p); }
                }
                total+=int(queue.size()); ++components; tiny += queue.size()<32;
                largest = std::max(largest,int(queue.size()));
            }
            std::cout << "[relief-shape] hills=" << hill << " seed=" << seed
                      << " rock=" << double(total)/(w*h) << " components=" << components
                      << " largest=" << largest << '\n';
            // Mountain country is a few broad barriers; hill country has
            // smaller separated forms. Neither requires an almost-solid map
            // cut by compulsory cross-map channels.
            PALADIN_CHECK(total > w*h*(hill ? .08 : .30));
            PALADIN_CHECK(total < w*h*(hill ? .65 : .90));
            PALADIN_CHECK(components <= (hill ? 45 : 24) && tiny <= 1);
            if (!hill) { PALADIN_CHECK(largest > total*.20); }
            // Every naturally open cave is attached to ordinary traversable land.
            std::fill(seen.begin(),seen.end(),false);
            std::vector<SettlementTilePosition> reachable;
            int caves=0;
            for(int y=0;y<h;++y) for(int x=0;x<w;++x)
            {
                const auto& t=*map->grid().tile({x,y}); caves+=t.rockFloor;
                if(t.terrain==TerrainType::Land && !t.rockFloor)
                { seen[y*w+x]=true; reachable.push_back({x,y}); }
            }
            for(std::size_t i=0;i<reachable.size();++i) for(auto d:directions)
            {
                const SettlementTilePosition p{reachable[i].x+d.x,reachable[i].y+d.y};
                const auto* t=map->grid().tile(p);
                if(t && !seen[p.y*w+p.x] && t->terrain==TerrainType::Land)
                { seen[p.y*w+p.x]=true; reachable.push_back(p); }
            }
            for(int y=0;y<h;++y) for(int x=0;x<w;++x)
            { PALADIN_CHECK(!map->grid().tile({x,y})->rockFloor || seen[y*w+x]); }
            std::cout << "[relief-caves] seed=" << seed << " hills=" << hill << " cells=" << caves << '\n';
            if(!hill) { PALADIN_CHECK(caves > 0 && caves < w*h*.015); }
        }
        std::cout << "[city-corrections] six 384x384 range maps: coherent ranges, sparse accessible caves\n";
    }
    struct TradeFixture
    {
        static WorldGenerationSettings settings()
        { WorldGenerationSettings s; s.width=128; s.height=64; s.seed=9922; s.populateAiRealms=false; return s; }
        Simulation sim{settings()};
        SettlementId home, foreign;
        RealmId seller,buyer;
        SettlementObjectId depot,storage;
        SettlementMap* map=nullptr;
        explicit TradeFixture(int localScale = 16)
        {
            auto& world=sim.world();
            for(int y=0;y<64;++y) for(int x=0;x<128;++x)
            {
                auto& t=*world.grid().tile({x,y});
                t.terrain=TerrainType::Land; t.relief=ReliefType::Lowland;
                t.biome=BiomeType::Plain; t.temperature=Temperature{.6};
            }
            world.grid().terrainChanged();
            home=sim.foundPlayerCapital({32,32},{"Test Realm","Test Folk","Test City",{},"civic"});
            PALADIN_CHECK(home);
            seller=sim.playerRealmId(); buyer=world.createRealm();
            auto profile=defaultSettlementFoundationProfile();
            profile.initialPopulation=200; profile.initialDetailedCitizenCount=0; profile.initialResources={};
            foreign=world.foundCapitalSettlement({44,32},buyer,{"Buyer","Buyer Folk","Buyer City",{},"civic"},profile);
            PALADIN_CHECK(foreign);
            PALADIN_CHECK(world.settlement(foreign)->simulationState().economy().configure({{"iron",0,1,0}}));
            SettlementMapGenerationSettings settings; settings.localTilesPerWorldTile=localScale;
            PALADIN_CHECK(sim.prepareSettlementMap(home,settings)); map=sim.settlementMap(home);
            for(int y=0;y<map->grid().height();++y) for(int x=0;x<map->grid().width();++x)
            {
                auto& t=*map->grid().tile({x,y}); t.terrain=TerrainType::Land;
                t.biome=BiomeType::Plain; t.rockFloor=false;
            }
            map->objectState().invalidateTerrainCache();
            map->naturalFeatures().clear({{0,0},map->grid().width(),map->grid().height()});
            if(!map->objectState().hasCityKeep()) complete(*map,SettlementObjectTypes::CityKeep,{{2,2},5,7});
            depot=complete(*map,SettlementObjectTypes::TradeDepot,{{12,3},8,5});
            storage=complete(*map,SettlementObjectTypes::Stockpile,{{3,13},5,5});
            PALADIN_CHECK(map->logistics.add(map->logistics.forObject(storage),"iron",30,0));
            PALADIN_CHECK(map->logistics.add(map->logistics.forObject(storage),"stone",30,0));
            world.diplomacy().relations.push_back({seller,buyer,false,true,false});
            world.realm(seller)->treasury->balance=0; world.realm(buyer)->treasury->balance=100000;
        }
        double goods(std::string_view resource) const
        {
            double total=0;
            for(const auto& city:sim.world().settlements()) total+=WorldShipmentSystem::total(city,resource);
            for(const auto& s:sim.world().shipments()) if(s.resource==resource) total+=s.cargo;
            return total;
        }
        Money money() const
        {
            Money total=0;
            for(auto r:{seller,buyer}) total+=sim.world().realm(r)->treasury->balance;
            for (const auto& city : sim.world().settlements())
            {
                if (const auto* local = city.simulationState().localMap())
                { total += local->commerce.businessTotal() + local->commerce.householdTotal(); }
            }
            for(const auto& s:sim.world().shipments()) total+=s.escrow;
            return total;
        }
    };
    inline void trade()
    {
        TradeFixture f; auto& world=f.sim.world(); auto& map=*f.map;
        PALADIN_CHECK(map.trade.exportTarget(f.depot,"iron")==0);
        const auto cash=f.money(); const auto goods=f.goods("iron");
        PALADIN_CHECK(WorldMarketSystem::placeOrder(world,f.seller,f.home,f.depot,"iron",TradeDirection::Export,2,false));
        PALADIN_CHECK(map.trade.orders.size()==1 && map.trade.exportTarget(f.depot,"iron")==0);
        WorldMarketSystem::tickOrders(world,0);
        PALADIN_CHECK(map.trade.exportTarget(f.depot,"iron")==2);
        PALADIN_CHECK(map.trade.exportTarget(f.depot,"stone")==0);
        const auto orderId=map.trade.orders.front().id;
        const auto shipmentId=map.trade.orders.front().shipment;
        PALADIN_CHECK(world.shipment(shipmentId)->awaitingCollection);
        PALADIN_CHECK(world.shipment(shipmentId)->cargo==0 && world.shipment(shipmentId)->escrow>0);
        PALADIN_CHECK(f.money()==cash && f.goods("iron")==goods);
        // The reserved buyer can now be cash-poor without invalidating its paid batch.
        const auto deposit=map.logistics.forObject(f.depot), stock=map.logistics.forObject(f.storage);
        PALADIN_CHECK(map.logistics.moveAvailable(stock,deposit,"iron",2));
        WorldShipmentSystem::tick(world,1,1);
        PALADIN_CHECK(world.shipment(shipmentId)->cargo==2);
        PALADIN_CHECK(!world.shipment(shipmentId)->awaitingCollection);
        PALADIN_CHECK(map.trade.exportTarget(f.depot,"iron")==0);
        WorldShipmentSystem::tick(world,2,200);
        PALADIN_CHECK(world.shipment(shipmentId)->deliveries==1);
        WorldMarketSystem::tickOrders(world,202);
        PALADIN_CHECK(map.trade.orders.empty());
        PALADIN_CHECK(f.money()==cash && f.goods("iron")==goods);
        PALADIN_CHECK(!WorldMarketSystem::cancelOrder(world,f.seller,f.home,f.depot,orderId));
        // Cancellation before collection refunds the exact escrow, and cannot
        // leave a worker permission to take other stock.
        PALADIN_CHECK(WorldMarketSystem::placeOrder(world,f.seller,f.home,f.depot,"iron",TradeDirection::Export,2,true));
        WorldMarketSystem::tickOrders(world,210);
        const auto cancelId=map.trade.orders.back().id;
        PALADIN_CHECK(map.trade.exportTarget(f.depot,"iron")==2);
        PALADIN_CHECK(WorldMarketSystem::cancelOrder(world,f.seller,f.home,f.depot,cancelId));
        PALADIN_CHECK(map.trade.orders.empty() && map.trade.exportTarget(f.depot,"iron")==0);
        PALADIN_CHECK(f.money()==cash && f.goods("iron")==goods);
        // No buyer cash and no agreement each leave an order waiting, not a
        // permission to fetch stock. Orders remain visible and cancellable.
        world.realm(f.buyer)->treasury->balance=0;
        PALADIN_CHECK(WorldMarketSystem::placeOrder(world,f.seller,f.home,f.depot,"stone",TradeDirection::Export,2,false));
        WorldMarketSystem::tickOrders(world,220);
        PALADIN_CHECK(map.trade.exportTarget(f.depot,"stone")==0);
        PALADIN_CHECK(map.trade.visible(map.trade.orders.back()));
        // Import stock is separate and wholesales only to stockpiles while one exists.
        const auto imports=map.logistics.importsForObject(f.depot);
        PALADIN_CHECK(imports && imports!=deposit);
        PALADIN_CHECK(map.logistics.add(imports,"fish",3,220));
        PALADIN_CHECK(map.logistics.importsMaySupply(*map.logistics.inventory(imports),InventoryKind::Stockpile));
        PALADIN_CHECK(!map.logistics.importsMaySupply(*map.logistics.inventory(imports),InventoryKind::Market));
        PALADIN_CHECK(map.commerce.mealPrice(map,*map.logistics.inventory(imports))<0);
        PALADIN_CHECK(map.objectState().demolish(f.storage,{3,13}));
        map.logistics.synchronize(map.objectState(),220);
        PALADIN_CHECK(map.logistics.importsMaySupply(*map.logistics.inventory(imports),InventoryKind::Market));
        PALADIN_CHECK(map.logistics.importsMaySupply(*map.logistics.inventory(imports),InventoryKind::Workplace));
        PALADIN_CHECK(map.commerce.mealPrice(map,*map.logistics.inventory(imports))>=0);
        std::cout << "[city-corrections] funded exact-batch orders, one-shot removal, cancellation and separate imports\n";
    }
    inline void deaths()
    {
        TradeFixture f; auto& world=f.sim.world();
        auto& people=world.settlement(f.home)->simulationState().citizens();
        SimulationReports reports; reports.update(world,f.seller,true);
        auto& person=const_cast<SettlementCitizen&>(people.citizens().front());
        const auto name=person.name; person.health=0;
        f.map->activities.tick(*f.map,people,1,1);
        f.map->activities.tick(*f.map,people,2,1);
        reports.update(world,f.seller); // below the ordinary ten-minute sample cadence
        int count=0;
        for(const auto& event:reports.events()) if(event.key=="citizen-died" && event.city==f.home)
        { ++count; PALADIN_CHECK(event.text.find(name)!=std::string::npos); }
        PALADIN_CHECK(count==1);
        reports.update(world,f.seller,true);
        count=0; for(const auto& event:reports.events()) count+=event.key=="citizen-died";
        PALADIN_CHECK(count==1);
        std::cout << "[city-corrections] named death events are immediate and emitted exactly once\n";
    }
    inline void depotWorker()
    {
        TradeFixture f;
        auto& map = *f.map;
        auto& world = f.sim.world();
        auto& people = world.settlement(f.home)->simulationState().citizens();
        people.placeUnpositionedCitizens(map);
        map.employment().synchronize(map.objectState(), people);
        PALADIN_CHECK(map.employment().adjust(map.employment().forObject(f.depot), 1, people));
        map.activities.policy.hungerPerDay = 0;
        map.activities.policy.awakeEnergyPerMinute = 0;
        map.activities.policy.workEnergyPerMinute = 0;
        map.activities.policy.shiftStartMinute = 0;
        map.activities.policy.shiftEndMinute = 1440;
        for (const auto& record : people.citizens())
        {
            auto& person = const_cast<SettlementCitizen&>(record);
            person.hunger = 0; person.energy = person.health = 100;
        }
        const auto stock = map.logistics.forObject(f.storage);
        const auto depot = map.logistics.forObject(f.depot);
        world.time().advanceMinutes(600 - world.time().totalGameMinutes());
        const auto minuteStep = [&]()
        {
            const auto minute = double(world.time().totalGameMinutes());
            WorldMarketSystem::tickOrders(world, minute);
            map.activities.tick(map, people, minute, 1);
            WorldShipmentSystem::tick(world, minute, 1);
            world.time().advanceMinutes(1);
            for (const auto& person : people.citizens())
            {
                if (const auto* claim = map.logistics.reservation(person.id);
                    claim && claim->destination == depot)
                { PALADIN_CHECK(claim->resource == "iron" && claim->amount <= 2); }
            }
            PALADIN_CHECK(map.logistics.inventory(depot)->amount("stone") == 0);
        };
        for (int i = 0; i < 60; ++i) { minuteStep(); }
        PALADIN_CHECK(map.logistics.inventory(depot)->used() == 0);
        PALADIN_CHECK(map.logistics.inventory(stock)->amount("iron") == 30);
        PALADIN_CHECK(WorldMarketSystem::placeOrder(world, f.seller, f.home, f.depot,
            "iron", TradeDirection::Export, 2, false));
        bool carried = false;
        for (int i = 0; i < 200; ++i)
        {
            minuteStep();
            for (const auto& person : people.citizens())
            { carried |= person.carriedResource == "iron" && person.carriedAmount == 2; }
        }
        PALADIN_CHECK(carried);
        PALADIN_CHECK(world.settlement(f.foreign)->simulationState().stockpile().amount("iron") == 2);
        PALADIN_CHECK(map.logistics.inventory(stock)->amount("iron") == 28);
        PALADIN_CHECK(map.logistics.inventory(depot)->amount("iron") == 0 && map.trade.orders.empty());
        PALADIN_CHECK(WorldMarketSystem::placeOrder(world, f.seller, f.home, f.depot,
            "iron", TradeDirection::Export, 2, true));
        const auto orderId = map.trade.orders.back().id;
        for (int i = 0; i < 260 &&
            world.settlement(f.foreign)->simulationState().stockpile().amount("iron") < 6; ++i)
        { minuteStep(); }
        PALADIN_CHECK(world.settlement(f.foreign)->simulationState().stockpile().amount("iron") == 6);
        PALADIN_CHECK(!map.trade.orders.empty() && map.trade.orders.back().standing);
        PALADIN_CHECK(WorldMarketSystem::cancelOrder(world, f.seller, f.home, f.depot, orderId));
        for (int i = 0; i < 80; ++i) { minuteStep(); }
        PALADIN_CHECK(world.settlement(f.foreign)->simulationState().stockpile().amount("iron") == 6);
        PALADIN_CHECK(map.logistics.inventory(stock)->amount("iron") == 24);
        PALADIN_CHECK(map.trade.exportTarget(f.depot, "iron") == 0);
        std::cout << "[city-corrections] actual depot employee: no blind collection, exact 2-unit one-shot and standing batches\n";
    }
    inline void run()
    {
        for (int width : {3, 7, 9, 30}) for (int height : {3, 7, 12})
        for (std::size_t slot = 0; slot < 80; ++slot)
        {
            const auto p = miningWorkTile({10,20},width,height,slot);
            PALADIN_CHECK(p.x >= 10 && p.x < 10 + width);
            PALADIN_CHECK(p.y >= 20 + miningServiceRows(height) && p.y < 20 + height);
        }
        refunds(); hunger(); relief(); trade(); deaths(); depotWorker();
    }
}

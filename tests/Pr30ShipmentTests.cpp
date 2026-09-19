#include "TestFramework.h"
#include "simulation/WorldShipmentSystem.h"
#include "simulation/WorldLandNavigation.h"
#include "simulation/Simulation.h"
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
        { WorldGenerationSettings s; s.width=128; s.height=64; s.seed=300031; s.populateAiRealms=false; return s; }
        Simulation sim{settings()};
        World& world=sim.world();
        RealmId owner=sim.playerRealmId(), foreign;
        SettlementId source, destination;
        Fixture()
        {
            for (int y=0;y<world.grid().height();++y) for (int x=0;x<world.grid().width();++x)
            { auto& t=*world.grid().tile({x,y}); t.terrain=TerrainType::Land; t.biome=BiomeType::Plain; }
            world.grid().terrainChanged();
            source=sim.foundPlayerCapital({24,32},{"Logistics","Cargo Folk","Origin",{},"civic"});
            destination=world.foundSettlement({48,32},owner,playerSettlementFoundationProfile(300032));
            foreign=world.createRealm();
            PALADIN_CHECK(source && destination);
            PALADIN_CHECK(world.settlement(source)->simulationState().stockpile().setAmount("lumber",1000));
        }
        double goods() const
        {
            double total=0;
            for (const auto& city:world.settlements()) total+=WorldShipmentSystem::total(city,"lumber");
            for (const auto& route:world.shipments()) if (route.resource=="lumber") total+=route.cargo;
            return total;
        }
        ShipmentId send(bool repeat=false,int amount=20)
        {
            ShipmentId id;
            PALADIN_CHECK(WorldShipmentSystem::create(world,owner,source,destination,"lumber",amount,repeat,&id)==ShipmentResult::Success);
            PALADIN_CHECK(id); return id;
        }
        SettlementMap& physical(SettlementId id)
        {
            SettlementMapGenerationSettings options; options.localTilesPerWorldTile=4;
            PALADIN_CHECK(sim.prepareSettlementMap(id,options));
            auto& map=*sim.settlementMap(id);
            for (int y=0;y<map.grid().height();++y) for (int x=0;x<map.grid().width();++x)
                map.grid().tile({x,y})->terrain=TerrainType::Land;
            auto definition=*SettlementObjectCatalog::definition("city_keep"); definition.bypassesConstruction=true;
            PALADIN_CHECK(map.objectState().placeCompletedObject(map.grid(),definition,{{2,2},5,7}));
            map.naturalFeatures().clear({{2,2},5,7}); map.logistics.synchronize(map.objectState(),360);
            return map;
        }
    };
    void oneOffAndRepeat()
    {
        Fixture f;
        const auto total=f.goods(); const auto id=f.send();
        PALADIN_CHECK(f.goods()==total && f.world.shipment(id)->cargo==20);
        PALADIN_CHECK(f.world.settlement(f.destination)->simulationState().stockpile().amount("lumber")==0);
        WorldShipmentSystem::tick(f.world,360,0);
        WorldShipmentSystem::tick(f.world,360,std::numeric_limits<double>::quiet_NaN());
        PALADIN_CHECK(f.world.shipment(id)->tileIndex==0);
        WorldShipmentSystem::tick(f.world,360,5);
        PALADIN_CHECK(f.world.shipment(id)->tileIndex==0 && f.world.shipment(id)->visualX()>24 && f.world.shipment(id)->visualX()<25);
        WorldShipmentSystem::tick(f.world,365,235);
        PALADIN_CHECK(f.world.shipment(id)->deliveries==1 && f.world.shipment(id)->cargo==0);
        PALADIN_CHECK(f.world.shipment(id)->phase==ShipmentPhase::Returning && f.goods()==total);
        WorldShipmentSystem::tick(f.world,600,240);
        PALADIN_CHECK(!f.world.shipment(id)->active() && f.world.shipment(id)->position()==f.world.settlement(f.source)->position());
        const auto repeat=f.send(true,30);
        WorldShipmentSystem::tick(f.world,840,480*3+240);
        PALADIN_CHECK(f.world.shipment(repeat)->deliveries==4 && f.goods()==total);
        PALADIN_CHECK(WorldShipmentSystem::stop(f.world,f.owner,repeat)==ShipmentResult::Success);
        WorldShipmentSystem::tick(f.world,2520,10000);
        PALADIN_CHECK(!f.world.shipment(repeat)->active() && f.world.shipment(repeat)->deliveries==4 && f.goods()==total);
        std::cout<<"[shipments] physical cargo, interpolation, pause, one-off return and sustained trips conserve goods\n";
    }
    void authorizationAndFailures()
    {
        Fixture f; const auto total=f.goods(); ShipmentId id;
        using R=ShipmentResult;
        PALADIN_CHECK(WorldShipmentSystem::create(f.world,f.foreign,f.source,f.destination,"lumber",1,false)==R::NotOwned);
        PALADIN_CHECK(WorldShipmentSystem::create(f.world,f.owner,f.source,f.source,"lumber",1,false)==R::SameSettlement);
        PALADIN_CHECK(WorldShipmentSystem::create(f.world,f.owner,f.source,f.destination,"unknown",1,false)==R::InvalidResource);
        for (int amount:{0,-1,std::numeric_limits<int>::max()})
            PALADIN_CHECK(WorldShipmentSystem::create(f.world,f.owner,f.source,f.destination,"lumber",amount,false)==R::InvalidAmount);
        PALADIN_CHECK(WorldShipmentSystem::create(f.world,f.owner,f.source,f.destination,"lumber",1001,false)==R::InsufficientGoods);
        PALADIN_CHECK(f.goods()==total && f.world.shipments().empty());
        id=f.send(true);
        PALADIN_CHECK(WorldShipmentSystem::stop(f.world,f.foreign,id)==R::NotOwned);
        PALADIN_CHECK(f.world.assignSettlementToRealm(f.destination,f.foreign));
        WorldShipmentSystem::tick(f.world,360,1000);
        PALADIN_CHECK(!f.world.shipment(id)->active() && f.world.shipment(id)->deliveries==0 && f.goods()==total);
        PALADIN_CHECK(f.world.settlement(f.source)->simulationState().stockpile().amount("lumber")==1000);
        PALADIN_CHECK(f.world.assignSettlementToRealm(f.destination,f.owner));
        id=f.send();
        f.world.grid().tile({25,32})->terrain=TerrainType::Water; f.world.grid().terrainChanged();
        WorldShipmentSystem::tick(f.world,1360,100);
        PALADIN_CHECK(f.world.shipment(id)->phase==ShipmentPhase::Blocked && f.world.shipment(id)->cargo==20 && f.goods()==total);
        PALADIN_CHECK(WorldShipmentSystem::stop(f.world,f.owner,id)==R::Success);
        PALADIN_CHECK(!f.world.shipment(id)->active() && f.goods()==total);
        for (const auto p:{WorldTilePosition{47,32},{49,32},{48,31},{48,33}}) f.world.grid().tile(p)->terrain=TerrainType::Water;
        PALADIN_CHECK(WorldShipmentSystem::create(f.world,f.owner,f.source,f.destination,"lumber",1,false)==R::NoLandRoute);
        PALADIN_CHECK(f.goods()==total);
        // Seam-neighbouring tiles are one step apart, not a tour around the planet.
        const auto path=worldLandRoute(f.world.grid(),{0,16},{127,16});
        PALADIN_CHECK(path && path->size()==2);
        std::cout<<"[shipments] ownership, quantities, blocked routes, captured targets and longitude seam passed\n";
    }
    void localInventoriesAndOverflow()
    {
        Fixture f; auto& from=f.physical(f.source); auto& to=f.physical(f.destination);
        const auto keep=from.logistics.forObject(from.objectState().completedObjects().front().id);
        const auto reserve=from.logistics.drop({10,10},"lumber",5,360);
        PALADIN_CHECK(from.logistics.reserve(CitizenId{999},keep,reserve,"lumber",10));
        PALADIN_CHECK(WorldShipmentSystem::available(*f.world.settlement(f.source),"lumber")==35);
        const auto total=f.goods(); const auto id=f.send(false,35);
        PALADIN_CHECK(from.logistics.inventory(keep)->amount("lumber")==10); // claimed goods stay put
        WorldShipmentSystem::tick(f.world,360,240);
        PALADIN_CHECK(f.goods()==total && f.world.shipment(id)->deliveries==1);
        int piles=0;
        for(const auto& inventory:to.logistics.inventories()) if(inventory.kind==InventoryKind::Groundpile)
        {
            piles+=inventory.amount("lumber");
            PALADIN_CHECK(inventory.footprint.topLeft.y>=9); // outside and near the keep
        }
        PALADIN_CHECK(piles==35); // the founding keep is full
        PALADIN_CHECK(WorldShipmentSystem::create(f.world,f.owner,f.source,f.destination,"lumber",1,true)==ShipmentResult::InsufficientGoods); // reserved 10 cannot be taken
    }
    void cadenceAndWaiting()
    {
        Fixture a,b; const auto ia=a.send(true), ib=b.send(true);
        WorldShipmentSystem::tick(a.world,360,1300);
        for(int i=0;i<1300;++i) WorldShipmentSystem::tick(b.world,360+i,1);
        PALADIN_CHECK(a.goods()==b.goods());
        PALADIN_CHECK(a.world.shipment(ia)->deliveries==b.world.shipment(ib)->deliveries);
        PALADIN_CHECK(a.world.shipment(ia)->tileIndex==b.world.shipment(ib)->tileIndex);
        PALADIN_CHECK(a.world.shipment(ia)->cargo==b.world.shipment(ib)->cargo);
        PALADIN_CHECK(a.world.settlement(a.source)->simulationState().stockpile().setAmount("lumber",0));
        WorldShipmentSystem::tick(a.world,1660,2000);
        PALADIN_CHECK(a.world.shipment(ia)->phase==ShipmentPhase::Waiting);
        PALADIN_CHECK(a.world.settlement(a.source)->simulationState().stockpile().setAmount("lumber",20));
        WorldShipmentSystem::tick(a.world,3660,1);
        PALADIN_CHECK(a.world.shipment(ia)->phase==ShipmentPhase::Outbound && a.world.shipment(ia)->cargo==20);
        std::cout<<"[shipments] partitioned time and supply-wait restart passed\n";
    }
}
void runPr30ShipmentTests()
{
    oneOffAndRepeat(); authorizationAndFailures(); localInventoriesAndOverflow(); cadenceAndWaiting();
}

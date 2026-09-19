#include "simulation/WorldShipmentSystem.h"
#include "simulation/WorldLandNavigation.h"
#include "world/World.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include "simulation/systems/SettlementNavigation.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Paladin
{
    namespace
    {
        bool publicStorage(InventoryKind kind)
        { return kind == InventoryKind::Keep || kind == InventoryKind::Stockpile || kind == InventoryKind::Groundpile; }
        SettlementTilePosition unloadingTile(const SettlementMap& map)
        {
            SettlementTilePosition centre{map.grid().width()/2, map.grid().height()/2};
            for (const auto& object : map.objectState().completedObjects())
                if (object.objectTypeId == SettlementObjectTypes::CityKeep)
                {
                    // Outside the keep's wall ring, not a pile embedded in a wall.
                    centre = {object.footprint.topLeft.x + object.footprint.width/2,
                              object.footprint.topLeft.y + object.footprint.height};
                    break;
                }
            for (int radius=0; radius<std::max(map.grid().width(),map.grid().height()); ++radius)
                for (int y=centre.y-radius; y<=centre.y+radius; ++y)
                    for (int x=centre.x-radius; x<=centre.x+radius; ++x)
                    {
                        if (radius && x!=centre.x-radius && x!=centre.x+radius && y!=centre.y-radius && y!=centre.y+radius) continue;
                        if (SettlementNavigation{}.walkable(map,{x,y})) return {x,y};
                    }
            return {-1,-1};
        }
    }
    double WorldShipmentSystem::total(const Settlement& city, std::string_view resource)
    {
        const auto& state = city.simulationState();
        return state.localMap() ? state.localMap()->logistics.total(resource) : state.stockpile().amount(resource);
    }
    int WorldShipmentSystem::available(const Settlement& city, std::string_view resource)
    {
        if (!SettlementResourceCatalog::definition(resource)) return 0;
        if (const auto* local = city.simulationState().localMap())
        {
            std::int64_t count=0;
            for (const auto& inventory : local->logistics.inventories())
                if (publicStorage(inventory.kind)) count += local->logistics.available(inventory.id,resource);
            return int(std::min<std::int64_t>(count,MaximumShipment));
        }
        return int(std::clamp(std::floor(city.simulationState().stockpile().amount(resource)),0.0,double(MaximumShipment)));
    }
    bool WorldShipmentSystem::load(World& world, WorldShipment& shipment)
    {
        auto* source=world.settlement(shipment.source);
        auto* target=world.settlement(shipment.destination);
        if (!source || !target || source->ownerRealmId()!=shipment.owner || target->ownerRealmId()!=shipment.owner ||
            available(*source,shipment.resource)<shipment.amount) return false;
        if(shipment.aiManaged && shipment.resource=="food" &&
           (available(*source,"food")-shipment.amount<2.*source->population() ||
            total(*target,"food")>12.*target->population()+120)) return false;
        if (auto* local=source->simulationState().localMap_.get())
        {
            int remaining=shipment.amount;
            for (const auto& inventory : local->logistics.inventories())
            {
                if (!publicStorage(inventory.kind)) continue;
                const int amount=std::min(remaining,local->logistics.available(inventory.id,shipment.resource));
                if (amount>0 && local->logistics.consumeAvailable(inventory.id,shipment.resource,amount)) remaining-=amount;
                if (!remaining) break;
            }
        }
        else if (!source->simulationState().stockpile().addAmount(shipment.resource,-shipment.amount)) return false;
        shipment.cargo=shipment.amount; shipment.phase=ShipmentPhase::Outbound; shipment.stepMinutes=0;
        return true;
    }
    bool WorldShipmentSystem::receive(World& world, SettlementId id, std::string_view resource, int amount, double minute)
    {
        auto* city=world.settlement(id);
        if (!city || amount<=0 || amount>MaximumShipment || !SettlementResourceCatalog::definition(resource)) return false;
        auto* local=city->simulationState().localMap_.get();
        if (!local) return city->simulationState().stockpile().addAmount(resource,amount);
        const auto dropAt=unloadingTile(*local);
        if (dropAt.x<0) return false;
        // Resolve every inventory by ID. Adding an overflow pile may grow its
        // vector, so it deliberately happens after the storage iteration.
        int remaining=amount;
        for (const auto kind : {InventoryKind::Stockpile,InventoryKind::Keep})
            for (const auto& inventory : local->logistics.inventories())
            {
                if (inventory.kind!=kind) continue;
                const int fit=std::min(remaining,local->logistics.receivable(inventory.id,resource));
                if (fit>0 && local->logistics.add(inventory.id,resource,fit,minute)) remaining-=fit;
                if (!remaining) return true;
            }
        return bool(local->logistics.drop(dropAt,resource,remaining,minute));
    }
    ShipmentResult WorldShipmentSystem::create(World& world, RealmId actor, SettlementId from,
        SettlementId to, std::string_view resource, int amount, bool repeating, ShipmentId* created, bool aiManaged)
    {
        if (created) *created={};
        const auto* source=world.settlement(from); const auto* target=world.settlement(to);
        if (!source || !target || !source->simulationState().isInitialized() || !target->simulationState().isInitialized())
            return ShipmentResult::InvalidSettlement;
        if (!actor || source->ownerRealmId()!=actor || target->ownerRealmId()!=actor) return ShipmentResult::NotOwned;
        if (from==to || source->position()==target->position()) return ShipmentResult::SameSettlement;
        if (!SettlementResourceCatalog::definition(resource)) return ShipmentResult::InvalidResource;
        if (amount<=0 || amount>MaximumShipment) return ShipmentResult::InvalidAmount;
        const auto active=std::count_if(world.shipments_.begin(),world.shipments_.end(),[&](const auto& s)
            { return s.owner==actor && s.active(); });
        if (active>=MaximumActiveRoutesPerRealm) return ShipmentResult::RouteLimit;
        if (available(*source,resource)<amount) return ShipmentResult::InsufficientGoods;
        auto path=worldLandRoute(world.grid(),source->position(),target->position());
        if (!path || path->size()<2) return ShipmentResult::NoLandRoute;
        WorldShipment shipment;
        shipment.id=world.shipmentIds_.generate(); shipment.owner=actor; shipment.source=from; shipment.destination=to;
        shipment.resource=resource; shipment.amount=amount; shipment.repeating=repeating; shipment.aiManaged=aiManaged;
        shipment.path=std::move(*path); shipment.wrapWidth=world.grid().width();
        if (!load(world,shipment)) return ShipmentResult::InsufficientGoods;
        // Finished route records are expendable history, unlike active cargo.
        std::erase_if(world.shipments_,[&](const auto& s) { return s.owner==actor && !s.active(); });
        const auto id=shipment.id; world.shipments_.push_back(std::move(shipment));
        if (created) *created=id;
        return ShipmentResult::Success;
    }
    ShipmentResult WorldShipmentSystem::stop(World& world, RealmId actor, ShipmentId id)
    {
        for (auto& shipment : world.shipments_)
            if (shipment.id==id)
            {
                if (!actor || shipment.owner!=actor) return ShipmentResult::NotOwned;
                shipment.repeating=false;
                if (shipment.phase==ShipmentPhase::Waiting) shipment.phase=ShipmentPhase::Completed;
                if (shipment.phase==ShipmentPhase::Blocked)
                {
                    const auto* home=world.settlement(shipment.source);
                    if (home && home->ownerRealmId()==actor)
                    {
                        auto back=worldLandRoute(world.grid(),home->position(),shipment.position());
                        if (back && back->size()>1)
                        {
                            shipment.path=std::move(*back); shipment.tileIndex=shipment.path.size()-1;
                            shipment.phase=ShipmentPhase::Returning; shipment.stepMinutes=0;
                        }
                        else if (back && (shipment.cargo==0 || receive(world,home->id(),shipment.resource,shipment.cargo,double(world.time().totalGameMinutes()))))
                        { shipment.cargo=0; shipment.phase=ShipmentPhase::Completed; }
                    }
                }
                return ShipmentResult::Success;
            }
        return ShipmentResult::InvalidRoute;
    }
    void WorldShipmentSystem::tick(World& world, double minute, double elapsed)
    {
        if (!std::isfinite(elapsed) || elapsed<=0 || !std::isfinite(minute)) return;
        for (auto& shipment : world.shipments_)
        {
            if (!shipment.active() || shipment.phase==ShipmentPhase::Blocked) continue;
            double remaining=elapsed+shipment.deferredMinutes; shipment.deferredMinutes=0;
            std::size_t steps=0;
            while (remaining>1e-9 && steps++<4096)
            {
                if (shipment.phase==ShipmentPhase::Waiting)
                {
                    const auto* source=world.settlement(shipment.source); const auto* target=world.settlement(shipment.destination);
                    if (!source || !target || source->ownerRealmId()!=shipment.owner || target->ownerRealmId()!=shipment.owner || !shipment.repeating)
                    { shipment.phase=ShipmentPhase::Completed; break; }
                    if (!load(world,shipment)) break; // goods may arrive on a future tick
                }
                if (!shipment.moving() || shipment.path.size()<2) break;
                const double used=std::min(remaining,WorldShipment::MinutesPerTile-shipment.stepMinutes);
                shipment.stepMinutes+=used; remaining-=used;
                if (shipment.stepMinutes+1e-9<WorldShipment::MinutesPerTile) break;
                const auto next=shipment.nextIndex();
                const auto* tile=world.grid().tile(shipment.path[next]);
                if (!tile || tile->terrain!=TerrainType::Land)
                { shipment.phase=ShipmentPhase::Blocked; shipment.stepMinutes=0; break; }
                shipment.tileIndex=next; shipment.stepMinutes=0;
                if (shipment.phase==ShipmentPhase::Outbound && shipment.tileIndex+1==shipment.path.size())
                {
                    const auto* target=world.settlement(shipment.destination);
                    if (target && target->ownerRealmId()==shipment.owner && target->position()==shipment.position() &&
                        receive(world,target->id(),shipment.resource,shipment.cargo,minute+elapsed-remaining))
                    { shipment.cargo=0; ++shipment.deliveries; }
                    else shipment.repeating=false; // cargo returns intact if delivery is no longer authorized
                    shipment.phase=ShipmentPhase::Returning;
                }
                else if (shipment.phase==ShipmentPhase::Returning && shipment.tileIndex==0)
                {
                    const auto* source=world.settlement(shipment.source);
                    if (shipment.cargo>0)
                    {
                        if (!source || source->ownerRealmId()!=shipment.owner || source->position()!=shipment.position() ||
                            !receive(world,source->id(),shipment.resource,shipment.cargo,minute+elapsed-remaining))
                        { shipment.phase=ShipmentPhase::Blocked; break; }
                        shipment.cargo=0;
                    }
                    shipment.phase=shipment.repeating?ShipmentPhase::Waiting:ShipmentPhase::Completed;
                }
            }
            if (steps>=4096 && remaining>0 && shipment.moving()) shipment.deferredMinutes=remaining;
        }
    }
    std::string_view shipmentResultText(ShipmentResult result) noexcept
    {
        switch(result)
        {
        case ShipmentResult::Success: return "Shipment dispatched.";
        case ShipmentResult::InvalidSettlement: return "Choose an existing settlement.";
        case ShipmentResult::NotOwned: return "Both settlements must belong to your realm.";
        case ShipmentResult::SameSettlement: return "Choose a different settlement.";
        case ShipmentResult::InvalidResource: return "Unknown resource.";
        case ShipmentResult::InvalidAmount: return "Choose between 1 and 1,000,000 goods.";
        case ShipmentResult::InsufficientGoods: return "Not enough unreserved public stock to ship.";
        case ShipmentResult::NoLandRoute: return "No passable land route. Sea transport is not available.";
        case ShipmentResult::RouteLimit: return "This realm already has 64 active shipment routes.";
        case ShipmentResult::InvalidRoute: return "This shipment no longer exists.";
        }
        return "Shipment unavailable.";
    }
}

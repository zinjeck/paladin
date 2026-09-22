#include "simulation/WorldMarketSystem.h"

namespace Paladin
{
    bool WorldMarketSystem::setOrder(
        World& world,
        RealmId actor,
        SettlementId city,
        SettlementObjectId depot,
        std::string_view resource,
        TradeDirection direction,
        int quantity,
        bool enabled
    )
    {
        auto* settlement = world.settlement(city);
        if (!settlement || settlement->ownerRealmId() != actor ||
            !SettlementResourceCatalog::definition(resource))
        {
            return false;
        }
        auto* map = settlement->simulationState().localMap_.get();
        const auto* object =
            map ? map->objectState().completedObject(depot) : nullptr;
        if (!object ||
            object->objectTypeId != SettlementObjectTypes::TradeDepot)
        {
            return false;
        }
        auto& order = map->trade.order(depot, resource);
        if (order.shipment && enabled)
        {
            static_cast<void>(WorldShipmentSystem::stop(world, actor, order.shipment));
            order.shipment = {};
        }
        if (!enabled && order.shipment)
        {
            static_cast<void>(
                WorldShipmentSystem::stop(world, actor, order.shipment)
            );
        }
        order.direction = direction;
        order.quantity =
            std::clamp(quantity, 1, WorldShipmentSystem::MaximumShipment);
        order.enabled = enabled;
        order.standing = true;
        order.fulfilled = false;
        order.collectionAuthorized = false;
        order.collectionIssue = DepotCollectionIssue::None;
        map->activities.cancelDepotTasks(*map, settlement->simulationState().citizens_, depot,
                                         double(world.time().totalGameMinutes()));
        map->trade.nextOrderMinute = 0;
        order.nextAttemptMinute = double(world.time().totalGameMinutes());
        order.status = enabled ? "Waiting for the next caravan."
                               : "Standing order stopped.";
        return true;
    }
    bool WorldMarketSystem::placeOrder(
        World& world, RealmId actor, SettlementId city,
        SettlementObjectId depot, std::string_view resource,
        TradeDirection direction, int quantity, bool standing)
    {
        auto* settlement = world.settlement(city);
        if (!settlement || settlement->ownerRealmId() != actor ||
            !SettlementResourceCatalog::definition(resource) || quantity < 1 ||
            quantity > WorldShipmentSystem::MaximumShipment)
        { return false; }
        auto* map = settlement->simulationState().localMap_.get();
        const auto* building = map ? map->objectState().completedObject(depot) : nullptr;
        if (!building || building->objectTypeId != SettlementObjectTypes::TradeDepot ||
            map->trade.orders.size() >= 64)
        { return false; }
        SettlementTradeOrder order;
        order.id = ++map->trade.nextOrderId;
        order.depot = depot;
        order.resource = resource;
        order.direction = direction;
        order.quantity = quantity;
        order.standing = standing;
        order.enabled = true;
        order.status = "Waiting for a funded buyer/supplier and land route.";
        map->trade.orders.push_back(std::move(order));
        map->trade.nextOrderMinute = 0;
        return true;
    }
    bool WorldMarketSystem::cancelOrder(World& world, RealmId actor, SettlementId city,
                            SettlementObjectId depot, std::uint64_t id)
    {
        auto* settlement = world.settlement(city);
        if (!settlement || settlement->ownerRealmId() != actor) { return false; }
        auto* map = settlement->simulationState().localMap_.get();
        if (!map) { return false; }
        for (auto& order : map->trade.orders)
        {
            if (order.depot != depot || order.id != id) { continue; }
            order.enabled = order.collectionAuthorized = false;
            if (order.shipment)
            { static_cast<void>(WorldShipmentSystem::stop(world, actor, order.shipment)); }
            map->activities.cancelDepotTasks(*map, settlement->simulationState().citizens_, depot,
                                             double(world.time().totalGameMinutes()));
            std::erase_if(map->trade.orders, [id](const auto& o) { return o.id == id; });
            return true;
        }
        return false;
    }
    void WorldMarketSystem::tickOrders(World& world, double minute)
    {
        SettlementTradeOrder* nextOrder = nullptr;
        SettlementMap* nextMap = nullptr;
        SettlementId nextCity;
        RealmId nextActor;
        for (const auto& record : world.settlements())
        {
            auto* map = world.settlement(record.id())
                            ->simulationState()
                            .localMap_.get();
            if (!map)
            {
                continue;
            }
            map->trade.expire(minute);
            for (auto& order : map->trade.orders)
            {
                if (!map->objectState().completedObject(order.depot))
                {
                    if (order.shipment)
                    { static_cast<void>(WorldShipmentSystem::stop(world, record.ownerRealmId(), order.shipment)); }
                    order.enabled = order.collectionAuthorized = false;
                }
                const auto* shipment = world.shipment(order.shipment);
                order.collectionAuthorized = order.enabled && shipment &&
                    order.direction == TradeDirection::Export &&
                    shipment->awaitingCollection && shipment->active() &&
                    shipment->phase != ShipmentPhase::Blocked;
                if (shipment && shipment->deliveries) { order.fulfilled = true; }
            }
            // Synchronize every batch before distributing staged stock between
            // order cards. A loaded caravan must no longer claim that stock.
            for (auto& order : map->trade.orders)
            {
                const auto* shipment = world.shipment(order.shipment);
                if (shipment && shipment->active())
                {
                    order.status = shipment->phase == ShipmentPhase::Blocked
                        ? "Route blocked | cargo retained"
                        : shipment->awaitingCollection
                            ? DepotCollection::status(*map, record.simulationState().citizens(), order, minute)
                        : shipment->cargo ? "Caravan delivering goods"
                                          : "Caravan returning";
                }
            }
            std::erase_if(
                map->trade.orders,
                [&](const auto& order)
                { return !map->objectState().completedObject(order.depot) ||
                         (!order.standing && order.fulfilled); }
            );
            if (minute < map->trade.nextOrderMinute ||
                map->trade.orders.empty())
            {
                continue;
            }
            // Skip in-flight, disabled and cooling-down orders immediately.
            // Previously each occupied slot consumed another five minutes, and
            // returning here left every later city's statuses out of date.
            for (std::size_t i = 0; i < map->trade.orders.size(); ++i)
            {
                auto& order = map->trade.orders[
                    (map->trade.orderCursor + i) % map->trade.orders.size()];
                const auto* shipment = world.shipment(order.shipment);
                if (!order.enabled || minute < order.nextAttemptMinute ||
                    (shipment && shipment->active()))
                { continue; }
                if (!nextOrder || order.nextAttemptMinute < nextOrder->nextAttemptMinute)
                {
                    nextOrder = &order;
                    nextMap = map;
                    nextCity = record.id();
                    nextActor = record.ownerRealmId();
                }
            }
        }
        // Oldest eligible attempt wins across cities. Route searches stay
        // bounded to one per tick without starving the later settlements.
        if (!nextOrder) { return; }
        auto& order = *nextOrder;
        nextMap->trade.nextOrderMinute = minute + 5;
        nextMap->trade.orderCursor =
            (std::size_t(nextOrder - nextMap->trade.orders.data()) + 1) %
            nextMap->trade.orders.size();
        order.nextAttemptMinute = minute + 30;
        order.collectionIssue = DepotCollectionIssue::None;
        order.shipment = {};
        const auto result = dispatch(world, nextActor, nextCity, order.depot,
            order.resource, order.direction, order.quantity, &order.shipment,
            order.direction == TradeDirection::Export);
        order.status = shipmentResultText(result);
        order.collectionAuthorized = false;
        if (const auto* shipment = world.shipment(order.shipment))
        {
            order.collectionAuthorized = order.enabled && shipment->active() &&
                shipment->awaitingCollection && order.direction == TradeDirection::Export;
            order.fulfilled = false;
            if (order.collectionAuthorized)
            {
                order.status = DepotCollection::status(*nextMap,
                    world.settlement(nextCity)->simulationState().citizens(), order, minute);
            }
        }
        if (result == ShipmentResult::NoLandRoute)
        { order.nextAttemptMinute = minute + 5; }
    }
}

#pragma once

#include "simulation/DiplomacySystem.h"
#include "simulation/WorldShipmentSystem.h"
#include "world/World.h"
#include "world/WorldGeography.h"
#include "world/settlements/SettlementFoodDemand.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace Paladin
{
    class WorldMarketSystem
    {
    public:
        static ResourceMarketQuote quote(
            const World& world,
            const Settlement& city,
            std::string_view resource
        )
        {
            const auto& state = city.simulationState();
            const auto* local = state.localMap();
            if (!local)
            {
                return state.economy()
                    .quote(state.stockpile(), city.population(), resource);
            }
            const auto* definition =
                SettlementResourceCatalog::definition(resource);
            if (!definition)
            {
                return {};
            }
            const auto& report = local->commerce.dailyResourceReport(
                *local,
                state.citizens(),
                double(world.time().totalGameMinutes())
            );
            const auto found = report.find(std::string(resource));
            double need = found == report.end() ? 0 : found->second.depletion;
            double stock = WorldShipmentSystem::total(city, resource);
            if (definition->edible && !definition->emergencyOnly)
            {
                // Foods substitute for one another; do not purchase a complete
                // diet separately for every edible resource.
                need = 0;
                stock = 0;
                for (const auto& food :
                     SettlementResourceCatalog::definitions())
                {
                    if (!food.edible || food.emergencyOnly)
                    {
                        continue;
                    }
                    if (const auto flow = report.find(std::string(food.id));
                        flow != report.end())
                    {
                        need += flow->second.depletion;
                    }
                    stock += WorldShipmentSystem::total(city, food.id);
                }
                double diet = 0;
                for (const auto& citizen : state.citizens().citizens())
                {
                    diet +=
                        citizenFoodPerDay(citizen, local->activities.policy);
                }
                // Empty pantries have no observed meals, but still need food.
                need = std::max(need, diet);
            }
            if (resource == SettlementResourceTypes::Lumber)
            {
                need = std::max(need, double(city.population()) * .75);
            }
            SettlementEconomy forecast;
            const std::vector<ResourceFlowRate> rates{
                {std::string(resource), 0, need, 0}
            };
            if (!forecast.configure(rates))
            {
                return {};
            }
            ResourceStockpile snapshot;
            if (!snapshot.setAmount(std::string(resource), stock))
            {
                return {};
            }
            auto result = forecast.quote(snapshot, 1, resource);
            result.dailyOutput =
                found == report.end() ? 0 : found->second.production;
            result.offered = std::min(
                result.offered,
                double(WorldShipmentSystem::available(city, resource))
            );
            return result;
        }
        struct DepotOffer
        {
            SettlementId partner;
            Money unitPrice = 0;
            int available = 0;
            double distance = 0;
            std::size_t treatyPartners = 0;
        };
        static DepotOffer depotOffer(
            const World& world,
            SettlementId cityId,
            SettlementObjectId depot,
            std::string_view resource,
            TradeDirection direction,
            int minimumStock = 1
        )
        {
            DepotOffer result;
            const auto* home = world.settlement(cityId);
            if (!home || !SettlementResourceCatalog::definition(resource))
            {
                return result;
            }
            std::vector<std::pair<double, SettlementId>> candidates;
            for (const auto& candidate : world.settlements())
            {
                if (candidate.ownerRealmId() == home->ownerRealmId() ||
                    !WorldShipmentSystem::hasTradeDepot(candidate))
                {
                    continue;
                }
                const auto* relation = world.diplomacy().between(
                    home->ownerRealmId(),
                    candidate.ownerRealmId()
                );
                if (!relation || !relation->trading || relation->atWar)
                {
                    continue;
                }
                const double distance = geographicDistance(
                    home->position(),
                    candidate.position(),
                    world.grid().width(),
                    world.grid().height()
                );
                if (distance > DiplomacySystem::RangeRadians)
                {
                    continue;
                }
                ++result.treatyPartners;
                const auto* local = home->simulationState().localMap();
                if (local &&
                    local->trade.routeTerrainRevision ==
                        world.grid().revision() &&
                    std::find(
                        local->trade.unreachablePartners.begin(),
                        local->trade.unreachablePartners.end(),
                        candidate.id()
                    ) != local->trade.unreachablePartners.end())
                {
                    continue;
                }
                // Discard empty suppliers before the more expensive market
                // quote. Distance ranking must never hide an eligible ninth
                // (or later) city behind nearer cities without supply/demand.
                if (direction == TradeDirection::Import &&
                    WorldShipmentSystem::available(candidate, resource) <
                        std::max(1, minimumStock))
                {
                    continue;
                }
                candidates.emplace_back(distance, candidate.id());
            }
            std::sort(candidates.begin(), candidates.end());
            for (const auto& [distance, id] : candidates)
            {
                const auto* partner = world.settlement(id);
                const auto price = quote(world, *partner, resource);
                int stock = 0;
                Money unitPrice = price.unitPrice;
                if (direction == TradeDirection::Import)
                {
                    stock = std::min(
                        int(std::min(1000000., price.offered)),
                        WorldShipmentSystem::available(*partner, resource)
                    );
                }
                else
                {
                    const auto* buyer = world.realm(partner->ownerRealmId());
                    double reservedDemand = 0;
                    for (const auto& shipment : world.shipments())
                    {
                        if (shipment.destination == id && shipment.buyer &&
                            shipment.resource == resource && shipment.active() &&
                            shipment.deliveries == 0 &&
                            (shipment.awaitingCollection || shipment.cargo > 0))
                        { reservedDemand += shipment.amount; }
                    }
                    stock = int(std::min(
                        {1000000.,
                         std::max(0.0, price.wanted - reservedDemand),
                         double(
                             buyer ? buyer->treasury->balance /
                                         std::max<Money>(1, unitPrice)
                                   : 0
                         )}
                    ));
                }
                if (stock < std::max(1, minimumStock))
                {
                    continue;
                }
                result.partner = id;
                result.unitPrice = unitPrice;
                result.available = stock;
                result.distance = distance;
                return result;
            }
            return result;
        }
        static ShipmentResult dispatch(
            World& world,
            RealmId actor,
            SettlementId cityId,
            SettlementObjectId depot,
            std::string_view resource,
            TradeDirection direction,
            int quantity,
            ShipmentId* created = nullptr,
            bool collectBeforeLoading = false
        )
        {
            if (created)
            {
                *created = {};
            }
            const auto* home = world.settlement(cityId);
            if (!home || home->ownerRealmId() != actor)
            {
                return ShipmentResult::NotOwned;
            }
            if (quantity <= 0 ||
                quantity > WorldShipmentSystem::MaximumShipment)
            {
                return ShipmentResult::InvalidAmount;
            }
            const auto* map = home->simulationState().localMap();
            const auto* object =
                map ? map->objectState().completedObject(depot) : nullptr;
            if (!object ||
                object->objectTypeId != SettlementObjectTypes::TradeDepot)
            {
                return ShipmentResult::MissingTradeDepot;
            }
            auto* trade =
                world.settlement(cityId)->simulationState().localMap_.get();
            if (trade->trade.routeTerrainRevision != world.grid().revision())
            {
                trade->trade.routeTerrainRevision = world.grid().revision();
                trade->trade.unreachablePartners.clear();
            }
            ShipmentResult last = ShipmentResult::InsufficientGoods;
            // Disconnected nearest ports must not permanently mask the next
            // eligible land neighbour. Cache failures by terrain revision and
            // cap actual route searches per request.
            for (int attempt = 0; attempt < 2; ++attempt)
            {
                const auto offer = depotOffer(
                    world,
                    cityId,
                    depot,
                    resource,
                    direction,
                    quantity
                );
                if (!offer.treatyPartners)
                {
                    return ShipmentResult::TradeAgreementRequired;
                }
                if (!offer.partner)
                {
                    return last;
                }
                const auto* partner = world.settlement(offer.partner);
                if (direction == TradeDirection::Import)
                {
                    const auto inventory =
                        map->logistics.importsForObject(depot);
                    if (map->logistics.receivable(inventory, resource) <
                        quantity)
                    {
                        return ShipmentResult::DepotFull;
                    }
                    last = WorldShipmentSystem::create(
                        world,
                        partner->ownerRealmId(),
                        partner->id(),
                        cityId,
                        resource,
                        quantity,
                        false,
                        created,
                        false,
                        actor,
                        offer.unitPrice,
                        {},
                        depot
                    );
                }
                else
                {
                    last = WorldShipmentSystem::create(
                        world,
                        actor,
                        cityId,
                        partner->id(),
                        resource,
                        quantity,
                        false,
                        created,
                        false,
                        partner->ownerRealmId(),
                        offer.unitPrice,
                        depot,
                        {},
                        collectBeforeLoading
                    );
                }
                if (last != ShipmentResult::NoLandRoute)
                {
                    return last;
                }
                if (trade->trade.unreachablePartners.size() >= 64)
                {
                    trade->trade.unreachablePartners.erase(
                        trade->trade.unreachablePartners.begin()
                    );
                }
                trade->trade.unreachablePartners.push_back(offer.partner);
            }
            return last;
        }
        static bool setOrder(
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
            map->trade.nextOrderMinute = 0;
            order.nextAttemptMinute = double(world.time().totalGameMinutes());
            order.status = enabled ? "Waiting for the next caravan."
                                   : "Standing order stopped.";
            return true;
        }
        static bool placeOrder(
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
        static bool cancelOrder(World& world, RealmId actor, SettlementId city,
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
        static void tickOrders(World& world, double minute)
        {
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
                        shipment->awaitingCollection && shipment->active();
                    if (shipment && shipment->deliveries) { order.fulfilled = true; }
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
                map->trade.nextOrderMinute = minute + 5;
                auto& order =
                    map->trade.orders
                        [map->trade.orderCursor++ % map->trade.orders.size()];
                if (!order.enabled || minute < order.nextAttemptMinute)
                {
                    continue;
                }
                if (const auto* shipment = world.shipment(order.shipment);
                    shipment && shipment->active())
                {
                    order.status = shipment->awaitingCollection
                        ? "Buyer paid into escrow; collecting exact batch."
                        : shipment->phase == ShipmentPhase::Blocked ? "Route blocked; cargo retained."
                        : shipment->cargo ? "Cargo in transit." : "Caravan returning.";
                    continue;
                }
                order.nextAttemptMinute = minute + 30;
                const auto result = dispatch(
                    world,
                    record.ownerRealmId(),
                    record.id(),
                    order.depot,
                    order.resource,
                    order.direction,
                    order.quantity,
                    &order.shipment,
                    order.direction == TradeDirection::Export
                );
                order.status = shipmentResultText(result);
                if (const auto* shipment = world.shipment(order.shipment))
                {
                    order.collectionAuthorized = shipment->awaitingCollection;
                    order.fulfilled = false;
                    if (order.collectionAuthorized)
                    { order.status = "Buyer funded; collecting exact batch."; }
                }
                if (result == ShipmentResult::NoLandRoute)
                {
                    order.nextAttemptMinute = minute + 5;
                }
                // At most one human-configured route search per world tick.
                return;
            }
        }
        static void updateHistory(World& world)
        {
            auto& history = world.marketHistory;
            const double minute = double(world.time().totalGameMinutes());
            if (!history.collecting)
            {
                if (minute < history.nextMinute)
                {
                    return;
                }
                history.pending.clear();
                for (const auto& resource :
                     SettlementResourceCatalog::definitions())
                {
                    WorldResourceMarket row;
                    row.resource = resource.id;
                    history.pending.push_back(std::move(row));
                }
                history.collecting = true;
                history.cityCursor = 0;
                history.sampleMinute = minute;
            }
            // Bounded census work, never a whole world scan in the draw path.
            const auto cities = world.settlements();
            const auto end = std::min(cities.size(), history.cityCursor + 4);
            for (; history.cityCursor < end; ++history.cityCursor)
            {
                const auto& city = cities[history.cityCursor];
                for (auto& row : history.pending)
                {
                    const auto q = quote(world, city, row.resource);
                    const double weight =
                        std::max(1., q.dailyNeed + q.dailyOutput);
                    row.price += q.unitPrice * weight;
                    row.weight += weight;
                    row.supply += q.offered;
                    row.demand += q.wanted;
                    auto at = std::find_if(
                        row.producers.begin(),
                        row.producers.end(),
                        [&](const auto& producer)
                        { return producer.realm == city.ownerRealmId(); }
                    );
                    if (at == row.producers.end())
                    {
                        row.producers.push_back(
                            {city.ownerRealmId(), q.dailyOutput}
                        );
                    }
                    else
                    {
                        at->dailyOutput += q.dailyOutput;
                    }
                }
            }
            if (history.cityCursor < cities.size())
            {
                return;
            }
            for (auto& row : history.pending)
            {
                row.price /= std::max(1., row.weight);
                for (auto& old : history.resources)
                {
                    if (old.resource == row.resource)
                    {
                        row.history = std::move(old.history);
                    }
                }
                row.history.push_back({history.sampleMinute, row.price});
                while (row.history.size() > 97)
                {
                    row.history.pop_front();
                }
                std::erase_if(
                    row.producers,
                    [](const auto& p) { return p.dailyOutput <= 0; }
                );
                std::stable_sort(
                    row.producers.begin(),
                    row.producers.end(),
                    [](const auto& a, const auto& b)
                    { return a.dailyOutput > b.dailyOutput; }
                );
                if (row.producers.size() > 5)
                {
                    row.producers.resize(5);
                }
            }
            history.resources = std::move(history.pending);
            history.collecting = false;
            history.nextMinute = (std::floor(minute / 240) + 1) * 240;
        }
        static void tick(World& world)
        {
            const double minute = double(world.time().totalGameMinutes());
            updateHistory(world);
            tickOrders(world, minute);
            for (const auto& record : world.realms())
            {
                if (!record.aiControlled || minute < record.nextMarketMinute)
                {
                    continue;
                }
                auto* buyer = world.realm(record.id());
                buyer->nextMarketMinute =
                    minute + 360 + record.id().value() % 31;
                if (buyer->marketRouteRevision != world.grid().revision())
                {
                    buyer->marketRouteRevision = world.grid().revision();
                    buyer->unreachableMarketRoutes.clear();
                }
                std::vector<SettlementId> cities;
                for (const auto& city : world.settlements())
                {
                    if (city.ownerRealmId() == buyer->id() &&
                        city.population() > 0 &&
                        !city.simulationState().hasLocalMap() &&
                        WorldShipmentSystem::hasTradeDepot(city))
                    {
                        cities.push_back(city.id());
                    }
                }
                if (cities.empty() || buyer->treasury->balance <= 0)
                {
                    continue;
                }
                const auto cityId =
                    cities[buyer->marketCityCursor++ % cities.size()];
                const auto* destination = world.settlement(cityId);
                std::vector<std::pair<double, SettlementId>> candidates;
                for (const auto& city : world.settlements())
                {
                    if (city.ownerRealmId() == buyer->id() ||
                        !WorldShipmentSystem::hasTradeDepot(city))
                    {
                        continue;
                    }
                    const auto* seller = world.realm(city.ownerRealmId());
                    const auto* relation = world.diplomacy().between(
                        buyer->id(),
                        city.ownerRealmId()
                    );
                    if (!seller || !relation || !relation->trading ||
                        relation->atWar)
                    {
                        continue;
                    }
                    const double distance = geographicDistance(
                        destination->position(),
                        city.position(),
                        world.grid().width(),
                        world.grid().height()
                    );
                    const auto route = std::pair{city.id(), cityId};
                    if (distance <= DiplomacySystem::RangeRadians &&
                        std::find(
                            buyer->unreachableMarketRoutes.begin(),
                            buyer->unreachableMarketRoutes.end(),
                            route
                        ) == buyer->unreachableMarketRoutes.end())
                    {
                        candidates.emplace_back(distance, city.id());
                    }
                }
                std::sort(candidates.begin(), candidates.end());
                // Qualify stock, price and demand before ranking. Eight empty
                // nearby cities must not hide a ninth legitimate supplier.
                // These are aggregate quotes; only the winning offer below
                // performs a route search (still at most one per tick).
                struct Offer
                {
                    SettlementId seller;
                    std::string resource;
                    int amount = 0;
                    Money price = 0;
                    double priority = 0;
                } best;
                const auto& state = destination->simulationState();
                for (const auto& resource :
                     SettlementResourceCatalog::definitions())
                {
                    const auto demand = quote(world, *destination, resource.id);
                    double incoming = 0;
                    for (const auto& shipment : world.shipments())
                    {
                        const auto* incomingResource =
                            SettlementResourceCatalog::definition(
                                shipment.resource
                            );
                        const bool interchangeable =
                            state.hasLocalMap() && resource.edible &&
                            !resource.emergencyOnly && incomingResource &&
                            incomingResource->edible &&
                            !incomingResource->emergencyOnly;
                        if (shipment.destination == cityId &&
                            (shipment.resource == resource.id ||
                             interchangeable) &&
                            shipment.phase == ShipmentPhase::Outbound)
                        {
                            incoming += shipment.cargo;
                        }
                    }
                    const double wanted =
                        std::max(0., demand.wanted - incoming);
                    if (wanted < 1)
                    {
                        continue;
                    }
                    for (const auto& [distance, sellerId] : candidates)
                    {
                        const auto* seller = world.settlement(sellerId);
                        if (seller->simulationState().hasLocalMap())
                        {
                            // Player depots export only the goods explicitly
                            // configured in their own standing orders.
                            continue;
                        }
                        const auto supply = quote(world, *seller, resource.id);
                        if (supply.offered < 1 ||
                            supply.unitPrice > demand.unitPrice)
                        {
                            continue;
                        }
                        const Money price =
                            (supply.unitPrice + demand.unitPrice) / 2;
                        const Money budget = buyer->treasury->balance / 20;
                        const int amount = int(std::min(
                            {wanted,
                             supply.offered,
                             double(WorldShipmentSystem::available(
                                 *seller,
                                 resource.id
                             )),
                             double(budget / std::max<Money>(1, price)),
                             1000.}
                        ));
                        const double urgency =
                            (resource.edible ? 4. : 1.) * amount /
                            std::max(1., demand.dailyNeed) / (1 + distance);
                        if (amount > 0 && urgency > best.priority)
                        {
                            best = {
                                sellerId,
                                std::string(resource.id),
                                amount,
                                price,
                                urgency
                            };
                        }
                    }
                }
                if (best.amount > 0)
                {
                    const auto sellerRealm =
                        world.settlement(best.seller)->ownerRealmId();
                    const auto result = WorldShipmentSystem::create(
                        world,
                        sellerRealm,
                        best.seller,
                        cityId,
                        best.resource,
                        best.amount,
                        false,
                        nullptr,
                        true,
                        buyer->id(),
                        best.price
                    );
                    if (result == ShipmentResult::NoLandRoute)
                    {
                        if (buyer->unreachableMarketRoutes.size() >= 256)
                        {
                            buyer->unreachableMarketRoutes.erase(
                                buyer->unreachableMarketRoutes.begin()
                            );
                        }
                        buyer->unreachableMarketRoutes.emplace_back(
                            best.seller,
                            cityId
                        );
                        buyer->nextMarketMinute = minute + 15;
                    }
                }
                // One buyer and at most one bounded route search per tick.
                return;
            }
        }
    };
} // namespace Paladin

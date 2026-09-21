#include "simulation/WorldShipmentSystem.h"
#include "simulation/DiplomacySystem.h"
#include "simulation/LocalTradeVisitSystem.h"
#include "simulation/WorldLandNavigation.h"
#include "simulation/systems/SettlementNavigation.h"
#include "world/World.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include "world/settlements/StrategicFood.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Paladin
{
    namespace
    {
        bool authorizedDestination(
            const World& world,
            const WorldShipment& shipment
        )
        {
            const auto* target = world.settlement(shipment.destination);
            if (!target)
            {
                return false;
            }
            if (!shipment.buyer)
            {
                return target->ownerRealmId() == shipment.owner;
            }
            const auto* relation =
                world.diplomacy().between(shipment.owner, shipment.buyer);
            return target->ownerRealmId() == shipment.buyer && relation &&
                   relation->trading && !relation->atWar &&
                   DiplomacySystem::inRange(
                       world,
                       shipment.owner,
                       shipment.buyer
                   );
        }
        bool paymentFits(const World& world, const WorldShipment& shipment)
        {
            if (!shipment.buyer)
            {
                return true;
            }
            const auto* seller = world.realm(shipment.owner);
            return seller && seller->treasury && shipment.escrow >= 0 &&
                   seller->treasury->balance <=
                       std::numeric_limits<Money>::max() - shipment.escrow;
        }
        void refund(World& world, WorldShipment& shipment)
        {
            if (!shipment.escrow)
            {
                return;
            }
            if (auto* buyer = world.realm(shipment.buyer);
                buyer && buyer->treasury &&
                buyer->treasury->balance <=
                    std::numeric_limits<Money>::max() - shipment.escrow)
            {
                buyer->treasury->balance += shipment.escrow;
                shipment.escrow = 0;
            }
        }
        bool publicStorage(InventoryKind kind)
        {
            return kind == InventoryKind::TradeDepot;
        }
        SettlementTilePosition unloadingTile(const SettlementMap& map)
        {
            SettlementTilePosition centre{
                map.grid().width() / 2,
                map.grid().height() / 2
            };
            for (const auto& object : map.objectState().completedObjects())
            {
                if (object.objectTypeId == SettlementObjectTypes::TradeDepot)
                {
                    // Outside the loading yard, leaving its approach clear.
                    centre = {
                        object.footprint.topLeft.x + object.footprint.width / 2,
                        object.footprint.topLeft.y + object.footprint.height
                    };
                    break;
                }
            }
            SettlementNavigation navigation;
            for (int radius = 0;
                 radius < std::max(map.grid().width(), map.grid().height());
                 ++radius)
            {
                for (int y = centre.y - radius; y <= centre.y + radius; ++y)
                {
                    for (int x = centre.x - radius; x <= centre.x + radius; ++x)
                    {
                        if (radius && x != centre.x - radius &&
                            x != centre.x + radius && y != centre.y - radius &&
                            y != centre.y + radius)
                        {
                            continue;
                        }
                        if (navigation.walkable(map, {x, y}))
                        {
                            return {x, y};
                        }
                    }
                }
            }
            return {-1, -1};
        }
    } // namespace
    bool WorldShipmentSystem::hasTradeDepot(const Settlement& city)
    {
        const auto* map = city.simulationState().localMap();
        if (!map)
        {
            return city.simulationState().isInitialized();
        }
        return std::any_of(
            map->logistics.inventories().begin(),
            map->logistics.inventories().end(),
            [](const auto& inventory)
            { return inventory.kind == InventoryKind::TradeDepot; }
        );
    }
    double WorldShipmentSystem::total(
        const Settlement& city,
        std::string_view resource
    )
    {
        const auto& state = city.simulationState();
        return state.localMap() ? state.localMap()->logistics.total(resource)
                                : state.stockpile().amount(resource);
    }
    int WorldShipmentSystem::available(
        const Settlement& city,
        std::string_view resource,
        SettlementObjectId depot
    )
    {
        if (!SettlementResourceCatalog::definition(resource))
        {
            return 0;
        }
        if (const auto* local = city.simulationState().localMap())
        {
            std::int64_t count = 0;
            for (const auto& inventory : local->logistics.inventories())
            {
                if (publicStorage(inventory.kind) &&
                    (!depot || inventory.objectId == depot))
                {
                    count += local->logistics.available(inventory.id, resource);
                }
            }
            return int(std::min<std::int64_t>(count, MaximumShipment));
        }
        return int(std::clamp(
            std::floor(city.simulationState().stockpile().amount(resource)),
            0.0,
            double(MaximumShipment)
        ));
    }
    bool WorldShipmentSystem::load(World& world, WorldShipment& shipment)
    {
        auto* source = world.settlement(shipment.source);
        auto* target = world.settlement(shipment.destination);
        if (!source || !target || !hasTradeDepot(*source) ||
            !hasTradeDepot(*target) ||
            source->ownerRealmId() != shipment.owner ||
            !authorizedDestination(world, shipment) ||
            available(*source, shipment.resource, shipment.sourceDepot) <
                shipment.amount)
        {
            return false;
        }
        const auto* cargoDefinition =
            SettlementResourceCatalog::definition(shipment.resource);
        if (shipment.aiManaged && cargoDefinition && cargoDefinition->edible &&
            !cargoDefinition->emergencyOnly &&
            !source->simulationState().hasLocalMap() &&
            civilianFood(source->simulationState().stockpile()) -
                    shipment.amount <
                2. * source->population())
        {
            return false;
        }
        Money payment = 0;
        if (shipment.buyer)
        {
            const auto* buyer = world.realm(shipment.buyer);
            if (!buyer || shipment.unitPrice <= 0 || shipment.escrow ||
                shipment.unitPrice >
                    std::numeric_limits<Money>::max() / shipment.amount)
            {
                return false;
            }
            payment = shipment.unitPrice * shipment.amount;
            if (!buyer->treasury || buyer->treasury->balance < payment)
            {
                return false;
            }
        }
        if (auto* local = source->simulationState().localMap_.get())
        {
            int remaining = shipment.amount;
            for (const auto& inventory : local->logistics.inventories())
            {
                if (!publicStorage(inventory.kind) ||
                    (shipment.sourceDepot &&
                     inventory.objectId != shipment.sourceDepot))
                {
                    continue;
                }
                const int amount = std::min(
                    remaining,
                    local->logistics.available(inventory.id, shipment.resource)
                );
                if (amount > 0 && local->logistics.consumeAvailable(
                                      inventory.id,
                                      shipment.resource,
                                      amount
                                  ))
                {
                    remaining -= amount;
                }
                if (!remaining)
                {
                    break;
                }
            }
        }
        else if (!source->simulationState().stockpile().addAmount(
                     shipment.resource,
                     -shipment.amount
                 ))
        {
            return false;
        }
        if (payment)
        {
            world.realm(shipment.buyer)->treasury->balance -= payment;
            shipment.escrow = payment;
        }
        if (auto* local = source->simulationState().localMap_.get())
        {
            LocalTradeVisitSystem::arrive(
                *local,
                shipment.id,
                shipment.sourceDepot,
                shipment.resource,
                shipment.amount,
                false,
                double(world.time().totalGameMinutes())
            );
        }
        shipment.cargo = shipment.amount;
        shipment.phase = ShipmentPhase::Outbound;
        shipment.stepMinutes = 0;
        return true;
    }
    bool WorldShipmentSystem::receive(
        World& world,
        SettlementId id,
        std::string_view resource,
        int amount,
        double minute,
        SettlementObjectId depot,
        bool imported,
        bool strictCapacity
    )
    {
        auto* city = world.settlement(id);
        if (!city || amount <= 0 || amount > MaximumShipment ||
            !SettlementResourceCatalog::definition(resource))
        {
            return false;
        }
        auto* local = city->simulationState().localMap_.get();
        if (!local)
        {
            return city->simulationState().stockpile().addAmount(
                resource,
                amount
            );
        }
        if (!hasTradeDepot(*city))
        {
            return false;
        }
        const auto storageKind =
            imported ? InventoryKind::TradeImports : InventoryKind::TradeDepot;
        if (strictCapacity)
        {
            std::int64_t room = 0;
            for (const auto& inventory : local->logistics.inventories())
            {
                if (inventory.kind == storageKind &&
                    (!depot || inventory.objectId == depot))
                {
                    room += local->logistics.receivable(inventory.id, resource);
                }
            }
            if (room < amount)
            {
                return false;
            }
        }
        const auto dropAt = unloadingTile(*local);
        if (dropAt.x < 0)
        {
            return false;
        }
        // Resolve every inventory by ID. Adding an overflow pile may grow its
        // vector, so it deliberately happens after the storage iteration.
        int remaining = amount;
        for (const auto& inventory : local->logistics.inventories())
        {
            if (inventory.kind != storageKind ||
                (depot && inventory.objectId != depot))
            {
                continue;
            }
            const int fit = std::min(
                remaining,
                local->logistics.receivable(inventory.id, resource)
            );
            if (fit > 0 &&
                local->logistics.add(inventory.id, resource, fit, minute))
            {
                remaining -= fit;
            }
            if (!remaining)
            {
                return true;
            }
        }
        return bool(local->logistics.drop(dropAt, resource, remaining, minute));
    }
    ShipmentResult WorldShipmentSystem::create(
        World& world,
        RealmId actor,
        SettlementId from,
        SettlementId to,
        std::string_view resource,
        int amount,
        bool repeating,
        ShipmentId* created,
        bool aiManaged,
        RealmId buyer,
        std::int64_t unitPrice,
        SettlementObjectId sourceDepot,
        SettlementObjectId destinationDepot
    )
    {
        if (created)
        {
            *created = {};
        }
        const auto* source = world.settlement(from);
        const auto* target = world.settlement(to);
        if (!source || !target || !source->simulationState().isInitialized() ||
            !target->simulationState().isInitialized())
        {
            return ShipmentResult::InvalidSettlement;
        }
        if (!actor || source->ownerRealmId() != actor ||
            (buyer ? target->ownerRealmId() != buyer || buyer == actor
                   : target->ownerRealmId() != actor))
        {
            return ShipmentResult::NotOwned;
        }
        if (buyer)
        {
            const auto* relation = world.diplomacy().between(actor, buyer);
            if (!relation || !relation->trading || relation->atWar ||
                repeating || unitPrice <= 0 ||
                !DiplomacySystem::inRange(world, actor, buyer))
            {
                return ShipmentResult::TradeAgreementRequired;
            }
        }
        if (!hasTradeDepot(*source) || !hasTradeDepot(*target))
        {
            return ShipmentResult::MissingTradeDepot;
        }
        if (from == to || source->position() == target->position())
        {
            return ShipmentResult::SameSettlement;
        }
        if (!SettlementResourceCatalog::definition(resource))
        {
            return ShipmentResult::InvalidResource;
        }
        if (amount <= 0 || amount > MaximumShipment)
        {
            return ShipmentResult::InvalidAmount;
        }
        const auto active = std::count_if(
            world.shipments_.begin(),
            world.shipments_.end(),
            [&](const auto& s) { return s.owner == actor && s.active(); }
        );
        if (active >= MaximumActiveRoutesPerRealm)
        {
            return ShipmentResult::RouteLimit;
        }
        if (buyer)
        {
            const auto* payer = world.realm(buyer);
            if (!payer || !payer->treasury || unitPrice <= 0 ||
                unitPrice > std::numeric_limits<Money>::max() / amount ||
                payer->treasury->balance < unitPrice * amount)
            {
                return ShipmentResult::InsufficientMoney;
            }
        }
        if (available(*source, resource, sourceDepot) < amount)
        {
            return ShipmentResult::InsufficientGoods;
        }
        auto path = worldLandRoute(
            world.grid(),
            source->position(),
            target->position()
        );
        if (!path || path->size() < 2)
        {
            return ShipmentResult::NoLandRoute;
        }
        WorldShipment shipment;
        shipment.id = world.shipmentIds_.generate();
        shipment.owner = actor;
        shipment.source = from;
        shipment.destination = to;
        shipment.buyer = buyer;
        shipment.sourceDepot = sourceDepot;
        shipment.destinationDepot = destinationDepot;
        shipment.unitPrice = unitPrice;
        shipment.resource = resource;
        shipment.amount = amount;
        shipment.repeating = repeating;
        shipment.aiManaged = aiManaged;
        shipment.path = std::move(*path);
        // Strategic caravans represent a trade connection; local horses carry
        // its presentation. Near neighbours arrive in roughly 20-60 minutes.
        if (buyer)
        {
            shipment.minutesPerTile =
                std::max(1.5, 20. / double(shipment.path.size() - 1));
        }
        shipment.wrapWidth = world.grid().width();
        if (!load(world, shipment))
        {
            return ShipmentResult::InsufficientGoods;
        }
        // Finished route records are expendable history, unlike active cargo.
        std::erase_if(
            world.shipments_,
            [&](const auto& s)
            { return s.owner == actor && !s.active() && !s.cargo && !s.escrow; }
        );
        const auto id = shipment.id;
        world.shipments_.push_back(std::move(shipment));
        if (created)
        {
            *created = id;
        }
        return ShipmentResult::Success;
    }
    ShipmentResult WorldShipmentSystem::stop(
        World& world,
        RealmId actor,
        ShipmentId id
    )
    {
        for (auto& shipment : world.shipments_)
        {
            if (shipment.id == id)
            {
                if (!actor ||
                    (shipment.owner != actor && shipment.buyer != actor))
                {
                    return ShipmentResult::NotOwned;
                }
                shipment.repeating = false;
                if (shipment.phase == ShipmentPhase::Waiting)
                {
                    shipment.phase = ShipmentPhase::Completed;
                }
                if (shipment.phase == ShipmentPhase::Blocked)
                {
                    const auto* home = world.settlement(shipment.source);
                    if (home && home->ownerRealmId() == actor)
                    {
                        auto back = worldLandRoute(
                            world.grid(),
                            home->position(),
                            shipment.position()
                        );
                        if (back && back->size() > 1)
                        {
                            shipment.path = std::move(*back);
                            shipment.tileIndex = shipment.path.size() - 1;
                            shipment.phase = ShipmentPhase::Returning;
                            shipment.stepMinutes = 0;
                        }
                        else if (
                            back &&
                            (shipment.cargo == 0 ||
                             receive(
                                 world,
                                 home->id(),
                                 shipment.resource,
                                 shipment.cargo,
                                 double(world.time().totalGameMinutes()),
                                 shipment.sourceDepot,
                                 false
                             ))
                        )
                        {
                            shipment.cargo = 0;
                            refund(world, shipment);
                            shipment.phase = ShipmentPhase::Completed;
                        }
                    }
                }
                return ShipmentResult::Success;
            }
        }
        return ShipmentResult::InvalidRoute;
    }
    void WorldShipmentSystem::tick(World& world, double minute, double elapsed)
    {
        if (!std::isfinite(elapsed) || elapsed <= 0 || !std::isfinite(minute))
        {
            return;
        }
        bool recoveryAttempted = false;
        for (auto& shipment : world.shipments_)
        {
            if (shipment.phase == ShipmentPhase::Blocked &&
                !recoveryAttempted && minute >= shipment.nextRecoveryMinute)
            {
                recoveryAttempted = true;
                shipment.nextRecoveryMinute = minute + 60;
                // A blocked caravan retains its actual cargo and retries a
                // safe return at most once per hour. Reopened roads/depot
                // reconstruction cannot leave paid goods stuck forever.
                const auto* home = world.settlement(shipment.source);
                if (home && home->ownerRealmId() == shipment.owner)
                {
                    auto route = worldLandRoute(
                        world.grid(),
                        home->position(),
                        shipment.position()
                    );
                    if (route && route->size() > 1)
                    {
                        shipment.path = std::move(*route);
                        shipment.tileIndex = shipment.path.size() - 1;
                        shipment.phase = ShipmentPhase::Returning;
                        shipment.stepMinutes = 0;
                    }
                    else if (
                        route &&
                        (shipment.cargo == 0 || receive(
                                                    world,
                                                    home->id(),
                                                    shipment.resource,
                                                    shipment.cargo,
                                                    minute,
                                                    shipment.sourceDepot,
                                                    false
                                                ))
                    )
                    {
                        shipment.cargo = 0;
                        refund(world, shipment);
                        shipment.phase = shipment.escrow
                                             ? ShipmentPhase::Blocked
                                             : ShipmentPhase::Completed;
                    }
                }
            }
            if (!shipment.active() || shipment.phase == ShipmentPhase::Blocked)
            {
                continue;
            }
            double remaining = elapsed + shipment.deferredMinutes;
            shipment.deferredMinutes = 0;
            std::size_t steps = 0;
            while (remaining > 1e-9 && steps++ < 4096)
            {
                if (shipment.phase == ShipmentPhase::Waiting)
                {
                    const auto* source = world.settlement(shipment.source);
                    const auto* target = world.settlement(shipment.destination);
                    if (!source || !target || !hasTradeDepot(*source) ||
                        !hasTradeDepot(*target) ||
                        source->ownerRealmId() != shipment.owner ||
                        !authorizedDestination(world, shipment) ||
                        !shipment.repeating)
                    {
                        shipment.phase = ShipmentPhase::Completed;
                        break;
                    }
                    if (!load(world, shipment))
                    {
                        break; // goods may arrive on a future tick
                    }
                }
                if (!shipment.moving() || shipment.path.size() < 2)
                {
                    break;
                }
                const auto next = shipment.nextIndex();
                if (next >= shipment.path.size() ||
                    !worldLandStepAllowed(
                        world.grid(),
                        shipment.position(),
                        shipment.path[next],
                        WorldLandMovement::Cardinal
                    ))
                {
                    shipment.phase = ShipmentPhase::Blocked;
                    shipment.stepMinutes = 0;
                    break;
                }
                const double used = std::min(
                    remaining,
                    shipment.minutesPerTile - shipment.stepMinutes
                );
                shipment.stepMinutes += used;
                remaining -= used;
                if (shipment.stepMinutes + 1e-9 < shipment.minutesPerTile)
                {
                    break;
                }
                shipment.tileIndex = next;
                shipment.stepMinutes = 0;
                if (shipment.phase == ShipmentPhase::Outbound &&
                    shipment.tileIndex + 1 == shipment.path.size())
                {
                    const auto* target = world.settlement(shipment.destination);
                    if (target && authorizedDestination(world, shipment) &&
                        paymentFits(world, shipment) &&
                        target->position() == shipment.position() &&
                        receive(
                            world,
                            target->id(),
                            shipment.resource,
                            shipment.cargo,
                            minute + elapsed - remaining,
                            shipment.destinationDepot,
                            bool(shipment.buyer),
                            bool(shipment.buyer)
                        ))
                    {
                        if (shipment.escrow)
                        {
                            world.realm(shipment.owner)->treasury->balance +=
                                shipment.escrow;
                            shipment.escrow = 0;
                        }
                        if (auto* local =
                                world.settlement(shipment.destination)
                                    ->simulationState()
                                    .localMap_.get())
                        {
                            LocalTradeVisitSystem::arrive(
                                *local,
                                shipment.id,
                                shipment.destinationDepot,
                                shipment.resource,
                                shipment.cargo,
                                true,
                                minute + elapsed - remaining
                            );
                        }
                        shipment.cargo = 0;
                        ++shipment.deliveries;
                    }
                    else
                    {
                        shipment.repeating =
                            false; // cargo returns intact if delivery is no
                                   // longer authorized
                    }
                    shipment.phase = ShipmentPhase::Returning;
                }
                else if (
                    shipment.phase == ShipmentPhase::Returning &&
                    shipment.tileIndex == 0
                )
                {
                    const auto* source = world.settlement(shipment.source);
                    if (shipment.cargo > 0)
                    {
                        if (!source ||
                            source->ownerRealmId() != shipment.owner ||
                            source->position() != shipment.position() ||
                            !receive(
                                world,
                                source->id(),
                                shipment.resource,
                                shipment.cargo,
                                minute + elapsed - remaining,
                                shipment.sourceDepot,
                                false
                            ))
                        {
                            shipment.phase = ShipmentPhase::Blocked;
                            break;
                        }
                        shipment.cargo = 0;
                    }
                    refund(world, shipment);
                    shipment.phase = shipment.escrow ? ShipmentPhase::Blocked
                                     : shipment.repeating
                                         ? ShipmentPhase::Waiting
                                         : ShipmentPhase::Completed;
                }
            }
            if (steps >= 4096 && remaining > 0 && shipment.moving())
            {
                shipment.deferredMinutes = remaining;
            }
        }
    }
    std::string_view shipmentResultText(ShipmentResult result) noexcept
    {
        switch (result)
        {
        case ShipmentResult::Success:
            return "Shipment dispatched.";
        case ShipmentResult::InvalidSettlement:
            return "Choose an existing settlement.";
        case ShipmentResult::NotOwned:
            return "Both settlements must belong to your realm.";
        case ShipmentResult::SameSettlement:
            return "Choose a different settlement.";
        case ShipmentResult::InvalidResource:
            return "Unknown resource.";
        case ShipmentResult::InvalidAmount:
            return "Choose between 1 and 1,000,000 goods.";
        case ShipmentResult::InsufficientGoods:
            return "Not enough unreserved public stock to ship.";
        case ShipmentResult::NoLandRoute:
            return "No passable land route. Sea transport is not available.";
        case ShipmentResult::RouteLimit:
            return "This realm already has 64 active shipment routes.";
        case ShipmentResult::MissingTradeDepot:
            return "Build a Trade Depot at both settlements first.";
        case ShipmentResult::TradeAgreementRequired:
            return "An active trade agreement within diplomatic range is "
                   "required.";
        case ShipmentResult::DepotFull:
            return "The depot's import counter has no room for this order.";
        case ShipmentResult::InsufficientMoney:
            return "The buyer has insufficient treasury gold.";
        case ShipmentResult::InvalidRoute:
            return "This shipment no longer exists.";
        }
        return "Shipment unavailable.";
    }
} // namespace Paladin

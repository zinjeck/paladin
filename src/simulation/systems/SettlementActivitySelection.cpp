#include "simulation/systems/SettlementActivitySystem.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/DepotCollection.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include "world/settlements/objects/SettlementDoor.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include "world/settlements/objects/WorkplaceCompound.h"
#include "world/settlements/objects/jobs/market/MarketJob.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace Paladin
{
    CitizenRoutePlan::CitizenRoutePlan(const SettlementCitizen& c)
        : CitizenRoutePlan(c, c.tilePosition)
    {
        destination = c.destination;
        path = c.path;
        pathIndex = c.pathIndex;
        stepProgress = c.stepProgress;
        explicitMovement = c.explicitMovement;
    }

    CitizenRoutePlan::CitizenRoutePlan(
        const SettlementCitizen& c, SettlementTilePosition origin
    )
        : homeId(c.homeId), task{c.task.kind, c.task.delivering},
          tilePosition(origin), destination(origin), stepDuration(c.stepDuration)
    {
    }

    CitizenRoutePlan CitizenRoutePlan::fromPosition(
        SettlementTilePosition origin
    ) const
    {
        CitizenRoutePlan next;
        next.homeId = homeId;
        next.task = task;
        next.tilePosition = next.destination = origin;
        next.stepDuration = stepDuration;
        return next;
    }

    void CitizenRoutePlan::applyTo(SettlementCitizen& c)
    {
        c.path = std::move(path);
        c.pathIndex = pathIndex;
        c.stepProgress = stepProgress;
        c.stepDuration = stepDuration;
        c.destination = destination;
        c.explicitMovement = explicitMovement;
    }

    namespace
    {
        int distance(
            SettlementTilePosition p,
            const SettlementObjectFootprint& f
        )
        {
            return std::abs(
                       p.x -
                       std::clamp(p.x, f.topLeft.x, f.topLeft.x + f.width - 1)
                   ) +
                   std::abs(
                       p.y -
                       std::clamp(p.y, f.topLeft.y, f.topLeft.y + f.height - 1)
                   );
        }
        std::uint64_t jobRouteKey(std::uint64_t id, SettlementTilePosition tile)
        {
            std::uint64_t value = id * 0x9E3779B97F4A7C15ULL;
            value ^= std::uint64_t(std::uint32_t(tile.x)) +
                     0x9E3779B97F4A7C15ULL + (value << 6) + (value >> 2);
            value ^= std::uint64_t(std::uint32_t(tile.y)) +
                     0x9E3779B97F4A7C15ULL + (value << 6) + (value >> 2);
            return value ? value : 1;
        }
        bool failedRoute(
            const SettlementCitizen& citizen,
            InventoryId target,
            SettlementTilePosition origin,
            const SettlementMap& map,
            double minute
        )
        {
            return std::any_of(
                citizen.routeFailures.begin(),
                citizen.routeFailures.end(),
                [&](const auto& failure)
                {
                    return failure.source == target &&
                           failure.untilMinute > minute &&
                           failure.topologyVersion ==
                               map.objectState().navigationVersion() &&
                           distance(origin, {failure.origin, 1, 1}) <= 8;
                }
            );
        }
        void rememberFailure(
            SettlementCitizen& citizen,
            InventoryId target,
            SettlementTilePosition origin,
            const SettlementMap& map,
            double minute
        )
        {
            std::erase_if(
                citizen.routeFailures,
                [&](const auto& f)
                {
                    return f.untilMinute <= minute ||
                           f.topologyVersion !=
                               map.objectState().navigationVersion();
                }
            );
            if (citizen.routeFailures.size() >= 64)
            {
                citizen.routeFailures.erase(citizen.routeFailures.begin());
            }
            citizen.routeFailures.push_back(
                {target,
                 origin,
                 map.objectState().navigationVersion(),
                 minute + 30}
            );
        }
        bool publicStorage(const SettlementInventory& inventory)
        {
            return inventory.kind == InventoryKind::Keep ||
                   inventory.kind == InventoryKind::Stockpile;
        }
    } // namespace

    bool SettlementActivitySystem::routeFailed(
        const SettlementCitizen& citizen,
        RouteFailureDomain domain,
        std::uint64_t target,
        SettlementTilePosition origin,
        const SettlementMap& map,
        double minute
    )
    {
        return std::any_of(
            routeFailures_.begin(),
            routeFailures_.end(),
            [&](const RouteFailure& failure)
            {
                return failure.citizen == citizen.id &&
                       failure.domain == domain && failure.target == target &&
                       failure.untilMinute > minute &&
                       failure.topologyVersion ==
                           map.objectState().navigationVersion() &&
                       distance(origin, {failure.origin, 1, 1}) <= 8;
            }
        );
    }

    void SettlementActivitySystem::rememberRouteFailure(
        const SettlementCitizen& citizen,
        RouteFailureDomain domain,
        std::uint64_t target,
        SettlementTilePosition origin,
        const SettlementMap& map,
        double minute
    )
    {
        const auto topology = map.objectState().navigationVersion();
        std::erase_if(
            routeFailures_,
            [&](const RouteFailure& failure)
            {
                return failure.untilMinute <= minute ||
                       failure.topologyVersion != topology;
            }
        );
        if (routeFailures_.size() >= 256)
        {
            routeFailures_.erase(routeFailures_.begin());
        }
        routeFailures_.push_back(
            {citizen.id, domain, target, origin, topology, minute + 30}
        );
    }

    template<typename RouteState>
    bool SettlementActivitySystem::planRoute(
        SettlementMap& map,
        SettlementCitizenState& citizens,
        RouteState& c,
        const SettlementObjectFootprint& f,
        bool inside
    )
    {
        auto routePolicy = citizens.movementPolicy;
        routePolicy.avoidBuildingFootprints =
            c.task.kind == CitizenTaskKind::AnimalWork && c.task.delivering;
        const auto original = c.tilePosition;
        const bool midStep = c.pathIndex < c.path.size() && c.stepProgress > 0;
        const double progress = midStep ? c.stepProgress : 0;
        const double duration = c.stepDuration;
        std::vector<SettlementTilePosition> exitPath;
        bool successful = false;
        struct RestorePosition
        {
            RouteState& citizen;
            SettlementTilePosition original;
            std::vector<SettlementTilePosition>& exit;
            bool& successful;
            double progress;
            double duration;
            const SettlementMap& map;
            const SettlementNavigation& navigation;
            const CitizenMovementPolicy& movementPolicy;
            ~RestorePosition()
            {
                citizen.tilePosition = original;
                if (successful && !exit.empty())
                {
                    citizen.path
                        .insert(citizen.path.begin(), exit.begin(), exit.end());
                    citizen.explicitMovement = true;
                    citizen.stepProgress = progress;
                    if (progress > 0)
                    {
                        citizen.stepDuration = duration;
                    }
                    else
                    {
                        // A prepended house exit changes the first edge even
                        // when the outdoor leg needs no path search.
                        citizen.stepDuration = navigation.stepCost(
                            map,
                            original,
                            citizen.path.front(),
                            movementPolicy
                        );
                    }
                }
            }
        } restore{
            c,
            original,
            exitPath,
            successful,
            progress,
            duration,
            map,
            citizens.navigation_,
            citizens.movementPolicy
        };
        // Finish the current physical step before changing direction. Planning
        // from its endpoint preserves interpolation instead of snapping
        // backward.
        if (midStep)
        {
            c.tilePosition = c.path[c.pathIndex];
            exitPath.push_back(c.tilePosition);
        }
        if (const auto* home = map.objectState().completedObject(c.homeId);
            home && home->door && home->footprint.contains(c.tilePosition))
        {
            auto p = c.tilePosition;
            appendRoomPath(
                exitPath,
                p,
                *home->door,
                home->footprint,
                *home->door
            );
            p = outsideDoor(home->footprint, *home->door);
            const auto* tile = map.grid().tile(p);
            if (!tile || tile->terrain == TerrainType::Water ||
                tile->terrain == TerrainType::Mountain ||
                map.objectState().blocksMovement(p))
            {
                return false;
            }
            exitPath.push_back(p);
            c.tilePosition = p;
        }
        routeBudgetLimited_ = false;
        auto& navigation = citizens.navigation_;
        navigation.synchronize(map);
        std::vector<SettlementTilePosition> candidates;
        const auto append = [&](SettlementTilePosition p)
        {
            const auto* targetTerrain = map.grid().tile(f.topLeft);
            if (f.width == 1 && f.height == 1 && targetTerrain &&
                targetTerrain->terrain == TerrainType::Mountain &&
                std::abs(p.x - f.topLeft.x) + std::abs(p.y - f.topLeft.y) != 1)
            {
                return;
            }
            if (navigation
                    .walkable(map, p, routePolicy.avoidBuildingFootprints))
            {
                candidates.push_back(p);
            }
        };
        const auto* targetObject =
            map.objectState().completedObjectAt(f.topLeft);
        std::optional<SettlementTilePosition> doorwayTarget;
        if (inside && targetObject)
        {
            const auto door = workplaceCompound(targetObject->objectTypeId)
                ? std::optional{workplaceRoomDoor(targetObject->footprint)}
                : targetObject->door;
            if (door)
            {
                const SettlementTilePosition entry{
                    std::clamp(door->x, f.topLeft.x, f.topLeft.x + f.width - 1),
                    std::clamp(door->y, f.topLeft.y, f.topLeft.y + f.height - 1)};
                // A storage room is entered through its real door, not the
                // nearest corner behind a wall. This also bounds long hauls.
                if (navigation.walkable(map, entry, routePolicy.avoidBuildingFootprints))
                { doorwayTarget = entry; append(entry); }
            }
        }
        const bool doorAccess = !inside && targetObject &&
                                targetObject->footprint == f &&
                                targetObject->door;
        if (doorAccess)
        {
            append(outsideDoor(f, *targetObject->door));
        }
        if (inside)
        {
            append(
                {std::clamp(
                     c.tilePosition.x,
                     f.topLeft.x,
                     f.topLeft.x + f.width - 1
                 ),
                 std::clamp(
                     c.tilePosition.y,
                     f.topLeft.y,
                     f.topLeft.y + f.height - 1
                 )}
            );
        }
        const bool interiorAvailable = !candidates.empty();
        if (!interiorAvailable && !doorAccess)
        {
            for (int x = f.topLeft.x - 1; x <= f.topLeft.x + f.width; ++x)
            {
                append({x, f.topLeft.y - 1});
                append({x, f.topLeft.y + f.height});
            }
            for (int y = f.topLeft.y; y < f.topLeft.y + f.height; ++y)
            {
                append({f.topLeft.x - 1, y});
                append({f.topLeft.x + f.width, y});
            }
        }
        // Construction explicitly requests an interior tile, never perimeter
        // labor.
        if (inside && f.contains(c.tilePosition) &&
            navigation.walkable(
                map,
                c.tilePosition,
                routePolicy.avoidBuildingFootprints
            ))
        {
            candidates.insert(candidates.begin(), c.tilePosition);
        }
        std::stable_sort(
            candidates.begin(),
            candidates.end(),
            [&](auto a, auto b)
            {
                if (doorwayTarget && ((a == *doorwayTarget) != (b == *doorwayTarget)))
                { return a == *doorwayTarget; }
                return distance(c.tilePosition, {a, 1, 1}) <
                       distance(c.tilePosition, {b, 1, 1});
            }
        );
        const auto maximum = std::min<std::size_t>(candidates.size(), 6);
        for (std::size_t i = 0; i < maximum; ++i)
        {
            const auto goal = candidates[i];
            if (goal == c.tilePosition)
            {
                c.path.clear();
                c.pathIndex = 0;
                c.stepProgress = 0;
                c.destination = goal;
                c.explicitMovement = false;
                successful = true;
                return true;
            }
            if (pathsRemaining_ == 0)
            {
                routeBudgetLimited_ = true;
                return false;
            }
            --pathsRemaining_;
            auto path =
                navigation.findPath(map, c.tilePosition, goal, routePolicy);
            if (path.empty())
            {
                continue;
            }
            c.path = std::move(path);
            c.pathIndex = 0;
            c.stepProgress = 0;
            c.stepDuration = navigation.stepCost(
                map,
                c.tilePosition,
                c.path.front(),
                citizens.movementPolicy
            );
            c.destination = goal;
            c.explicitMovement = true;
            successful = true;
            return true;
        }
        return false;
    }
    bool SettlementActivitySystem::route(
        SettlementMap& map, SettlementCitizenState& citizens,
        SettlementCitizen& c, const SettlementObjectFootprint& f, bool inside
    )
    {
        return planRoute(map, citizens, c, f, inside);
    }

    bool SettlementActivitySystem::route(
        SettlementMap& map, SettlementCitizenState& citizens,
        CitizenRoutePlan& planned, const SettlementObjectFootprint& f, bool inside
    )
    {
        return planRoute(map, citizens, planned, f, inside);
    }

    bool SettlementActivitySystem::boatMealAvailable(
        SettlementMap& map,
        SettlementCitizenState& citizens,
        SettlementCitizen& c,
        double minute
    )
    {
        if (minute < c.nextDecisionMinute)
        {
            return false;
        }
        c.nextDecisionMinute = minute + policy.retryMinutes;
        std::vector<InventoryId> candidates;
        for (const auto& inventory : map.logistics.inventories())
        {
            if (inventory.kind == InventoryKind::Construction ||
                failedRoute(c, inventory.id, c.boatLanding, map, minute))
            {
                continue;
            }
            const auto price = map.commerce.mealPrice(map, inventory);
            if (price < 0 ||
                !map.commerce.canAccessMeal(map, inventory, c, citizens) ||
                (inventory.kind == InventoryKind::Market &&
                 !map.commerce.marketOpen(map, citizens, inventory.objectId)))
            {
                continue;
            }
            for (const auto& goods : inventory.goods)
            {
                if (map.logistics.canEat(goods.resource) &&
                    map.logistics.available(inventory.id, goods.resource) > 0)
                {
                    candidates.push_back(inventory.id);
                    break;
                }
            }
        }
        std::stable_sort(
            candidates.begin(),
            candidates.end(),
            [&](auto a, auto b)
            {
                return distance(
                           c.boatLanding,
                           map.logistics.inventory(a)->footprint
                       ) <
                       distance(
                           c.boatLanding,
                           map.logistics.inventory(b)->footprint
                       );
            }
        );
        for (const auto source : candidates)
        {
            // Land routing is checked from the physical landing, without
            // interrupting the boat or reserving food before arrival.
            CitizenRoutePlan planned(c, c.boatLanding);
            if (route(
                    map,
                    citizens,
                    planned,
                    map.logistics.inventory(source)->footprint,
                    true
                ))
            {
                return true;
            }
            if (routeBudgetLimited_)
            {
                c.nextDecisionMinute = minute + 1;
                return false;
            }
            rememberFailure(c, source, c.boatLanding, map, minute);
        }
        return false;
    }
    bool SettlementActivitySystem::chooseFood(
        SettlementMap& map,
        SettlementCitizenState& citizens,
        SettlementCitizen& c,
        double minute
    )
    {
        if (c.child)
        {
            std::array<SettlementCitizen*, 2> parents{};
            std::size_t parentCount = 0;
            for (const auto id : {c.motherId, c.fatherId})
            {
                auto* parent = citizens.mutableCitizen(id);
                if (parent && !parent->child && parent->health > 0 &&
                    (parentCount == 0 || parents[0] != parent))
                {
                    parents[parentCount++] = parent;
                }
            }
            std::sort(
                parents.begin(),
                parents.begin() + parentCount,
                [&](const auto* a, const auto* b)
                {
                    const auto da = distance(c.tilePosition, {a->tilePosition, 1, 1});
                    const auto db = distance(c.tilePosition, {b->tilePosition, 1, 1});
                    return da != db ? da < db : a->id < b->id;
                }
            );
            for (auto* parent : std::span(parents).first(parentCount))
            {
                if (parent->hunger >= policy.foodSeekThreshold ||
                    parent->task.kind == CitizenTaskKind::FamilyMeal ||
                    parent->task.kind == CitizenTaskKind::Sleep ||
                    parent->task.kind == CitizenTaskKind::Eat ||
                    parent->exitingHomeId || !parent->path.empty() ||
                    (parent->insideHome && !parent->path.empty()))
                {
                    continue;
                }
                CitizenRoutePlan planned(c);
                CitizenRoutePlan parentPlan(*parent);
                const auto* childHome =
                    map.objectState().completedObject(c.homeId);
                const bool bringMealHome =
                    childHome && childHome->door &&
                    !inChildNeighborhood(map, c, parent->tilePosition);
                const auto* home =
                    parent->insideHome
                        ? map.objectState().completedObject(parent->homeId)
                        : nullptr;
                if (home && c.homeId != parent->homeId)
                {
                    continue;
                }
                const bool togetherAtHome = home && c.insideHome;
                if (bringMealHome)
                {
                    const auto meeting =
                        outsideDoor(childHome->footprint, *childHome->door);
                    if (!route(
                            map,
                            citizens,
                            parentPlan,
                            {meeting, 1, 1},
                            true
                        ) ||
                        !route(map, citizens, planned, {meeting, 1, 1}, true))
                    {
                        continue;
                    }
                }
                else if (
                    !togetherAtHome &&
                    !route(
                        map,
                        citizens,
                        planned,
                        home
                            ? home->footprint
                            : SettlementObjectFootprint{parent->tilePosition, 1, 1},
                        !home
                    )
                )
                {
                    if (routeBudgetLimited_)
                    {
                        return false;
                    }
                    continue;
                }
                finish(map, c, minute);
                finish(map, *parent, minute);
                if (!togetherAtHome)
                {
                    planned.applyTo(c);
                }
                else
                {
                    c.destination = c.tilePosition;
                }
                if (bringMealHome)
                {
                    parentPlan.applyTo(*parent);
                }
                else
                {
                    parent->destination = parent->tilePosition;
                }
                c.task.kind = parent->task.kind = CitizenTaskKind::FamilyMeal;
                c.task.partner = parent->id;
                parent->task.partner = c.id;
                c.task.endMinute = parent->task.endMinute =
                    minute + policy.familyMealTimeoutMinutes;
                c.activity = CitizenActivity::SeekingFood;
                return true;
            }
            // Living parents provide food, never access to their wallet.
            // Orphans may use free public supplies as a survival fallback.
            if (parentCount != 0)
            {
                return false;
            }
        }
        struct Food
        {
            InventoryId id;
            std::string resource;
            int distance;
            int preference = 0;
        };
        std::vector<Food> foods;
        for (const auto& inventory : map.logistics.inventories())
        {
            if (inventory.kind == InventoryKind::Construction)
            {
                continue;
            }
            const bool market = inventory.kind == InventoryKind::Market;
            const auto price = map.commerce.mealPrice(map, inventory);
            if (price < 0 ||
                !map.commerce.canAccessMeal(map, inventory, c, citizens) ||
                (market &&
                 !map.commerce.marketOpen(map, citizens, inventory.objectId)))
            {
                continue;
            }
            for (const auto& goods : inventory.goods)
            {
                const auto* definition =
                    SettlementResourceCatalog::definition(goods.resource);
                if (definition && map.logistics.mayExport(map.objectState(),inventory,goods.resource) && map.logistics.canEat(goods.resource) &&
                    map.logistics.available(inventory.id, goods.resource) > 0)
                {
                    foods.push_back(
                        {inventory.id,
                         goods.resource,
                         distance(c.tilePosition, inventory.footprint),
                         SettlementCommerce::sourcePreference(inventory.kind)}
                    );
                }
            }
        }
        std::stable_sort(
            foods.begin(),
            foods.end(),
            [](const auto& a, const auto& b)
            {
                return a.preference != b.preference ? a.preference < b.preference
                                                    : a.distance < b.distance;
            }
        );
        // Nearest sources first; unlike ordinary hauling, food searches have no
        // radius cutoff.
        for (const auto& food : foods)
        {
            if (failedRoute(c, food.id, c.tilePosition, map, minute))
            {
                continue;
            }
            const auto* inventory = map.logistics.inventory(food.id);
            if (!inventory)
            {
                continue;
            }
            CitizenRoutePlan planned(c);
            if (!route(map, citizens, planned, inventory->footprint, true))
            {
                if (routeBudgetLimited_)
                {
                    break;
                }
                rememberFailure(c, food.id, c.tilePosition, map, minute);
                continue;
            }
            if (c.breakUntil > 0 &&
                !breakTripFits(map, citizens, c, planned, minute, 1))
            {
                continue;
            }
            finish(map, c, minute);
            if (!map.logistics.reserve(c.id, food.id, {}, food.resource, 1))
            {
                return false;
            }
            planned.applyTo(c);
            c.task.kind = CitizenTaskKind::Eat;
            c.task.source = food.id;
            c.task.startedMinute = minute;
            c.activity = CitizenActivity::SeekingFood;
            return true;
        }
        return false;
    }
    bool SettlementActivitySystem::beginHaul(
        SettlementMap& map,
        SettlementCitizenState& citizens,
        SettlementCitizen& c,
        InventoryId source,
        InventoryId destination,
        std::string_view resource,
        int amount,
        double minute
    )
    {
        const auto* sourceInventory = map.logistics.inventory(source);
        const auto* targetInventory = map.logistics.inventory(destination);
        if (!sourceInventory || !targetInventory || amount <= 0)
        {
            return false;
        }
        if (!map.logistics.mayExport(map.objectState(),*sourceInventory,resource) ||
            !map.logistics.importsMaySupply(*sourceInventory, targetInventory->kind))
        { return false; }
        if (targetInventory->kind == InventoryKind::TradeDepot)
        {
            if (!DepotCollection::source(map, *sourceInventory, resource)) { return false; }
            amount = std::min(amount, DepotCollection::needed(map, *targetInventory, resource));
        }
        amount = map.commerce.affordableTradeUnits(
            *sourceInventory,
            *targetInventory,
            amount,
            &citizens,
            resource, !c.workplaceId
        );
        if (amount <= 0)
        {
            return false;
        }
        const auto sourceFootprint = sourceInventory->footprint;
        const auto destinationFootprint = targetInventory->footprint;
        if (failedRoute(c, source, c.tilePosition, map, minute))
        {
            return false;
        }
        CitizenRoutePlan planned(c);
        if (!route(map, citizens, planned, sourceFootprint, true))
        {
            if (!routeBudgetLimited_)
            {
                rememberFailure(c, source, c.tilePosition, map, minute);
            }
            return false;
        }
        // Prove both travel legs before reserving goods or destination space.
        auto delivery = planned.fromPosition(planned.destination);
        if (failedRoute(c, destination, delivery.tilePosition, map, minute))
        {
            return false;
        }
        if (!route(map, citizens, delivery, destinationFootprint, true))
        {
            if (!routeBudgetLimited_)
            {
                rememberFailure(
                    c,
                    destination,
                    delivery.tilePosition,
                    map,
                    minute
                );
            }
            return false;
        }
        if (!map.logistics.reserve(c.id, source, destination, resource, amount))
        {
            return false;
        }
        planned.applyTo(c);
        c.task = {};
        c.task.kind = CitizenTaskKind::Haul;
        c.task.treasuryPurchase = !c.workplaceId;
        c.haulDeliveryPath = std::move(delivery.path);
        c.haulDeliveryTarget = delivery.destination;
        c.haulDeliveryTopology = map.objectState().navigationVersion();
        c.task.source = source;
        c.task.destination = destination;
        c.task.startedMinute = minute;
        c.activity = CitizenActivity::Hauling;
        return true;
    }
    bool SettlementActivitySystem::chooseDepotHaul(
        SettlementMap& map, SettlementCitizenState& citizens,
        SettlementCitizen& citizen, double minute, InventoryId destination)
    {
        const auto* target = map.logistics.inventory(destination);
        if (!target || target->kind != InventoryKind::TradeDepot) { return false; }
        struct Source { InventoryId id; int rank, distance; };
        // Advance before searching: an inaccessible order must not consume
        // every worker's route budget and starve all the other goods.
        const auto orderCount = map.trade.orders.size();
        for (std::size_t checked = 0; checked < orderCount; ++checked)
        {
            auto& order = map.trade.orders[map.trade.collectionCursor++ % orderCount];
            if (!order.enabled || !order.collectionAuthorized ||
                order.depot != target->objectId || order.direction != TradeDirection::Export)
            { continue; }
            const int needed = DepotCollection::needed(map, *target, order.resource);
            if (!needed) { continue; }
            const int space = map.logistics.receivable(destination, order.resource);
            if (space <= 0)
            {
                DepotCollection::report(map, target->objectId, order.resource,
                                        DepotCollectionIssue::NoSpace);
                continue;
            }
            std::vector<Source> sources;
            bool hasStock = false;
            for (const auto& inventory : map.logistics.inventories())
            {
                if (!DepotCollection::source(map, inventory, order.resource) ||
                    map.logistics.available(inventory.id, order.resource) <= 0)
                { continue; }
                hasStock = true;
                if (map.commerce.affordableTradeUnits(inventory, *target, 1,
                                                     &citizens, order.resource) <= 0)
                { continue; }
                sources.push_back({inventory.id,
                    inventory.kind == InventoryKind::Stockpile ? 0 : 1,
                    distance(citizen.tilePosition, inventory.footprint)});
            }
            std::stable_sort(sources.begin(), sources.end(), [](const auto& a, const auto& b)
            { return a.rank != b.rank ? a.rank < b.rank : a.distance < b.distance; });
            for (const auto& source : sources)
            {
                const int units = std::min({needed, policy.carryingCapacity,
                    map.logistics.available(source.id, order.resource),
                    space});
                if (beginHaul(map, citizens, citizen, source.id, destination,
                              order.resource, units, minute))
                {
                    DepotCollection::report(map, target->objectId, order.resource,
                                            DepotCollectionIssue::None);
                    return true;
                }
                if (routeBudgetLimited_ || pathsRemaining_ == 0) { return false; }
            }
            DepotCollection::report(map, target->objectId, order.resource,
                !sources.empty() ? DepotCollectionIssue::NoPath :
                hasStock ? DepotCollectionIssue::NoMoney : DepotCollectionIssue::NoStock);
        }
        return false;
    }

    bool SettlementActivitySystem::chooseHaul(
        SettlementMap& map,
        SettlementCitizenState& citizens,
        SettlementCitizen& c,
        double minute,
        InventoryId assignedDestination
    )
    {
        struct Opportunity
        {
            InventoryId source, destination;
            std::string resource;
            int amount, score;
        };
        std::vector<Opportunity> opportunities;
        const auto* assigned = map.logistics.inventory(assignedDestination);
        if (assigned && assigned->kind == InventoryKind::TradeDepot)
        { return chooseDepotHaul(map, citizens, c, minute, assignedDestination); }
        const bool market = assigned && assigned->kind == InventoryKind::Market;
        const bool homeDelivery =
            assigned && assigned->kind == InventoryKind::Home;
        if (!market)
        {
            std::unordered_set<SettlementObjectId, StrongIdHash> occupiedHomes;
            for (const auto& resident : citizens.citizens())
            {
                if (resident.homeId && resident.health > 0)
                {
                    occupiedHomes.insert(resident.homeId);
                }
            }
            for (const auto& home : map.logistics.inventories())
            {
                if (home.kind != InventoryKind::Home ||
                    (homeDelivery && home.id != assignedDestination) ||
                    home.amount(SettlementResourceTypes::Lumber) > 2 ||
                    !occupiedHomes.contains(home.objectId))
                {
                    continue;
                }
                for (const auto& source : map.logistics.inventories())
                {
                    if (source.kind == InventoryKind::Home ||
                        source.kind == InventoryKind::Construction ||
                        !map.logistics.mayExport(map.objectState(),source,SettlementResourceTypes::Lumber) ||
                        (source.kind != InventoryKind::TradeImports &&
                         (distance(c.tilePosition, source.footprint) > policy.stockpile.collectionRadius ||
                          distance(source.footprint.topLeft, home.footprint) > policy.stockpile.collectionRadius)))
                    {
                        continue;
                    }
                    const int amount = std::min(
                        {policy.carryingCapacity,
                         map.commerce.affordableTradeUnits(
                             source,
                             home,
                             policy.carryingCapacity,
                             &citizens,
                             SettlementResourceTypes::Lumber
                         ),
                         map.logistics.available(
                             source.id,
                             SettlementResourceTypes::Lumber
                         ),
                         map.logistics.receivable(
                             home.id,
                             SettlementResourceTypes::Lumber
                         )}
                    );
                    if (amount > 0)
                    {
                        opportunities.push_back(
                            {source.id,
                             home.id,
                             std::string(SettlementResourceTypes::Lumber),
                             amount,
                             -10000 +
                                 SettlementCommerce::sourcePreference(source.kind)*100000 +
                                 distance(c.tilePosition, source.footprint) +
                                 distance(
                                     source.footprint.topLeft,
                                     home.footprint
                                 )}
                        );
                    }
                }
            }
        }
        for (const auto& source : map.logistics.inventories())
        {
            if (homeDelivery)
            {
                break;
            }
            const bool wholesaleSource =
                source.kind == InventoryKind::Stockpile ||
                source.kind == InventoryKind::Workplace ||
                source.kind == InventoryKind::Groundpile ||
                source.kind == InventoryKind::TradeImports;
            if ((market
                     ? !wholesaleSource
                     : (source.kind != InventoryKind::Groundpile &&
                        source.kind != InventoryKind::Workplace &&
                        source.kind != InventoryKind::TradeImports)) ||
                source.used() <= 0)
            {
                continue;
            }
            if (!assignedDestination && source.kind != InventoryKind::TradeImports &&
                distance(c.tilePosition, source.footprint) >
                    policy.localSearchRadius)
            {
                continue;
            }
            bool covered = false;
            if (!assignedDestination && policy.isWorkTime(minute) &&
                minute - source.createdMinute <
                    policy.stockpile.employeePreferenceMinutes)
            {
                for (const auto& w : map.employment().workplaces())
                {
                    if (w.operational &&
                        w.objectTypeId == SettlementObjectTypes::Stockpile &&
                        std::any_of(
                            citizens.citizens().begin(),
                            citizens.citizens().end(),
                            [&](const auto& employee)
                            {
                                return employee.workplaceId == w.id &&
                                       employee.health > 0 &&
                                       (employee.task.kind ==
                                            CitizenTaskKind::Work ||
                                        employee.task.kind ==
                                            CitizenTaskKind::Haul);
                            }
                        ) &&
                        distance(source.footprint.topLeft, w.footprint) <=
                            policy.stockpile.collectionRadius &&
                        map.logistics.freeSpace(
                            map.logistics.forObject(w.objectId)
                        ) > 0)
                    {
                        covered = true;
                        break;
                    }
                }
            }
            if (covered)
            {
                continue;
            }
            for (const auto& destination : map.logistics.inventories())
            {
                if (destination.id == source.id ||
                    !(publicStorage(destination) ||
                      (market &&
                       destination.id == assignedDestination)) ||
                    (assignedDestination &&
                     destination.id != assignedDestination) ||
                    (source.kind != InventoryKind::TradeImports &&
                     distance(source.footprint.topLeft, destination.footprint) >
                        policy.stockpile.collectionRadius))
                {
                    continue;
                }
                for (const auto& goods : source.goods)
                {
                    if (!map.logistics.mayExport(
                            map.objectState(),
                            source,
                            goods.resource
                        ))
                    {
                        continue;
                    }
                    const auto* resource =
                        SettlementResourceCatalog::definition(goods.resource);
                    if (market && !resource)
                    {
                        continue;
                    }
                    const int affordable = map.commerce.affordableTradeUnits(
                        source,
                        destination,
                        policy.carryingCapacity,
                        &citizens,
                        goods.resource, !c.workplaceId
                    );
                    const int target =
                        market
                            ? (goods.resource == SettlementResourceTypes::Lumber
                                   ? std::max(2, destination.capacity / 4)
                               : resource && resource->edible
                                   ? std::max(4, destination.capacity * 3 / 4)
                                   : std::max(2, destination.capacity / 12))
                            : destination.capacity;
                    int held = destination.amount(goods.resource);
                    if (market && resource && resource->edible)
                    {
                        held = 0;
                        for (const auto& item : destination.goods)
                        {
                            if (const auto* food =
                                    SettlementResourceCatalog::definition(
                                        item.resource
                                    );
                                food && food->edible)
                            {
                                held += item.amount;
                            }
                        }
                    }
                    const int incoming = std::max(0, destination.capacity - destination.used() -
                                         map.logistics.freeSpace(destination.id));
                    const int needed = std::max(0, target - held - incoming);
                    const int amount = std::min(
                        {policy.carryingCapacity,
                         needed,
                         affordable,
                         map.logistics.available(source.id, goods.resource),
                         map.logistics
                             .receivable(destination.id, goods.resource)}
                    );
                    const bool urgent=(resource && resource->edible) || source.used()>=source.capacity*3/4;
                    if(!market && !urgent && amount<policy.carryingCapacity && minute-goods.batchMinute<45) continue;
                    if (amount > 0)
                    {
                        opportunities.push_back(
                            {source.id,
                             destination.id,
                             goods.resource,
                             amount,
                             -amount*8 - (urgent?100:0) + distance(c.tilePosition, source.footprint) +
                                 distance(
                                     source.footprint.topLeft,
                                     destination.footprint
                                 )}
                        );
                    }
                }
            }
        }
        std::stable_sort(
            opportunities.begin(),
            opportunities.end(),
            [](const auto& a, const auto& b) { return a.score < b.score; }
        );
        for (const auto& opportunity : opportunities)
        {
            if (beginHaul(
                    map,
                    citizens,
                    c,
                    opportunity.source,
                    opportunity.destination,
                    opportunity.resource,
                    opportunity.amount,
                    minute
                ))
            {
                return true;
            }
            if (pathsRemaining_ == 0)
            {
                break;
            }
        }
        return false;
    }
    bool SettlementActivitySystem::chooseConstruction(
        SettlementMap& map,
        SettlementCitizenState& citizens,
        SettlementCitizen& c,
        double minute
    )
    {
        jobBoard_.refreshConstructionBoard(map);
        struct Choice
        {
            ConstructionSiteId id;
            int score;
        };
        std::vector<Choice> choices;
        std::unordered_set<ConstructionSiteId, StrongIdHash> seen;
        const auto append = [&](ConstructionSiteId id)
        {
            if (!seen.insert(id).second)
            {
                return;
            }
            if (const auto* site = map.objectState().constructionSite(id))
            {
                choices.push_back(
                    {id, distance(c.tilePosition, site->footprint)}
                );
            }
        };
        // A local job board bounds each worker's query independently of all
        // queued tiles.
        constexpr int chunk = 32;
        std::size_t examined = 0;
        for (int dy = -1; dy <= 1 && examined < 384; ++dy)
        {
            for (int dx = -1; dx <= 1 && examined < 384; ++dx)
            {
                const int x = c.tilePosition.x / chunk + dx,
                          y = c.tilePosition.y / chunk + dy;
                if (x < 0 || y < 0)
                {
                    continue;
                }
                const auto bucket = jobBoard_.constructionBucket(
                    (std::uint64_t(y) << 32) | std::uint32_t(x)
                );
                for (std::size_t i = 0;
                     i < std::min<std::size_t>(bucket.size(), 48);
                     ++i, ++examined)
                {
                    append(
                        bucket[(c.constructionSearchCursor + i) % bucket.size()]
                    );
                }
            }
        }
        const auto& all = map.objectState().constructionSites();
        for (std::size_t i = 0; i < std::min<std::size_t>(all.size(), 64); ++i)
        {
            append(all[(c.constructionSearchCursor + i) % all.size()].id);
        }
        c.constructionSearchCursor += 64;
        std::stable_sort(
            choices.begin(),
            choices.end(),
            [](const auto& a, const auto& b) { return a.score < b.score; }
        );
        std::size_t clearingBudget = 512;
        for (const auto& choice : choices)
        {
            const auto id = choice.id;
            const auto& site = *map.objectState().constructionSite(id);
            const auto routeKey =
                jobRouteKey(id.value(), site.footprint.topLeft);
            if (routeFailed(
                    c,
                    RouteFailureDomain::Construction,
                    routeKey,
                    c.tilePosition,
                    map,
                    minute
                ))
            {
                continue;
            }
            if (map.naturalFeatures().countIn(site.footprint) > 0)
            {
                bool routeFailure = false;
                while (clearingBudget > 0)
                {
                    const auto next = jobBoard_.nextClearingTarget(
                        map,
                        id,
                        site.footprint,
                        clearingBudget
                    );
                    if (!next)
                    {
                        break;
                    }
                    const auto tile = *next;
                    CitizenTask clearing;
                    clearing.kind = CitizenTaskKind::Gather;
                    clearing.site = id;
                    clearing.workTile = tile;
                    clearing.startedMinute = minute;
                    if (!jobBoard_.claimed(clearing))
                    {
                        if (route(map, citizens, c, {tile, 1, 1}, true))
                        {
                            c.task = clearing;
                            jobBoard_.claim(c.task);
                            c.activity = CitizenActivity::AssignedToCommand;
                            return true;
                        }
                        if (routeBudgetLimited_)
                        {
                            return false;
                        }
                        rememberRouteFailure(
                            c,
                            RouteFailureDomain::Construction,
                            routeKey,
                            c.tilePosition,
                            map,
                            minute
                        );
                        routeFailure = true;
                        if (!pathsRemaining_)
                        {
                            return false;
                        }
                        break;
                    }
                    if (!pathsRemaining_)
                    {
                        return false;
                    }
                }
                if (routeFailure)
                {
                    continue;
                }
                continue;
            }
            const auto destination = map.logistics.forSite(id);
            const auto* inventory = map.logistics.inventory(destination);
            if (!inventory && !site.resourceDeliveries.empty())
            {
                continue;
            }
            bool ready = true;
            for (const auto& cost : site.resourceDeliveries)
            {
                if (inventory->amount(cost.resourceId) >=
                    int(cost.requiredAmount))
                {
                    continue;
                }
                ready = false;
                struct Source
                {
                    InventoryId id;
                    int score;
                };
                std::vector<Source> sources;
                for (const auto& source : map.logistics.inventories())
                {
                    if (source.kind == InventoryKind::Construction ||
                        source.kind == InventoryKind::Home ||
                        map.logistics.available(source.id, cost.resourceId) <=
                            0)
                    {
                        continue;
                    }
                    sources.push_back(
                        {source.id,
                         distance(c.tilePosition, source.footprint) +
                             (SettlementCommerce::sourcePreference(source.kind) * 100000)}
                    );
                }
                std::stable_sort(
                    sources.begin(),
                    sources.end(),
                    [](const auto& a, const auto& b)
                    { return a.score < b.score; }
                );
                for (const auto& source : sources)
                {
                    const int amount = std::min(
                        {policy.carryingCapacity,
                         map.logistics.available(source.id, cost.resourceId),
                         map.logistics.receivable(destination, cost.resourceId),
                         int(cost.requiredAmount) -
                             inventory->amount(cost.resourceId)}
                    );
                    if (beginHaul(
                            map,
                            citizens,
                            c,
                            source.id,
                            destination,
                            cost.resourceId,
                            amount,
                            minute
                        ))
                    {
                        return true;
                    }
                    if (pathsRemaining_ == 0)
                    {
                        return false;
                    }
                }
            }
            if (!ready)
            {
                continue;
            }
            CitizenTask task;
            task.kind = CitizenTaskKind::Build;
            task.site = id;
            // Construction is shared work: each attending citizen contributes
            // labor to the site rather than reserving the whole building.
            SettlementTilePosition goal{
                std::clamp(
                    c.tilePosition.x,
                    site.footprint.topLeft.x,
                    site.footprint.topLeft.x + site.footprint.width - 1
                ),
                std::clamp(
                    c.tilePosition.y,
                    site.footprint.topLeft.y,
                    site.footprint.topLeft.y + site.footprint.height - 1
                )
            };
            if (site.objectTypeId == SettlementObjectTypes::Road)
            {
                // A compact road run has one current tile of shared progress.
                // Join that tile so labor never transfers between distant
                // tiles.
                for (const auto& worker : citizens.citizens())
                {
                    if (worker.task.kind == CitizenTaskKind::Build &&
                        worker.task.site == id)
                    {
                        goal = worker.task.workTile;
                        break;
                    }
                }
            }
            CitizenRoutePlan planned(c);
            if (!route(map, citizens, planned, {goal, 1, 1}, true) ||
                !site.footprint.contains(planned.destination))
            {
                if (!routeBudgetLimited_)
                {
                    rememberRouteFailure(
                        c,
                        RouteFailureDomain::Construction,
                        routeKey,
                        c.tilePosition,
                        map,
                        minute
                    );
                }
                if (!pathsRemaining_)
                {
                    return false;
                }
                continue;
            }
            task.workTile = planned.destination;
            task.startedMinute = minute;
            planned.applyTo(c);
            c.task = task;
            c.activity = CitizenActivity::Constructing;
            return true;
        }
        return false;
    }
    bool SettlementActivitySystem::chooseCommand(
        SettlementMap& map,
        SettlementCitizenState& citizens,
        SettlementCitizen& c,
        double minute
    )
    {
        struct Choice
        {
            std::size_t index;
            int score;
        };
        std::vector<Choice> choices;
        std::unordered_set<std::size_t> seen;
        const auto append = [&](std::size_t index)
        {
            if (!seen.insert(index).second)
            {
                return;
            }
            const auto& job = jobBoard_.command(index);
            if (!map.commandState().contains(
                    map,
                    job.command,
                    job.footprint.topLeft,
                    job.object,
                    job.site
                ))
            {
                return;
            }
            if (!jobBoard_.claimed(job.task()))
            {
                choices.push_back(
                    {index, distance(c.tilePosition, job.footprint)}
                );
            }
        };
        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                const int x = c.tilePosition.x / 32 + dx,
                          y = c.tilePosition.y / 32 + dy;
                if (x < 0 || y < 0)
                {
                    continue;
                }
                const auto bucket = jobBoard_.commandBucket(
                    (std::uint64_t(y) << 32) | std::uint32_t(x)
                );
                for (std::size_t i = 0;
                     i < std::min<std::size_t>(bucket.size(), 32);
                     ++i)
                {
                    append(bucket[(c.commandSearchCursor + i) % bucket.size()]);
                }
            }
        }
        // The global fallback deliberately has no distance cutoff. Distance is
        // a preference only; an accessible player command remains eligible
        // anywhere on the settlement map.
        for (std::size_t i = 0;
             i <
             std::min<std::size_t>(jobBoard_.availableCommands().size(), 64);
             ++i)
        {
            append(jobBoard_.availableCommands()
                       [(c.commandSearchCursor + i) %
                        jobBoard_.availableCommands().size()]);
        }
        c.commandSearchCursor += 64;
        std::stable_sort(
            choices.begin(),
            choices.end(),
            [](const auto& a, const auto& b) { return a.score < b.score; }
        );
        for (const auto& choice : choices)
        {
            const auto& job = jobBoard_.command(choice.index);
            const auto key =
                jobRouteKey(job.command.value(), job.footprint.topLeft);
            if (routeFailed(
                    c,
                    RouteFailureDomain::Command,
                    key,
                    c.tilePosition,
                    map,
                    minute
                ))
            {
                continue;
            }
            if (route(map, citizens, c, job.footprint, true))
            {
                c.task = job.task();
                jobBoard_.claim(c.task);
                c.task.startedMinute = minute;
                c.assignedCommandId = c.task.command;
                c.activity = CitizenActivity::AssignedToCommand;
                return true;
            }
            if (!routeBudgetLimited_)
            {
                rememberRouteFailure(
                    c,
                    RouteFailureDomain::Command,
                    key,
                    c.tilePosition,
                    map,
                    minute
                );
            }
            if (!pathsRemaining_)
            {
                break;
            }
        }
        return false;
    }
} // namespace Paladin

namespace Paladin
{
    bool SettlementActivitySystem::chooseWork(
        SettlementMap& map,
        SettlementCitizenState& citizens,
        SettlementCitizen& c,
        double minute
    )
    {
        const auto* job = map.employment().workplace(c.workplaceId);
        if (!job || !job->operational)
        {
            return false;
        }
        const auto workplace = *job;
        if (const auto* mine = miningJob(workplace.objectTypeId))
        {
            const auto* object =
                map.objectState().completedObject(workplace.objectId);
            if (!object)
            {
                return false;
            }
            auto& site = map.mining.prepare(map.grid(), *object);
            if (site.exhausted || site.cursor >= site.columns.size())
            {
                return false;
            }
            const auto& f = object->footprint;
            const bool tunnel =
                mine->tunnels && map.mining.depth(*object) >= .98;
            for (std::size_t attempt = 0; attempt < 16; ++attempt)
            {
                // Extraction still debits the canonical ore columns, while
                // workers stand at the cut face, never inside the service hut.
                auto target = miningWorkTile(f.topLeft, f.width, f.height,
                                             c.id.value() + attempt);
                if (tunnel)
                {
                    target = quarryEntrance(
                        f.topLeft,
                        f.width,
                        c.id.value() + attempt,
                        f.height
                    );
                }
                if (!tunnel &&
                    std::any_of(
                        citizens.citizens().begin(),
                        citizens.citizens().end(),
                        [&](const auto& other)
                        {
                            return other.id != c.id &&
                                   other.task.kind == CitizenTaskKind::Work &&
                                   other.task.object == workplace.objectId &&
                                   other.task.workTile == target;
                        }
                    ))
                {
                    continue;
                }
                if (!route(map, citizens, c, {target, 1, 1}, true) ||
                    c.destination != target)
                {
                    if (!pathsRemaining_)
                    {
                        break;
                    }
                    continue;
                }
                c.task = {};
                c.task.kind = CitizenTaskKind::Work;
                c.task.object = workplace.objectId;
                c.task.workTile = target;
                c.task.startedMinute = minute;
                c.activity = CitizenActivity::TravelingToWork;
                return true;
            }
            return false;
        }
        if (workplace.objectTypeId == SettlementObjectTypes::LoggingGrounds)
        {
            int slot = 0;
            for (const auto& other : citizens.citizens())
            {
                if (other.id == c.id)
                {
                    break;
                }
                if (other.workplaceId == c.workplaceId)
                {
                    ++slot;
                }
            }
            const auto& f = workplace.footprint;
            const int columns = std::max(1, (f.width + 2) / 3);
            const SettlementTilePosition target{
                f.topLeft.x + std::min(f.width - 1, slot % columns * 3 + 1),
                f.topLeft.y + std::min(f.height - 1, slot / columns * 3 + 2)
            };
            if (!route(map, citizens, c, {target, 1, 1}, true) ||
                c.destination != target)
            {
                return false;
            }
            c.task = {};
            c.task.kind = CitizenTaskKind::Work;
            c.task.object = workplace.objectId;
            c.task.workTile = target;
            c.task.startedMinute = minute;
            c.activity = CitizenActivity::TravelingToWork;
            return true;
        }
        if (workplace.objectTypeId == SettlementObjectTypes::Pastureland)
        {
            return choosePastureWork(map, citizens, c, minute);
        }
        if (workplace.objectTypeId == SettlementObjectTypes::Market)
        {
            int slot = 0;
            for (const auto& other : citizens.citizens())
            {
                if (other.id == c.id)
                {
                    break;
                }
                if (other.workplaceId == c.workplaceId)
                {
                    ++slot;
                }
            }
            const auto& f = workplace.footprint;
            const auto target =
                marketStallTile(f.topLeft, f.width, f.height, slot);
            if (!route(map, citizens, c, {target, 1, 1}, true))
            {
                return false;
            }
            c.task = {};
            c.task.kind = CitizenTaskKind::Work;
            c.task.object = workplace.objectId;
            c.task.workTile = target;
            c.task.startedMinute = minute;
            c.activity = CitizenActivity::TravelingToWork;
            return true;
        }
        if (workplace.objectTypeId == SettlementObjectTypes::FishingGrounds)
        {
            const auto* object =
                map.objectState().completedObject(workplace.objectId);
            if (!object)
            {
                return false;
            }
            std::vector<SettlementTilePosition> occupied;
            for (const auto& other : citizens.citizens())
            {
                if (other.id != c.id &&
                    other.task.kind == CitizenTaskKind::Work &&
                    other.task.object == object->id)
                {
                    occupied.push_back(other.task.target);
                }
            }
            auto voyage = map.fishingBoats.plan(
                map,
                *object,
                c.id.value() ^ ++c.choiceSequence,
                occupied
            );
            if (voyage.route.empty())
            {
                return false;
            }
            CitizenRoutePlan planned(c);
            if (!route(
                    map,
                    citizens,
                    planned,
                    {voyage.launch.land, 1, 1},
                    true
                ) ||
                planned.destination != voyage.launch.land)
            {
                return false;
            }
            planned.applyTo(c);
            c.task = {};
            c.task.kind = CitizenTaskKind::Work;
            c.task.object = workplace.objectId;
            c.task.workTile = voyage.launch.land;
            c.task.target = voyage.route.back();
            c.task.startedMinute = minute;
            c.boatRoute = std::move(voyage.route);
            c.activity = CitizenActivity::TravelingToWork;
            return true;
        }
        const auto* definition=SettlementObjectCatalog::definition(workplace.objectTypeId);
        if(definition && definition->wallThickness>0)
        {
            const auto room=buildingInterior(workplace.footprint,workplace.objectTypeId);
            const int slot=int(c.id.value()%std::max(1,room.width*room.height));
            const SettlementTilePosition station{room.topLeft.x+slot%room.width,room.topLeft.y+slot/room.width};
            if(!route(map,citizens,c,{station,1,1},true)) return false;
        }
        else         if (!route(map, citizens, c, workplace.footprint, true))
        {
            return false;
        }
        c.task = {};
        c.task.kind = CitizenTaskKind::Work;
        c.task.object = workplace.objectId;
        c.task.startedMinute = minute;
        c.activity = CitizenActivity::TravelingToWork;
        return true;
    }
} // namespace Paladin

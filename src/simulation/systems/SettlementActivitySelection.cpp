#include "simulation/systems/SettlementActivitySystem.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Paladin
{
namespace
{
int distance(SettlementTilePosition p, const SettlementObjectFootprint& f)
{
    return std::abs(
               p.x - std::clamp(p.x, f.topLeft.x, f.topLeft.x + f.width - 1)
           ) +
           std::abs(
               p.y - std::clamp(p.y, f.topLeft.y, f.topLeft.y + f.height - 1)
           );
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
            return failure.source == target && failure.untilMinute > minute &&
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
                   f.topologyVersion != map.objectState().navigationVersion();
        }
    );
    if (citizen.routeFailures.size() >= 64)
    {
        citizen.routeFailures.erase(citizen.routeFailures.begin());
    }
    citizen.routeFailures.push_back(
        {target, origin, map.objectState().navigationVersion(), minute + 30}
    );
}
void copyRoute(SettlementCitizen& to, SettlementCitizen& from)
{
    to.path = std::move(from.path);
    to.pathIndex = 0;
    to.stepProgress = 0;
    to.stepDuration = from.stepDuration;
    to.destination = from.destination;
    to.explicitMovement = !to.path.empty();
}
bool publicStorage(const SettlementInventory& inventory)
{
    return inventory.kind == InventoryKind::Keep ||
           inventory.kind == InventoryKind::Stockpile;
}
} // namespace
bool SettlementActivitySystem::route(
    SettlementMap& map,
    SettlementCitizenState& citizens,
    SettlementCitizen& c,
    const SettlementObjectFootprint& f,
    bool inside
)
{
    routeBudgetLimited_ = false;
    auto& navigation = citizens.navigation_;
    navigation.synchronize(map);
    std::vector<SettlementTilePosition> candidates;
    const auto append = [&](SettlementTilePosition p)
    {
        if (navigation.walkable(map, p))
        {
            candidates.push_back(p);
        }
    };
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
    if (!interiorAvailable)
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
    // Construction explicitly requests an interior tile, never perimeter labor.
    if (inside && f.contains(c.tilePosition) &&
        navigation.walkable(map, c.tilePosition))
    {
        candidates.insert(candidates.begin(), c.tilePosition);
    }
    std::stable_sort(
        candidates.begin(),
        candidates.end(),
        [&](auto a, auto b)
        {
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
            return true;
        }
        if (pathsRemaining_ == 0)
        {
            routeBudgetLimited_ = true;
            return false;
        }
        --pathsRemaining_;
        auto path =
            navigation
                .findPath(map, c.tilePosition, goal, citizens.movementPolicy);
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
        return true;
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
    struct Food
    {
        InventoryId id;
        std::string resource;
        int distance;
    };
    std::vector<Food> foods;
    for (const auto& inventory : map.logistics.inventories())
    {
        if (inventory.kind == InventoryKind::Construction)
        {
            continue;
        }
        for (const auto& goods : inventory.goods)
        {
            const auto* definition =
                SettlementResourceCatalog::definition(goods.resource);
            if (definition && definition->edible &&
                map.logistics.available(inventory.id, goods.resource) > 0)
            {
                foods.push_back(
                    {inventory.id,
                     goods.resource,
                     distance(c.tilePosition, inventory.footprint)}
                );
            }
        }
    }
    std::stable_sort(
        foods.begin(),
        foods.end(),
        [](const auto& a, const auto& b) { return a.distance < b.distance; }
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
        auto planned = c;
        if (!route(map, citizens, planned, inventory->footprint, true))
        {
            if (routeBudgetLimited_)
            {
                break;
            }
            rememberFailure(c, food.id, c.tilePosition, map, minute);
            continue;
        }
        finish(map, c, minute);
        if (!map.logistics.reserve(c.id, food.id, {}, food.resource, 1))
        {
            return false;
        }
        copyRoute(c, planned);
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
    const auto sourceFootprint = sourceInventory->footprint;
    const auto destinationFootprint = targetInventory->footprint;
    if (failedRoute(c, source, c.tilePosition, map, minute))
    {
        return false;
    }
    auto planned = c;
    if (!route(map, citizens, planned, sourceFootprint, true))
    {
        if (!routeBudgetLimited_)
        {
            rememberFailure(c, source, c.tilePosition, map, minute);
        }
        return false;
    }
    // Prove both travel legs before reserving goods or destination space.
    auto delivery = planned;
    delivery.tilePosition = planned.destination;
    if (failedRoute(c, destination, delivery.tilePosition, map, minute))
    {
        return false;
    }
    if (!route(map, citizens, delivery, destinationFootprint, true))
    {
        if (!routeBudgetLimited_)
        {
            rememberFailure(c, destination, delivery.tilePosition, map, minute);
        }
        return false;
    }
    if (!map.logistics.reserve(c.id, source, destination, resource, amount))
    {
        return false;
    }
    copyRoute(c, planned);
    c.task = {};
    c.task.kind = CitizenTaskKind::Haul;
    c.task.source = source;
    c.task.destination = destination;
    c.task.startedMinute = minute;
    c.activity = CitizenActivity::Hauling;
    return true;
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
    for (const auto& source : map.logistics.inventories())
    {
        if ((source.kind != InventoryKind::Groundpile &&
             source.kind != InventoryKind::Workplace) ||
            source.used() <= 0)
        {
            continue;
        }
        if (!assignedDestination && distance(c.tilePosition, source.footprint) >
                                        policy.localSearchRadius)
        {
            continue;
        }
        bool covered = false;
        if (!assignedDestination &&
            minute - source.createdMinute <
                policy.stockpile.employeePreferenceMinutes)
        {
            for (const auto& w : map.employment().workplaces())
            {
                if (w.operational &&
                    w.objectTypeId == SettlementObjectTypes::Stockpile &&
                    map.employment().employed(w.id, citizens) > 0 &&
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
            if (destination.id == source.id || !publicStorage(destination) ||
                (assignedDestination &&
                 destination.id != assignedDestination) ||
                distance(source.footprint.topLeft, destination.footprint) >
                    policy.stockpile.collectionRadius)
            {
                continue;
            }
            for (const auto& goods : source.goods)
            {
                const int amount = std::min(
                    {policy.carryingCapacity,
                     map.logistics.available(source.id, goods.resource),
                     map.logistics.freeSpace(destination.id)}
                );
                if (amount > 0)
                {
                    opportunities.push_back(
                        {source.id,
                         destination.id,
                         goods.resource,
                         amount,
                         distance(c.tilePosition, source.footprint) +
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
bool SettlementActivitySystem::claimed(
    const SettlementCitizenState& citizens,
    const CitizenTask& task
) const
{
    for (const auto& other : citizens.citizens())
    {
        if (other.task.kind == task.kind &&
            ((task.site && task.site == other.task.site) ||
             (task.object && task.object == other.task.object) ||
             (task.command && task.command == other.task.command &&
              task.workTile == other.task.workTile)))
        {
            return true;
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
    std::vector<ConstructionSiteId> sites;
    for (const auto& site : map.objectState().constructionSites())
    {
        sites.push_back(site.id);
    }
    std::stable_sort(
        sites.begin(),
        sites.end(),
        [&](auto a, auto b)
        {
            return distance(
                       c.tilePosition,
                       map.objectState().constructionSite(a)->footprint
                   ) <
                   distance(
                       c.tilePosition,
                       map.objectState().constructionSite(b)->footprint
                   );
        }
    );
    for (std::size_t attempt = 0; attempt < sites.size(); ++attempt)
    {
        const auto id = sites[c.constructionSearchCursor++ % sites.size()];
        const auto site = *map.objectState().constructionSite(id);
        bool obstructed = false;
        for (int y = site.footprint.topLeft.y;
             y < site.footprint.topLeft.y + site.footprint.height;
             ++y)
        {
            for (int x = site.footprint.topLeft.x;
                 x < site.footprint.topLeft.x + site.footprint.width;
                 ++x)
            {
                const SettlementTilePosition tile{x, y};
                if (map.naturalFeatures().at(tile).kind ==
                    NaturalFeatureKind::None)
                {
                    continue;
                }
                obstructed = true;
                CitizenTask clearing;
                clearing.kind = CitizenTaskKind::Gather;
                clearing.site = id;
                clearing.workTile = tile;
                clearing.startedMinute = minute;
                if (!claimed(citizens, clearing) &&
                    route(map, citizens, c, {tile, 1, 1}, true))
                {
                    c.task = clearing;
                    c.activity = CitizenActivity::AssignedToCommand;
                    return true;
                }
            }
        }
        if (obstructed)
        {
            continue;
        }
        const auto destination = map.logistics.forSite(id);
        const auto* inventory = map.logistics.inventory(destination);
        if (!inventory)
        {
            continue;
        }
        bool ready = true;
        for (const auto& cost : site.resourceDeliveries)
        {
            if (inventory->amount(cost.resourceId) >= int(cost.requiredAmount))
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
                    map.logistics.available(source.id, cost.resourceId) <= 0)
                {
                    continue;
                }
                sources.push_back(
                    {source.id,
                     distance(c.tilePosition, source.footprint) +
                         distance(source.footprint.topLeft, site.footprint)}
                );
            }
            std::stable_sort(
                sources.begin(),
                sources.end(),
                [](const auto& a, const auto& b) { return a.score < b.score; }
            );
            for (const auto& source : sources)
            {
                const int amount = std::min(
                    {policy.carryingCapacity,
                     map.logistics.available(source.id, cost.resourceId),
                     map.logistics.freeSpace(destination),
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
        if (claimed(citizens, task))
        {
            continue;
        }
        // Route to a real interior work tile, not merely an access tile.
        const SettlementTilePosition goal{
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
        auto planned = c;
        if (!route(map, citizens, planned, {goal, 1, 1}, true) ||
            !site.footprint.contains(planned.destination))
        {
            continue;
        }
        task.workTile = planned.destination;
        task.startedMinute = minute;
        copyRoute(c, planned);
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
    struct CommandChoice
    {
        CitizenTask task;
        SettlementObjectFootprint footprint;
        int score;
    };
    std::vector<CommandChoice> choices;
    for (const auto& command : map.commandState().commands())
    {
        for (const auto& target : command.targets)
        {
            if (target.objectId)
            {
                const auto* object =
                    map.objectState().completedObject(target.objectId);
                if (!object)
                {
                    continue;
                }
            }
            CitizenTask task;
            task.kind = target.objectId || target.constructionId
                            ? CitizenTaskKind::Demolish
                            : CitizenTaskKind::Gather;
            task.site = target.constructionId;
            task.command = command.id;
            task.object = target.objectId;
            task.workTile = target.footprint.topLeft;
            task.startedMinute = minute;
            if (claimed(citizens, task))
            {
                continue;
            }
            choices.push_back(
                {task,
                 target.footprint,
                 distance(c.tilePosition, target.footprint)}
            );
        }
    }
    std::stable_sort(
        choices.begin(),
        choices.end(),
        [](const auto& a, const auto& b) { return a.score < b.score; }
    );
    for (std::size_t attempt = 0; attempt < choices.size(); ++attempt)
    {
        const auto& choice = choices[c.commandSearchCursor++ % choices.size()];
        if (route(map, citizens, c, choice.footprint, true))
        {
            c.task = choice.task;
            c.assignedCommandId = choice.task.command;
            c.activity = CitizenActivity::AssignedToCommand;
            return true;
        }
        if (pathsRemaining_ == 0)
        {
            break;
        }
    }
    return false;
}
} // namespace Paladin

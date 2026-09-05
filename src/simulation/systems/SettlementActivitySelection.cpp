#include "simulation/systems/SettlementActivitySystem.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include "world/settlements/objects/SettlementDoor.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

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
    const auto original = c.tilePosition;
    std::vector<SettlementTilePosition> exitPath;
    if (const auto* home = map.objectState().completedObject(c.homeId);
        home && home->door && home->footprint.contains(original))
    {
        auto p = original;
        while (p != *home->door)
        {
            if (p.x != home->door->x)
                p.x += p.x < home->door->x ? 1 : -1;
            else
                p.y += p.y < home->door->y ? 1 : -1;
            exitPath.push_back(p);
        }
        p = outsideDoor(home->footprint, *home->door);
        const auto* tile = map.grid().tile(p);
        if (!tile || tile->terrain == TerrainType::Water ||
            tile->terrain == TerrainType::Mountain ||
            map.objectState().blocksMovement(p))
            return false;
        exitPath.push_back(p);
        c.tilePosition = p;
    }
    bool successful = false;
    struct RestorePosition
    {
        SettlementCitizen& citizen;
        SettlementTilePosition original;
        std::vector<SettlementTilePosition>& exit;
        bool& successful;
        ~RestorePosition()
        {
            citizen.tilePosition = original;
            if (successful && !exit.empty())
            {
                citizen.path
                    .insert(citizen.path.begin(), exit.begin(), exit.end());
                citizen.explicitMovement = true;
            }
        }
    } restore{c, original, exitPath, successful};
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
    const auto* targetObject = map.objectState().completedObjectAt(f.topLeft);
    const bool doorAccess = !inside && targetObject &&
                            targetObject->footprint == f && targetObject->door;
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
        successful = true;
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
        if (c.breakUntil > 0 &&
            !breakTripFits(map, citizens, c, planned, minute, 1))
            continue;
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
bool SettlementActivitySystem::claimed(const CitizenTask& task) const
{
    if (task.kind == CitizenTaskKind::Gather)
        return gatheringClaims_.contains(
            (std::uint64_t(std::uint32_t(task.workTile.y)) << 32) |
            std::uint32_t(task.workTile.x)
        );
    if (task.kind == CitizenTaskKind::Demolish)
        return task.object ? demolitionClaims_.contains(task.object)
                           : siteDemolitionClaims_.contains(task.site);
    return false;
}
void SettlementActivitySystem::claim(const CitizenTask& task)
{
    if (task.kind == CitizenTaskKind::Gather)
        gatheringClaims_.insert(
            (std::uint64_t(std::uint32_t(task.workTile.y)) << 32) |
            std::uint32_t(task.workTile.x)
        );
    else if (task.kind == CitizenTaskKind::Demolish)
    {
        if (task.object)
            demolitionClaims_.insert(task.object);
        else if (task.site)
            siteDemolitionClaims_.insert(task.site);
    }
}
void SettlementActivitySystem::releaseClaim(const CitizenTask& task)
{
    if (task.kind == CitizenTaskKind::Gather)
        gatheringClaims_.erase(
            (std::uint64_t(std::uint32_t(task.workTile.y)) << 32) |
            std::uint32_t(task.workTile.x)
        );
    else if (task.kind == CitizenTaskKind::Demolish)
    {
        if (task.object)
            demolitionClaims_.erase(task.object);
        else if (task.site)
            siteDemolitionClaims_.erase(task.site);
    }
}
void SettlementActivitySystem::refreshConstructionBoard(
    const SettlementMap& map
)
{
    const auto version = map.objectState().navigationVersion();
    if (boardVersion_ == version)
        return;
    constructionBuckets_.clear();
    for (const auto& site : map.objectState().constructionSites())
    {
        const auto& f = site.footprint;
        for (int y = f.topLeft.y / 32; y <= (f.topLeft.y + f.height - 1) / 32;
             ++y)
            for (int x = f.topLeft.x / 32;
                 x <= (f.topLeft.x + f.width - 1) / 32;
                 ++x)
                constructionBuckets_
                    [(std::uint64_t(y) << 32) | std::uint32_t(x)]
                        .push_back(site.id);
    }
    std::erase_if(
        clearingCursors_,
        [&](const auto& entry)
        { return !map.objectState().constructionSite(entry.first); }
    );
    boardVersion_ = version;
}
bool SettlementActivitySystem::chooseConstruction(
    SettlementMap& map,
    SettlementCitizenState& citizens,
    SettlementCitizen& c,
    double minute
)
{
    refreshConstructionBoard(map);
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
            return;
        if (const auto* site = map.objectState().constructionSite(id))
            choices.push_back({id, distance(c.tilePosition, site->footprint)});
    };
    // A local job board bounds each worker's query independently of all queued
    // tiles.
    constexpr int chunk = 32;
    std::size_t examined = 0;
    for (int dy = -1; dy <= 1 && examined < 384; ++dy)
        for (int dx = -1; dx <= 1 && examined < 384; ++dx)
        {
            const int x = c.tilePosition.x / chunk + dx,
                      y = c.tilePosition.y / chunk + dy;
            if (x < 0 || y < 0)
                continue;
            const auto found = constructionBuckets_.find(
                (std::uint64_t(y) << 32) | std::uint32_t(x)
            );
            if (found == constructionBuckets_.end())
                continue;
            const auto& bucket = found->second;
            for (std::size_t i = 0;
                 i < std::min<std::size_t>(bucket.size(), 48);
                 ++i, ++examined)
                append(
                    bucket[(c.constructionSearchCursor + i) % bucket.size()]
                );
        }
    const auto& all = map.objectState().constructionSites();
    for (std::size_t i = 0; i < std::min<std::size_t>(all.size(), 64); ++i)
        append(all[(c.constructionSearchCursor + i) % all.size()].id);
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
        if (map.naturalFeatures().countIn(site.footprint) > 0)
        {
            auto& cursor = clearingCursors_[id];
            while (clearingBudget > 0)
            {
                const auto next = map.naturalFeatures().nextIn(
                    site.footprint,
                    cursor,
                    clearingBudget
                );
                if (!next)
                    break;
                const auto tile = *next;
                CitizenTask clearing;
                clearing.kind = CitizenTaskKind::Gather;
                clearing.site = id;
                clearing.workTile = tile;
                clearing.startedMinute = minute;
                if (!claimed(clearing) &&
                    route(map, citizens, c, {tile, 1, 1}, true))
                {
                    c.task = clearing;
                    claim(c.task);
                    c.activity = CitizenActivity::AssignedToCommand;
                    return true;
                }
                if (!pathsRemaining_)
                    return false;
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
            // Join that tile so labor never transfers between distant tiles.
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
CitizenTask SettlementActivitySystem::CommandWork::task() const
{
    CitizenTask result;
    result.kind =
        object || site ? CitizenTaskKind::Demolish : CitizenTaskKind::Gather;
    result.command = command;
    result.object = object;
    result.site = site;
    result.workTile = footprint.topLeft;
    return result;
}
bool SettlementActivitySystem::refreshCommandBoard(const SettlementMap& map)
{
    const auto& state = map.commandState();
    if (commandBoardVersion_ != state.selectionVersion() ||
        (!commandBoardReady_ && commandSourceVersion_ != state.version()))
    {
        commandJobs_.clear();
        commandBuckets_.clear();
        availableCommandJobs_.clear();
        commandBuildCommand_ = commandBuildTarget_ = commandSweep_ = 0;
        commandBoardReady_ = false;
        commandBoardVersion_ = state.selectionVersion();
        commandSourceVersion_ = state.version();
    }
    // Build and retire opportunities incrementally. A map-wide designation
    // must never become a map-wide burst of AI work on the following frame.
    std::size_t budget = 2048;
    const auto commands = state.commands();
    while (!commandBoardReady_ && commandBuildCommand_ < commands.size() &&
           budget > 0)
    {
        const auto& command = commands[commandBuildCommand_];
        while (commandBuildTarget_ < command.targets.size() && budget > 0)
        {
            --budget;
            const auto& target = command.targets[commandBuildTarget_++];
            const auto p = target.footprint.topLeft;
            auto& bucket = commandBuckets_
                [(std::uint64_t(p.y / 32) << 32) | std::uint32_t(p.x / 32)];
            const auto index = commandJobs_.size();
            commandJobs_.push_back(
                {command.id,
                 target.objectId,
                 target.constructionId,
                 target.footprint,
                 availableCommandJobs_.size(),
                 bucket.size()}
            );
            bucket.push_back(index);
            availableCommandJobs_.push_back(index);
        }
        if (commandBuildTarget_ >= command.targets.size())
        {
            ++commandBuildCommand_;
            commandBuildTarget_ = 0;
        }
    }
    if (commandBuildCommand_ >= commands.size())
        commandBoardReady_ = true;
    budget = std::min<std::size_t>(1024, availableCommandJobs_.size());
    while (commandBoardReady_ && !availableCommandJobs_.empty() && budget-- > 0)
    {
        commandSweep_ %= availableCommandJobs_.size();
        const auto index = availableCommandJobs_[commandSweep_];
        auto& job = commandJobs_[index];
        if (state.contains(
                map,
                job.command,
                job.footprint.topLeft,
                job.object,
                job.site
            ))
        {
            ++commandSweep_;
            continue;
        }
        const auto p = job.footprint.topLeft;
        auto& bucket = commandBuckets_
            [(std::uint64_t(p.y / 32) << 32) | std::uint32_t(p.x / 32)];
        bucket[job.bucketSlot] = bucket.back();
        commandJobs_[bucket.back()].bucketSlot = job.bucketSlot;
        bucket.pop_back();
        availableCommandJobs_[job.globalSlot] = availableCommandJobs_.back();
        commandJobs_[availableCommandJobs_.back()].globalSlot = job.globalSlot;
        availableCommandJobs_.pop_back();
        job.available = false;
    }
    return commandBoardReady_;
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
            return;
        const auto& job = commandJobs_[index];
        if (!map.commandState().contains(
                map,
                job.command,
                job.footprint.topLeft,
                job.object,
                job.site
            ))
            return;
        if (!claimed(job.task()))
            choices.push_back({index, distance(c.tilePosition, job.footprint)});
    };
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
        {
            const int x = c.tilePosition.x / 32 + dx,
                      y = c.tilePosition.y / 32 + dy;
            if (x < 0 || y < 0)
                continue;
            const auto found = commandBuckets_.find(
                (std::uint64_t(y) << 32) | std::uint32_t(x)
            );
            if (found == commandBuckets_.end())
                continue;
            const auto& bucket = found->second;
            for (std::size_t i = 0;
                 i < std::min<std::size_t>(bucket.size(), 32);
                 ++i)
                append(bucket[(c.commandSearchCursor + i) % bucket.size()]);
        }
    for (std::size_t i = 0;
         i < std::min<std::size_t>(availableCommandJobs_.size(), 64);
         ++i)
        append(
            availableCommandJobs_
                [(c.commandSearchCursor + i) % availableCommandJobs_.size()]
        );
    c.commandSearchCursor += 64;
    std::stable_sort(
        choices.begin(),
        choices.end(),
        [](const auto& a, const auto& b) { return a.score < b.score; }
    );
    for (const auto& choice : choices)
    {
        const auto& job = commandJobs_[choice.index];
        if (route(map, citizens, c, job.footprint, true))
        {
            c.task = job.task();
            claim(c.task);
            c.task.startedMinute = minute;
            c.assignedCommandId = c.task.command;
            c.activity = CitizenActivity::AssignedToCommand;
            return true;
        }
        if (!pathsRemaining_)
            break;
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
        return false;
    const auto workplace = *job;
    if (workplace.objectTypeId == SettlementObjectTypes::FishingGrounds)
    {
        const auto* object =
            map.objectState().completedObject(workplace.objectId);
        if (!object)
            return false;
        auto spots = fisheryShoreline(map.grid(), *object);
        std::stable_sort(
            spots.begin(),
            spots.end(),
            [&](const auto& a, const auto& b)
            {
                return distance(c.tilePosition, {a.land, 1, 1}) <
                       distance(c.tilePosition, {b.land, 1, 1});
            }
        );
        for (const auto& spot : spots)
        {
            if (std::any_of(
                    citizens.citizens().begin(),
                    citizens.citizens().end(),
                    [&](const auto& other)
                    {
                        return other.id != c.id &&
                               other.task.kind == CitizenTaskKind::Work &&
                               other.task.workTile == spot.land;
                    }
                ))
                continue;
            auto planned = c;
            if (!route(map, citizens, planned, {spot.land, 1, 1}, true) ||
                planned.destination != spot.land)
            {
                if (!pathsRemaining_)
                    break;
                continue;
            }
            copyRoute(c, planned);
            c.task = {};
            c.task.kind = CitizenTaskKind::Work;
            c.task.object = workplace.objectId;
            c.task.workTile = spot.land;
            c.task.target = spot.water;
            c.task.startedMinute = minute;
            c.activity = CitizenActivity::TravelingToWork;
            return true;
        }
        return false;
    }
    if (!route(map, citizens, c, workplace.footprint, true))
        return false;
    c.task = {};
    c.task.kind = CitizenTaskKind::Work;
    c.task.object = workplace.objectId;
    c.task.startedMinute = minute;
    c.activity = CitizenActivity::TravelingToWork;
    return true;
}
} // namespace Paladin

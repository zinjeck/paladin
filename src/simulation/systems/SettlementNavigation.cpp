#include "simulation/systems/SettlementNavigation.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
namespace Paladin
{
    void SettlementNavigation::synchronize(const SettlementMap& map)
    {
        if (sourceInstance_ == map.instanceId() &&
            version_ == map.objectState().navigationVersion())
        {
            return;
        }
        routeCache_.clear();
        roads_.assign(map.grid().tileCount(), 0);
        hasRoads_ = false;
        for (const auto& object : map.objectState().completedObjects())
        {
            if (object.objectTypeId != SettlementObjectTypes::Road)
            {
                continue;
            }
            hasRoads_ = true;
            const auto& f = object.footprint;
            for (int y = f.topLeft.y; y < f.topLeft.y + f.height; ++y)
            {
                for (int x = f.topLeft.x; x < f.topLeft.x + f.width; ++x)
                {
                    roads_[std::size_t(y) * map.grid().width() + x] = 1;
                }
            }
        }
        sourceInstance_ = map.instanceId();
        version_ = map.objectState().navigationVersion();
    }

    bool SettlementNavigation::walkable(
        const SettlementMap& map,
        SettlementTilePosition p,
        bool avoidBuildingFootprints,
        ConstructionSiteId escapeConstructionSite
    ) const
    {
        const auto* tile = map.grid().tile(p);
        if (!tile || tile->terrain != TerrainType::Land ||
            map.objectState().blocksMovement(p))
        {
            return false;
        }
        if (!avoidBuildingFootprints)
        {
            return true;
        }
        const auto allowed = [](std::string_view type)
        {
            return type == SettlementObjectTypes::Road ||
                   type == SettlementObjectTypes::Pastureland;
        };
        const auto* object = map.objectState().completedObjectAt(p);
        const auto* site = map.objectState().constructionSiteAt(p);
        return (!object || allowed(object->objectTypeId)) &&
               (!site || site->id == escapeConstructionSite ||
                allowed(site->objectTypeId));
    }

    bool SettlementNavigation::canStep(
        const SettlementMap& map,
        SettlementTilePosition a,
        SettlementTilePosition b,
        bool avoidBuildingFootprints,
        ConstructionSiteId escapeConstructionSite
    ) const
    {
        const int dx = std::abs(a.x - b.x), dy = std::abs(a.y - b.y);
        return dx <= 1 && dy <= 1 && dx + dy > 0 &&
               walkable(
                   map,
                   b,
                   avoidBuildingFootprints,
                   escapeConstructionSite
               ) &&
               (dx == 0 || dy == 0 ||
                (walkable(
                     map,
                     {a.x, b.y},
                     avoidBuildingFootprints,
                     escapeConstructionSite
                 ) &&
                 walkable(
                     map,
                     {b.x, a.y},
                     avoidBuildingFootprints,
                     escapeConstructionSite
                 )));
    }

    double SettlementNavigation::stepCost(
        const SettlementMap& map,
        SettlementTilePosition a,
        SettlementTilePosition b,
        const CitizenMovementPolicy& policy
    ) const
    {
        const auto i = std::size_t(b.y) * map.grid().width() + b.x;
        return (a.x != b.x && a.y != b.y ? policy.diagonalCost : 1.0) /
               (i < roads_.size() && roads_[i] ? policy.roadSpeedMultiplier
                                               : 1.0);
    }

    std::vector<SettlementTilePosition> SettlementNavigation::findPath(
        const SettlementMap& map,
        SettlementTilePosition start,
        SettlementTilePosition goal,
        const CitizenMovementPolicy& policy
    ) const
    {
        ScopedTiming timer{timing};
        ++requests;
        ++failures;
        expandedNodes = 0;
        candidates = 0;
        lastCost = 0;

        // Terrain excavation publishes through the same navigation revision
        // as construction. Cached failures must be retried after a new passage
        // opens, even before the next activity synchronization.
        if (routeSource_ != map.instanceId() ||
            routeVersion_ != map.objectState().navigationVersion())
        {
            routeCache_.clear();
            routeSource_ = map.instanceId();
            routeVersion_ = map.objectState().navigationVersion();
        }

        if (!map.grid().isValidPosition(start) ||
            !walkable(
                map,
                goal,
                policy.avoidBuildingFootprints,
                policy.escapeConstructionSite
            ) ||
            !std::isfinite(policy.roadSpeedMultiplier) ||
            policy.roadSpeedMultiplier <= 0 ||
            !std::isfinite(policy.diagonalCost) || policy.diagonalCost < 1 ||
            policy.diagonalCost > 2)
        {
            return {};
        }
        for(const auto& cached:routeCache_)
        {
            const auto& p=cached.policy;
            if(cached.start==start && cached.goal==goal && p.diagonalCost==policy.diagonalCost &&
               p.roadSpeedMultiplier==policy.roadSpeedMultiplier && p.avoidBuildingFootprints==policy.avoidBuildingFootprints &&
               p.escapeConstructionSite==policy.escapeConstructionSite && p.maximumExpandedNodes==policy.maximumExpandedNodes)
            {if(!cached.path.empty() || start==goal) --failures;lastCost=cached.cost;return cached.path;}
        }
        const auto remember=[&](const auto& path,double cost)
        {
            if(path.size()>4096) return;
            routeCache_.push_back({start,goal,policy,path,cost});
            while(routeCache_.size()>128) routeCache_.pop_front();
        };
        const int width = map.grid().width();
        const auto heuristic = [&](SettlementTilePosition p)
        {
            const int dx = std::abs(goal.x - p.x), dy = std::abs(goal.y - p.y);
            // Fastest possible terrain keeps A* admissible even with roads.
            return (std::max(dx, dy) +
                    (policy.diagonalCost - 1) * std::min(dx, dy)) /
                   (hasRoads_ ? std::max(1.0, policy.roadSpeedMultiplier) : 1.0);
        };
        std::vector<SettlementTilePosition> direct;
        double directCost = std::numeric_limits<double>::infinity();
        // Long unobstructed journeys need O(distance), not an expanding area
        // search that hits the node budget and masquerades as "too far".
        if (std::max(std::abs(goal.x - start.x), std::abs(goal.y - start.y)) >
            32)
        {
            auto p = start;
            double cost = 0;
            while (p != goal)
            {
                const SettlementTilePosition next{
                    p.x + (goal.x > p.x) - (goal.x < p.x),
                    p.y + (goal.y > p.y) - (goal.y < p.y)
                };
                if (!canStep(
                        map,
                        p,
                        next,
                        policy.avoidBuildingFootprints,
                        policy.escapeConstructionSite
                    ))
                {
                    break;
                }
                cost += stepCost(map, p, next, policy);
                direct.push_back(next);
                p = next;
            }
            if (p == goal)
            {
                if (cost <= heuristic(start) + 1e-9)
                {
                    --failures;
                    lastCost = cost;
                    remember(direct,cost);
                    return direct;
                }
                // A nearby road may beat the straight walk. Keep this proven
                // route as the fallback if the bounded search cannot improve it.
                directCost = cost;
            }
            else { direct.clear(); }
        }
        const auto index = [width](SettlementTilePosition p)
        { return std::size_t(p.y) * width + p.x; };
        const auto position = [width](std::size_t i)
        { return SettlementTilePosition{int(i % width), int(i / width)}; };
        struct Entry
        {
            double estimate, cost;
            std::size_t tile;
            bool operator<(const Entry& rhs) const
            {
                if (estimate != rhs.estimate)
                {
                    return estimate > rhs.estimate;
                }
                // Equal estimates prefer progress toward the goal, avoiding
                // broad expansion of equally promising long-distance routes.
                if (cost != rhs.cost)
                {
                    return cost < rhs.cost;
                }
                return tile > rhs.tile;
            }
        };
        if(searchRecords_.size()!=map.grid().tileCount()) searchRecords_.resize(map.grid().tileCount());
        if(++searchGeneration_==0) {for(auto& r:searchRecords_) r.generation=0; ++searchGeneration_;}
        auto& records=searchRecords_;
        std::size_t touched=1;
        std::priority_queue<Entry> frontier;
        const auto startIndex = index(start), goalIndex = index(goal);
        records[startIndex]={0,startIndex,searchGeneration_};
        frontier.push({heuristic(start), 0, startIndex});
        std::size_t expanded = 0;
        while (!frontier.empty() && expanded < policy.maximumExpandedNodes)
        {
            const auto current = frontier.top();
            frontier.pop();
            if (current.estimate >= directCost) { break; }
            if (current.cost != records[current.tile].cost)
            {
                continue;
            }
            if (current.tile == goalIndex)
            {
                std::vector<SettlementTilePosition> path;
                for (auto i = goalIndex; i != startIndex;
                     i = records[i].parent)
                {
                    path.push_back(position(i));
                }
                std::reverse(path.begin(), path.end());
                --failures;
                lastCost = current.cost;
                candidates = touched;
                remember(path,lastCost);
                return path;
            }
            ++expanded;
            expandedNodes = expanded;
            candidates = touched;
            const auto from = position(current.tile);
            for (int dy = -1; dy <= 1; ++dy)
            {
                for (int dx = -1; dx <= 1; ++dx)
                {
                    const SettlementTilePosition next{from.x + dx, from.y + dy};
                    if (!canStep(
                            map,
                            from,
                            next,
                            policy.avoidBuildingFootprints,
                            policy.escapeConstructionSite
                        ))
                    {
                        continue;
                    }
                    const auto ni = index(next);
                    const double cost =
                        current.cost + stepCost(map, from, next, policy);
                    auto& found=records[ni];
                    if(found.generation==searchGeneration_ && found.cost<=cost) continue;
                    touched+=found.generation!=searchGeneration_;
                    found={cost,current.tile,searchGeneration_};
                    frontier.push({cost + heuristic(next), cost, ni});
                }
            }
        }
        if (!direct.empty())
        {
            --failures;
            lastCost = directCost;
            remember(direct,lastCost);
            return direct;
        }
        remember(std::vector<SettlementTilePosition>{},0);
        return {};
    }
} // namespace Paladin

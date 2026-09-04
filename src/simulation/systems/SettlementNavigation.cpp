#include "simulation/systems/SettlementNavigation.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_map>
#include <limits>
namespace Paladin
{
    void SettlementNavigation::synchronize(const SettlementMap& map)
    {
        if (source_ == &map && version_ == map.objectState().presentationVersion()) return;
        source_ = &map;
        version_ = map.objectState().presentationVersion();
        roads_.assign(map.grid().tileCount(), 0);
        for (const auto& object : map.objectState().completedObjects())
        {
            if (object.objectTypeId != SettlementObjectTypes::Road) continue;
            const auto& f = object.footprint;
            for (int y = f.topLeft.y; y < f.topLeft.y + f.height; ++y)
                for (int x = f.topLeft.x; x < f.topLeft.x + f.width; ++x)
                    roads_[std::size_t(y) * map.grid().width() + x] = 1;
        }
    }
    bool SettlementNavigation::walkable(const SettlementMap& map, SettlementTilePosition p) const
    {
        const auto* tile = map.grid().tile(p);
        return tile && tile->terrain == TerrainType::Land && !map.objectState().blocksMovement(p);
    }
    bool SettlementNavigation::canStep(const SettlementMap& map,
        SettlementTilePosition a, SettlementTilePosition b) const
    {
        const int dx = std::abs(a.x - b.x), dy = std::abs(a.y - b.y);
        return dx <= 1 && dy <= 1 && dx + dy > 0 && walkable(map, b)
            && (dx == 0 || dy == 0
                || (walkable(map, {a.x, b.y}) && walkable(map, {b.x, a.y})));
    }
    double SettlementNavigation::stepCost(const SettlementMap& map,
        SettlementTilePosition a, SettlementTilePosition b, const CitizenMovementPolicy& policy) const
    {
        const auto i = std::size_t(b.y) * map.grid().width() + b.x;
        return (a.x != b.x && a.y != b.y ? policy.diagonalCost : 1.0)
            / (i < roads_.size() && roads_[i] ? policy.roadSpeedMultiplier : 1.0);
    }
    std::vector<SettlementTilePosition> SettlementNavigation::findPath(const SettlementMap& map,
        SettlementTilePosition start, SettlementTilePosition goal, const CitizenMovementPolicy& policy) const
    {
        if (!map.grid().isValidPosition(start) || !walkable(map, goal)) return {};
        const int width = map.grid().width();
        const auto index = [width](SettlementTilePosition p) { return std::size_t(p.y) * width + p.x; };
        const auto position = [width](std::size_t i) { return SettlementTilePosition{int(i % width), int(i / width)}; };
        const auto heuristic = [&](SettlementTilePosition p)
        {
            const int dx = std::abs(goal.x - p.x), dy = std::abs(goal.y - p.y);
            // Fastest possible terrain keeps A* admissible even with roads.
            return (std::max(dx, dy) + (policy.diagonalCost - 1) * std::min(dx, dy))
                / std::max(1.0, policy.roadSpeedMultiplier);
        };
        struct Record { double cost; std::size_t parent; };
        struct Entry
        {
            double estimate, cost;
            std::size_t tile;
            bool operator<(const Entry& rhs) const
            {
                if (estimate != rhs.estimate) return estimate > rhs.estimate;
                return tile > rhs.tile;
            }
        };
        std::unordered_map<std::size_t, Record> records;
        records.reserve(policy.maximumExpandedNodes * 2);
        std::priority_queue<Entry> frontier;
        const auto startIndex = index(start), goalIndex = index(goal);
        records.emplace(startIndex, Record{0, startIndex});
        frontier.push({heuristic(start), 0, startIndex});
        std::size_t expanded = 0;
        while (!frontier.empty() && expanded < policy.maximumExpandedNodes)
        {
            const auto current = frontier.top();
            frontier.pop();
            if (current.cost != records.at(current.tile).cost) continue;
            if (current.tile == goalIndex)
            {
                std::vector<SettlementTilePosition> path;
                for (auto i = goalIndex; i != startIndex; i = records.at(i).parent)
                    path.push_back(position(i));
                std::reverse(path.begin(), path.end());
                return path;
            }
            ++expanded;
            const auto from = position(current.tile);
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                {
                    const SettlementTilePosition next{from.x + dx, from.y + dy};
                    if (!canStep(map, from, next)) continue;
                    const auto ni = index(next);
                    const double cost = current.cost + stepCost(map, from, next, policy);
                    const auto found = records.find(ni);
                    if (found != records.end() && found->second.cost <= cost) continue;
                    records.insert_or_assign(ni, Record{cost, current.tile});
                    frontier.push({cost + heuristic(next), cost, ni});
                }
        }
        return {};
    }
}

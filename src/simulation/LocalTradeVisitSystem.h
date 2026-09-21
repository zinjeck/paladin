#pragma once
#include "simulation/systems/SettlementNavigation.h"
#include "world/generation/GenerationNoise.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/objects/WorkplaceCompound.h"
#include <array>
#include <deque>
#include <unordered_map>
namespace Paladin
{
    // Presentation actors only. The shipment remains the sole owner of cargo
    // and cash. No citizen is created, removed, employed, or teleported here.
    class LocalTradeVisitSystem
    {
    public:
        static void arrive(
            SettlementMap& map,
            ShipmentId shipment,
            SettlementObjectId depot,
            std::string_view resource,
            int quantity,
            bool importing,
            double minute
        )
        {
            map.trade.expire(minute);
            if (map.trade.visits.size() >= 12)
            {
                return;
            }
            const auto* building = map.objectState().completedObject(depot);
            if (!building)
            {
                for (const auto& object : map.objectState().completedObjects())
                {
                    if (object.objectTypeId ==
                        SettlementObjectTypes::TradeDepot)
                    {
                        depot = object.id;
                        building = &object;
                        break;
                    }
                }
            }
            if (!building)
            {
                return;
            }
            const auto footprint = building->footprint;
            SettlementNavigation navigation;
            auto dock = outsideDoor(
                workplaceRoom(footprint),
                workplaceRoomDoor(footprint)
            );
            if (!navigation.walkable(map, dock))
            {
                dock = {-1, -1};
            }
            for (int distance = 0; distance < 6 && dock.x < 0; ++distance)
            {
                const int y = footprint.topLeft.y + footprint.height + distance;
                for (int offset = 0; offset < footprint.width && dock.x < 0;
                     ++offset)
                {
                    const SettlementTilePosition tile{
                        footprint.topLeft.x + offset,
                        y
                    };
                    if (navigation.walkable(map, tile))
                    {
                        dock = tile;
                    }
                }
            }
            if (dock.x < 0)
            {
                return;
            }
            const int width = map.grid().width(), height = map.grid().height();
            const auto key = [width](SettlementTilePosition p)
            { return std::int64_t(p.y) * width + p.x; };
            const auto decode = [width](std::int64_t k)
            { return SettlementTilePosition{int(k % width), int(k / width)}; };
            const auto edgeDistance = [width, height](SettlementTilePosition p)
            { return std::min({p.x, p.y, width - 1 - p.x, height - 1 - p.y}); };
            // Try randomized actual edges with a bounded A* budget first.
            const auto seed = GenerationNoise::mix(
                shipment.value() ^ depot.value() ^ std::uint64_t(minute)
            );
            std::array<SettlementTilePosition, 8> edges{
                {{int(seed % width), 0},
                 {width - 1, int(seed % height)},
                 {int((seed >> 12) % width), height - 1},
                 {0, int((seed >> 12) % height)},
                 {dock.x, 0},
                 {width - 1, dock.y},
                 {dock.x, height - 1},
                 {0, dock.y}}
            };
            CitizenMovementPolicy policy;
            policy.maximumExpandedNodes = 4096;
            std::vector<SettlementTilePosition> path;
            for (std::size_t i = 0; i < edges.size(); ++i)
            {
                const auto edge = edges[(i + seed % 4) % edges.size()];
                if (!navigation.walkable(map, edge))
                {
                    continue;
                }
                path = navigation.findPath(map, dock, edge, policy);
                if (!path.empty())
                {
                    break;
                }
            }
            if (path.empty())
            {
                // Isolated islands still receive an abstract trader at the
                // nearest reachable approach, never through water or walls.
                std::unordered_map<std::int64_t, std::int64_t> parents;
                std::vector<SettlementTilePosition> queue{dock};
                parents.emplace(key(dock), key(dock));
                auto best = dock;
                constexpr SettlementTilePosition
                    directions[]{{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
                for (std::size_t cursor = 0;
                     cursor < queue.size() && cursor < 8192;
                     ++cursor)
                {
                    const auto at = queue[cursor];
                    if (edgeDistance(at) < edgeDistance(best))
                    {
                        best = at;
                    }
                    if (!edgeDistance(best))
                    {
                        break;
                    }
                    for (const auto direction : directions)
                    {
                        const SettlementTilePosition next{
                            at.x + direction.x,
                            at.y + direction.y
                        };
                        if (navigation.canStep(map, at, next) &&
                            !parents.contains(key(next)))
                        {
                            parents.emplace(key(next), key(at));
                            queue.push_back(next);
                        }
                    }
                }
                for (auto at = key(best);; at = parents.at(at))
                {
                    path.push_back(decode(at));
                    if (at == key(dock))
                    {
                        break;
                    }
                }
                std::reverse(path.begin(), path.end());
            }
            if (path.size() < 2)
            {
                return;
            }
            if (path.front() != dock)
            {
                path.insert(path.begin(), dock);
            }
            std::reverse(path.begin(), path.end());
            const double duration =
                std::clamp(double(path.size()) * .18, 18., 45.);
            map.trade.visits.push_back(
                {shipment,
                 depot,
                 std::string(resource),
                 quantity,
                 importing,
                 minute,
                 duration,
                 std::move(path)}
            );
        }
    };
} // namespace Paladin

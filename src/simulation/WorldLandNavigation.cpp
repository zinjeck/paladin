#include "simulation/WorldLandNavigation.h"
#include "world/WorldGrid.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <functional>
#include <queue>
#include <tuple>

namespace Paladin
{
    namespace
    {
        int wrappedDelta(int from, int to, int width) noexcept
        {
            int delta = to - from;
            if (width > 0 && std::abs(delta) > width / 2)
                delta += delta > 0 ? -width : width;
            return delta;
        }
        bool dryLand(const WorldGrid& grid, WorldTilePosition p) noexcept
        {
            const auto* tile = grid.tile(p);
            return tile && tile->terrain == TerrainType::Land;
        }
    }
    double worldLandStepDistance(WorldTilePosition from, WorldTilePosition to, int width) noexcept
    {
        return std::hypot(double(wrappedDelta(from.x, to.x, width)), double(to.y - from.y));
    }
    bool worldLandStepAllowed(const WorldGrid& grid, WorldTilePosition from,
        WorldTilePosition to, WorldLandMovement movement) noexcept
    {
        if (!dryLand(grid, from) || !dryLand(grid, to)) return false;
        const int dx = wrappedDelta(from.x, to.x, grid.width()), dy = to.y - from.y;
        if (std::abs(dx) > 1 || std::abs(dy) > 1 || (!dx && !dy)) return false;
        if (!dx || !dy) return true;
        return movement == WorldLandMovement::EightWay &&
            dryLand(grid, {to.x, from.y}) && dryLand(grid, {from.x, to.y});
    }
    std::optional<std::vector<WorldTilePosition>> worldLandRoute(
        const WorldGrid& grid, WorldTilePosition start, WorldTilePosition destination,
        WorldLandMovement movement)
    {
        if (!dryLand(grid, start) || !dryLand(grid, destination) ||
            grid.tileCount() > std::size_t(std::numeric_limits<int>::max())) return {};
        const int width = grid.width();
        const auto index = [width](WorldTilePosition p) { return p.y * width + p.x; };
        const bool diagonal = movement == WorldLandMovement::EightWay;
        // Fixed-point octile cost makes ties reproducible on all platforms.
        const auto estimate = [&](WorldTilePosition p) -> std::int64_t
        {
            const int dx = std::abs(wrappedDelta(p.x, destination.x, width));
            const int dy = std::abs(p.y - destination.y);
            return diagonal ? 1000LL * std::max(dx, dy) + 414LL * std::min(dx, dy)
                            : 1000LL * (dx + dy);
        };
        constexpr auto Infinite = std::numeric_limits<std::int64_t>::max();
        std::vector<int> parents(grid.tileCount(), -1);
        std::vector<std::int64_t> cost(grid.tileCount(), Infinite);
        // (estimated total, remaining estimate, node index): stable tie order.
        using Entry = std::tuple<std::int64_t, std::int64_t, int>;
        std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;
        const int first = index(start), last = index(destination);
        parents[first] = first; cost[first] = 0;
        open.emplace(estimate(start), estimate(start), first);
        constexpr std::array<WorldTilePosition, 8> offsets{{
            {1,0}, {-1,0}, {0,1}, {0,-1}, {1,1}, {-1,1}, {1,-1}, {-1,-1}}};
        std::size_t expanded = 0;
        bool reached = false;
        while (!open.empty() && expanded < 262144)
        {
            const auto [total, remaining, at] = open.top(); open.pop();
            if (total - remaining != cost[at]) continue; // superseded queue entry
            if (at == last) { reached = true; break; }
            ++expanded;
            const WorldTilePosition p{at % width, at / width};
            for (std::size_t i = 0; i < (diagonal ? 8u : 4u); ++i)
            {
                const auto d = offsets[i];
                const WorldTilePosition next{(p.x + d.x + width) % width, p.y + d.y};
                if (!worldLandStepAllowed(grid, p, next, movement)) continue;
                const auto nextCost = cost[at] + (i < 4 ? 1000 : 1414);
                const int nextIndex = index(next);
                if (nextCost >= cost[nextIndex]) continue;
                cost[nextIndex] = nextCost; parents[nextIndex] = at;
                const auto h = estimate(next);
                open.emplace(nextCost + h, h, nextIndex);
            }
        }
        if (!reached) return {};
        std::vector<WorldTilePosition> path;
        for (int at = last;; at = parents[at])
        {
            path.push_back({at % width, at / width});
            if (at == first) break;
        }
        std::reverse(path.begin(), path.end());
        return path;
    }
}

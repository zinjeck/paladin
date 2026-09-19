#include "simulation/WorldLandNavigation.h"
#include "world/WorldGrid.h"
#include <algorithm>
#include <array>
#include <limits>

namespace Paladin
{
    std::optional<std::vector<WorldTilePosition>> worldLandRoute(
        const WorldGrid& grid, WorldTilePosition start, WorldTilePosition destination)
    {
        const auto* from = grid.tile(start); const auto* to = grid.tile(destination);
        if (!from || !to || from->terrain != TerrainType::Land || to->terrain != TerrainType::Land ||
            grid.tileCount() > std::size_t(std::numeric_limits<int>::max())) return {};
        const int width = grid.width();
        const auto index = [width](WorldTilePosition p) { return p.y * width + p.x; };
        std::vector<int> parents(grid.tileCount(), -1), open;
        open.reserve(std::min<std::size_t>(grid.tileCount(), 262144));
        const int first = index(start), last = index(destination);
        parents[first] = first; open.push_back(first);
        constexpr std::array<WorldTilePosition,4> offsets{{{1,0},{-1,0},{0,1},{0,-1}}};
        for (std::size_t cursor = 0; cursor < open.size() && cursor < 262144 && parents[last] < 0; ++cursor)
        {
            const WorldTilePosition p{open[cursor] % width, open[cursor] / width};
            for (const auto d : offsets)
            {
                const WorldTilePosition next{(p.x + d.x + width) % width, p.y + d.y};
                const auto* tile = grid.tile(next);
                if (!tile || tile->terrain != TerrainType::Land || parents[index(next)] >= 0) continue;
                parents[index(next)] = index(p); open.push_back(index(next));
            }
        }
        if (parents[last] < 0) return {};
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

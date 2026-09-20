#pragma once

#include "world/Settlement.h"
#include "world/WorldGrid.h"
#include "world/territory/TerritoryMap.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <queue>
#include <span>
#include <vector>

namespace Paladin
{
    struct RealmLinkCell
    {
        WorldTilePosition position;
        float strength;
    };

    // A bounded land catchment around a short least-cost route. Longitude
    // wraps; water, rival civic claims and rival settlement cores are
    // impassable. The same geometry feeds civic control and the tribal
    // influence model.
    inline std::vector<RealmLinkCell> realmTerritoryConnection(
        const WorldGrid& grid,
        const Settlement& a,
        const Settlement& b,
        std::span<const Settlement> settlements,
        const TerritoryMap* control = nullptr
    )
    {
        if (a.ownerRealmId() != b.ownerRealmId())
        {
            return {};
        }
        int dx = b.position().x - a.position().x;
        if (dx > grid.width() / 2)
        {
            dx -= grid.width();
        }
        if (dx < -grid.width() / 2)
        {
            dx += grid.width();
        }
        const int dy = b.position().y - a.position().y;
        const float separation = std::hypot(float(dx), float(dy));
        if (separation < 1 || separation > 24)
        {
            return {};
        }
        constexpr int Padding = 14;
        const int minX = std::min(0, dx) - Padding;
        const int minY = std::min(0, dy) - Padding;
        const int width = std::abs(dx) + 2 * Padding + 1;
        const int height = std::abs(dy) + 2 * Padding + 1;
        const int count = width * height;
        const auto position = [&](int index)
        {
            int x = a.position().x + minX + index % width;
            x = (x % grid.width() + grid.width()) % grid.width();
            return WorldTilePosition{x, a.position().y + minY + index / width};
        };
        std::vector<float> resistance(count, 0);
        for (int i = 0; i < count; ++i)
        {
            const auto p = position(i);
            const auto* tile = grid.tile(p);
            if (!tile || tile->terrain == TerrainType::Water)
            {
                continue;
            }
            if (control)
            {
                const auto owner = control->controllerAt(p);
                if (owner && owner != a.ownerRealmId())
                {
                    continue;
                }
            }
            bool rival = false;
            for (const auto& other : settlements)
            {
                if (other.ownerRealmId() == a.ownerRealmId())
                {
                    continue;
                }
                int x = std::abs(p.x - other.position().x);
                x = std::min(x, grid.width() - x);
                if (x * x + (p.y - other.position().y) *
                                (p.y - other.position().y) <=
                    16)
                {
                    rival = true;
                    break;
                }
            }
            if (rival)
            {
                continue;
            }
            resistance[i] = tile->terrain == TerrainType::Mountain ||
                                    tile->relief == ReliefType::Mountain
                                ? (control ? 1.3F : 2.4F)
                            : tile->relief == ReliefType::Hills ? 1.25F
                                                                : 1.F;
            // Low-amplitude coherent terrain variation avoids identical tubes.
            resistance[i] += .12F * (1.F + std::sin(p.x * .63F + p.y * .37F));
        }
        using Node = std::pair<float, int>;
        std::priority_queue<Node, std::vector<Node>, std::greater<Node>> queue;
        std::vector<float> cost(count, std::numeric_limits<float>::infinity());
        std::vector<int> previous(count, -1);
        const int start = -minY * width - minX;
        const int finish = (dy - minY) * width + dx - minX;
        if (!resistance[start] || !resistance[finish])
        {
            return {};
        }
        cost[start] = 0;
        queue.push({0.F, start});
        const float budget = separation * 1.6F + 2;
        const auto neighbors = [&](int index, auto visit)
        {
            const int x = index % width, y = index / width;
            if (x > 0)
            {
                visit(index - 1);
            }
            if (x + 1 < width)
            {
                visit(index + 1);
            }
            if (y > 0)
            {
                visit(index - width);
            }
            if (y + 1 < height)
            {
                visit(index + width);
            }
        };
        while (!queue.empty())
        {
            const auto [distance, index] = queue.top();
            queue.pop();
            if (distance != cost[index])
            {
                continue;
            }
            if (index == finish)
            {
                break;
            }
            neighbors(
                index,
                [&](int next)
                {
                    const float d = distance + resistance[next];
                    if (resistance[next] && d <= budget && d < cost[next])
                    {
                        cost[next] = d;
                        previous[next] = index;
                        queue.push({d, next});
                    }
                }
            );
        }
        if (!std::isfinite(cost[finish]))
        {
            return {};
        }
        std::vector<int> route;
        for (int i = finish; i != -1; i = previous[i])
        {
            route.push_back(i);
        }
        queue = {};
        std::fill(
            cost.begin(),
            cost.end(),
            std::numeric_limits<float>::infinity()
        );
        for (int i : route)
        {
            cost[i] = 0;
            queue.push({0.F, i});
        }
        // Civic land belongs to a connected district, not just its road.
        // Modest mountain resistance keeps enclosed ridges within that
        // district.
        const float reach = control ? std::max(10.F, separation * .48F)
                            : (a.isFortress() || b.isFortress()) ? 6.5F
                                                                 : 5.F;
        std::vector<RealmLinkCell> result;
        while (!queue.empty())
        {
            const auto [distance, index] = queue.top();
            queue.pop();
            if (distance != cost[index])
            {
                continue;
            }
            const float t = distance / reach;
            result.push_back({position(index), (1.F - t) * (1.F - t)});
            neighbors(
                index,
                [&](int next)
                {
                    const float d = distance + resistance[next];
                    if (resistance[next] && d < reach && d < cost[next])
                    {
                        cost[next] = d;
                        queue.push({d, next});
                    }
                }
            );
        }
        return result;
    }

    template<class Claim>
    inline void consolidateCivicDistrict(
        const WorldGrid& grid,
        const TerritoryMap& control,
        RealmId realm,
        std::span<const Settlement> settlements,
        Claim claim
    )
    {
        std::optional<WorldTilePosition> anchor;
        int left = 0, right = 0, top = grid.height(), bottom = -1;
        const auto wrappedDelta = [&](int x)
        {
            int dx = x - anchor->x;
            if (dx > grid.width() / 2)
            {
                dx -= grid.width();
            }
            if (dx < -grid.width() / 2)
            {
                dx += grid.width();
            }
            return dx;
        };
        for (const auto& city : settlements)
        {
            if (city.ownerRealmId() != realm)
            {
                continue;
            }
            if (!anchor)
            {
                anchor = city.position();
            }
            const int dx = wrappedDelta(city.position().x);
            left = std::min(left, dx - 16);
            right = std::max(right, dx + 16);
            top = std::min(top, city.position().y - 16);
            bottom = std::max(bottom, city.position().y + 16);
        }
        if (!anchor)
        {
            return;
        }
        top = std::max(0, top);
        bottom = std::min(grid.height() - 1, bottom);
        const int width = std::min(grid.width(), right - left + 1);
        const int height = bottom - top + 1;
        const auto position = [&](int i)
        {
            return WorldTilePosition{
                (anchor->x + left + i % width + grid.width()) % grid.width(),
                top + i / width
            };
        };
        std::vector<std::uint8_t> cells(std::size_t(width) * height, 0);
        for (int i = 0; i < int(cells.size()); ++i)
        {
            const auto p = position(i);
            const auto owner = control.controllerAt(p);
            cells[i] = owner == realm ? 1
                       : owner || grid.tile(p)->terrain == TerrainType::Water
                           ? 2
                           : 0;
        }
        // Foreign population centers remain protected even without binary
        // tribal ownership. A cosmetic consolidation must not annex them.
        for (const auto& city : settlements)
        {
            if (city.ownerRealmId() == realm)
            {
                continue;
            }
            const int cx = wrappedDelta(city.position().x) - left;
            const int cy = city.position().y - top;
            for (int y = std::max(0, cy - 4); y <= std::min(height - 1, cy + 4);
                 ++y)
            {
                for (int x = std::max(0, cx - 4);
                     x <= std::min(width - 1, cx + 4);
                     ++x)
                {
                    if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= 16 &&
                        cells[y * width + x] != 1)
                    {
                        cells[y * width + x] = 2;
                    }
                }
            }
        }
        const auto neighbors = [&](int i, auto visit)
        {
            if (i % width > 0)
            {
                visit(i - 1);
            }
            if (i % width + 1 < width)
            {
                visit(i + 1);
            }
            if (i >= width)
            {
                visit(i - width);
            }
            if (i + width < int(cells.size()))
            {
                visit(i + width);
            }
        };
        std::vector<int> distance(cells.size(), -1), previous(cells.size(), -1),
            queue;
        for (int i = 0; i < int(cells.size()); ++i)
        {
            if (cells[i] == 1)
            {
                distance[i] = 0;
                queue.push_back(i);
            }
        }
        for (std::size_t cursor = 0; cursor < queue.size(); ++cursor)
        {
            const int i = queue[cursor];
            if (distance[i] >= 3)
            {
                continue;
            }
            neighbors(
                i,
                [&](int n)
                {
                    if (cells[n] == 0 && distance[n] < 0)
                    {
                        distance[n] = distance[i] + 1;
                        previous[n] = i;
                        queue.push_back(n);
                    }
                }
            );
        }
        for (int i : queue)
        {
            if (cells[i] != 0)
            {
                continue;
            }
            bool coast = false;
            neighbors(
                i,
                [&](int n)
                {
                    coast |=
                        grid.tile(position(n))->terrain == TerrainType::Water;
                }
            );
            if (!coast)
            {
                continue;
            }
            for (int n = i; n >= 0 && cells[n] == 0; n = previous[n])
            {
                static_cast<void>(claim(position(n)));
                cells[n] = 1;
            }
        }
        std::vector<bool> visited(cells.size(), false);
        for (int start = 0; start < int(cells.size()); ++start)
        {
            if (cells[start] != 0 || visited[start])
            {
                continue;
            }
            queue = {start};
            visited[start] = true;
            bool enclosed = true;
            for (std::size_t cursor = 0; cursor < queue.size(); ++cursor)
            {
                const int i = queue[cursor];
                if (i % width == 0 || i % width == width - 1 || i < width ||
                    i >= int(cells.size()) - width)
                {
                    enclosed = false;
                }
                neighbors(
                    i,
                    [&](int n)
                    {
                        if (cells[n] == 2)
                        {
                            enclosed = false;
                        }
                        else if (cells[n] == 0 && !visited[n])
                        {
                            visited[n] = true;
                            queue.push_back(n);
                        }
                    }
                );
            }
            if (enclosed && queue.size() <= 256)
            {
                for (int i : queue)
                {
                    static_cast<void>(claim(position(i)));
                }
            }
        }
    }
} // namespace Paladin

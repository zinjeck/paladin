#pragma once

#include "world/Settlement.h"
#include "world/WorldGrid.h"
#include "world/territory/TerritoryMap.h"
#include <algorithm>
#include <cmath>
#include <limits>
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
        constexpr int Padding = 8;
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
                                ? 2.4F
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
        const float reach = (a.isFortress() || b.isFortress()) ? 6.5F : 5.F;
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
} // namespace Paladin

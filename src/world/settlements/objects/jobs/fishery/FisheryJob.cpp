#include "world/settlements/objects/jobs/fishery/FisheryJob.h"
#include "simulation/systems/SettlementNavigation.h"
#include "world/generation/GenerationNoise.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace Paladin
{
    int fisheryReach(
        const SettlementObjectFootprint& footprint,
        const FisheryJobPolicy& policy
    )
    {
        return std::max(
            1,
            int(std::lround(
                std::max(1, policy.baseReach) *
                std::pow(
                    std::max(
                        1.0,
                        double(footprint.width) * footprint.height /
                            std::max(1, policy.referenceArea)
                    ),
                    policy.reachAreaExponent
                )
            ))
        );
    }
    double fisheryProductionPerMinute(
        std::size_t waterTiles,
        int attendingWorkers,
        const FisheryJobPolicy& policy
    )
    {
        if (policy.minutesPerFish <= 0 || policy.waterTilesPerWorker <= 0)
        {
            return 0;
        }
        return std::min(
                   double(std::max(0, attendingWorkers)),
                   double(waterTiles) / policy.waterTilesPerWorker
               ) /
               policy.minutesPerFish;
    }
    FisheryZonePreview fisheryZonePreview(
        const SettlementGrid& grid,
        const SettlementObjectState& objects,
        const SettlementObjectFootprint& f
    )
    {
        FisheryZonePreview result;
        const FisheryJobPolicy policy;
        const int radius = fisheryReach(f, policy);
        const int left = std::max(0, f.topLeft.x - radius);
        const int top = std::max(0, f.topLeft.y - radius);
        const int right =
            std::min(grid.width(), f.topLeft.x + f.width + radius);
        const int bottom =
            std::min(grid.height(), f.topLeft.y + f.height + radius);
        result.bounds = {{left, top}, right - left, bottom - top};
        std::unordered_set<std::size_t> claimed;
        const auto record = [&](const auto& entries)
        {
            for (const auto& entry : entries)
            {
                if (entry.objectTypeId != SettlementObjectTypes::FishingGrounds)
                {
                    continue;
                }
                for (auto p : entry.productionWater)
                {
                    claimed.insert(std::size_t(p.y) * grid.width() + p.x);
                }
            }
        };
        record(objects.completedObjects());
        record(objects.constructionSites());
        for (int y = top; y < bottom; ++y)
        {
            for (int x = left; x < right; ++x)
            {
                const SettlementTilePosition p{x, y};
                const double dx =
                    x - std::clamp(x, f.topLeft.x, f.topLeft.x + f.width - 1);
                const double dy =
                    y - std::clamp(y, f.topLeft.y, f.topLeft.y + f.height - 1);
                if (dx * dx + dy * dy > double(radius) * radius)
                {
                    continue;
                }
                if (grid.tile(p)->terrain != TerrainType::Water)
                {
                    continue;
                }
                (claimed.contains(std::size_t(y) * grid.width() + x)
                     ? result.excludedWater
                     : result.availableWater)
                    .push_back(p);
            }
        }
        return result;
    }
} // namespace Paladin

namespace Paladin
{
    std::vector<FishingSpot> fisheryShoreline(
        const SettlementGrid& grid,
        const CompletedSettlementObject& fishery
    )
    {
        std::vector<FishingSpot> spots;
        std::unordered_set<std::uint64_t> seen;
        for (const auto water : fishery.productionWater)
        {
            for (const auto delta :
                 {SettlementTilePosition{1, 0}, {-1, 0}, {0, 1}, {0, -1}})
            {
                const SettlementTilePosition land{
                    water.x + delta.x,
                    water.y + delta.y
                };
                const auto* tile = grid.tile(land);
                if (!tile || tile->terrain != TerrainType::Land)
                {
                    continue;
                }
                const auto key = (std::uint64_t(std::uint32_t(land.x)) << 32) |
                                 std::uint32_t(land.y);
                if (seen.insert(key).second)
                {
                    spots.push_back({land, water});
                }
            }
        }
        return spots;
    }
} // namespace Paladin

namespace Paladin
{
    namespace
    {
        std::uint64_t waterKey(SettlementTilePosition p)
        {
            return (std::uint64_t(std::uint32_t(p.y)) << 32) |
                   std::uint32_t(p.x);
        }
        constexpr SettlementTilePosition
            WaterSteps[]{{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
    } // namespace
    void FisheryBoatNavigation::synchronize(
        const SettlementObjectState& objects
    )
    {
        if (objectVersion_ == objects.navigationVersion())
        {
            return;
        }
        objectVersion_ = objects.navigationVersion();
        std::erase_if(
            entries_,
            [&](const auto& item)
            {
                return !objects.completedObject(item.first) ||
                       item.second.launch.land.x < 0;
            }
        );
    }
    FisheryBoatNavigation::Entry& FisheryBoatNavigation::prepare(
        const SettlementMap& map,
        const CompletedSettlementObject& object
    )
    {
        synchronize(map.objectState());
        if (const auto at = entries_.find(object.id); at != entries_.end())
        {
            return at->second;
        }
        Entry result;
        auto oldPolicy = FisheryJobPolicy{};
        oldPolicy.baseReach = 4;
        const int oldReach = fisheryReach(object.footprint, oldPolicy);
        const auto& f = object.footprint;
        for (const auto p : object.productionWater)
        {
            const int dx =
                p.x - std::clamp(p.x, f.topLeft.x, f.topLeft.x + f.width - 1);
            const int dy =
                p.y - std::clamp(p.y, f.topLeft.y, f.topLeft.y + f.height - 1);
            result.productiveWater += dx * dx + dy * dy <= oldReach * oldReach;
        }
        SettlementNavigation navigation;
        auto shores = fisheryShoreline(map.grid(), object);
        const auto distance = [&](const FishingSpot& p)
        {
            return std::abs(2 * p.land.x - (2 * f.topLeft.x + f.width - 1)) +
                   std::abs(2 * p.land.y - (2 * f.topLeft.y + f.height - 1));
        };
        std::stable_sort(
            shores.begin(),
            shores.end(),
            [&](auto a, auto b) { return distance(a) < distance(b); }
        );
        for (const auto shore : shores)
        {
            if (navigation.walkable(map, shore.land))
            {
                result.launch = shore;
                break;
            }
        }
        if (result.launch.land.x >= 0)
        {
            const auto accepted = [&](SettlementTilePosition p)
            {
                // Construction stores its accepted zone in row-major order.
                return std::binary_search(
                    object.productionWater.begin(),
                    object.productionWater.end(),
                    p,
                    [](auto a, auto b) { return waterKey(a) < waterKey(b); }
                );
            };
            std::unordered_set<std::uint64_t> seen;
            result.nodes.push_back({result.launch.water, 0, 0});
            seen.insert(waterKey(result.launch.water));
            for (std::size_t head = 0; head < result.nodes.size() &&
                                       result.nodes.size() < MaximumWaterNodes;
                 ++head)
            {
                const auto node = result.nodes[head];
                for (const auto d : WaterSteps)
                {
                    const SettlementTilePosition next{
                        node.tile.x + d.x,
                        node.tile.y + d.y
                    };
                    const auto* tile = map.grid().tile(next);
                    if (!tile || tile->terrain != TerrainType::Water ||
                        !accepted(next) || !seen.insert(waterKey(next)).second)
                    {
                        continue;
                    }
                    result.nodes.push_back({next, head, node.depth + 1});
                    if (result.nodes.size() == MaximumWaterNodes)
                    {
                        break;
                    }
                }
            }
        }
        return entries_.emplace(object.id, std::move(result)).first->second;
    }
    std::size_t FisheryBoatNavigation::productiveWater(
        const SettlementMap& map,
        const CompletedSettlementObject& object
    )
    {
        return prepare(map, object).productiveWater;
    }
    FishingBoatPlan FisheryBoatNavigation::plan(
        const SettlementMap& map,
        const CompletedSettlementObject& object,
        std::uint64_t sequence,
        std::span<const SettlementTilePosition> occupiedTargets
    )
    {
        if (const auto cached = entries_.find(object.id);
            cached != entries_.end())
        {
            SettlementNavigation navigation;
            if (cached->second.launch.land.x >= 0 &&
                !navigation.walkable(map, cached->second.launch.land))
            {
                entries_.erase(cached);
            }
        }
        auto& entry = prepare(map, object);
        if (entry.nodes.empty())
        {
            return {};
        }
        const auto offset = GenerationNoise::mix(sequence) % entry.nodes.size();
        for (std::size_t attempt = 0; attempt < entry.nodes.size(); ++attempt)
        {
            auto index = (offset + attempt) % entry.nodes.size();
            const auto& node = entry.nodes[index];
            if ((node.depth < 2 && entry.nodes.size() > 3) ||
                std::find(
                    occupiedTargets.begin(),
                    occupiedTargets.end(),
                    node.tile
                ) != occupiedTargets.end())
            {
                continue;
            }
            FishingBoatPlan result;
            result.launch = entry.launch;
            while (index != 0)
            {
                result.route.push_back(entry.nodes[index].tile);
                index = entry.nodes[index].parent;
            }
            result.route.push_back(entry.launch.water);
            std::reverse(result.route.begin(), result.route.end());
            return result;
        }
        return {};
    }
    std::vector<SettlementTilePosition> FisheryBoatNavigation::returnRoute(
        const SettlementMap& map,
        SettlementTilePosition from,
        SettlementTilePosition preferredLanding
    ) const
    {
        SettlementNavigation navigation;
        if (navigation.walkable(map, from))
        {
            return {};
        }
        if (navigation.walkable(map, preferredLanding))
        {
            // The accepted-water tree also supplies the return leg. A whole
            // shift finishing together must not launch one water flood fill
            // per fisher. Search is bounded by this fishery's cached nodes;
            // only a blocked landing or an unfamiliar tile needs a new BFS.
            for (const auto& [id, entry] : entries_)
            {
                if (entry.launch.land != preferredLanding)
                {
                    continue;
                }
                const auto node = std::find_if(
                    entry.nodes.begin(),
                    entry.nodes.end(),
                    [from](const Node& candidate)
                    { return candidate.tile == from; }
                );
                if (node == entry.nodes.end())
                {
                    continue;
                }
                std::vector<SettlementTilePosition> result;
                auto index = std::size_t(node - entry.nodes.begin());
                while (index != 0)
                {
                    index = entry.nodes[index].parent;
                    result.push_back(entry.nodes[index].tile);
                }
                result.push_back(preferredLanding);
                return result;
            }
        }
        std::vector<Node> nodes{{from, 0, 0}};
        std::unordered_set<std::uint64_t> seen{waterKey(from)};
        std::size_t selected = 0;
        SettlementTilePosition landing{-1, -1};
        bool preferredFound = false;
        for (std::size_t head = 0;
             !preferredFound && head < nodes.size() && head < MaximumWaterNodes;
             ++head)
        {
            const auto node = nodes[head];
            for (const auto d : WaterSteps)
            {
                const SettlementTilePosition next{
                    node.tile.x + d.x,
                    node.tile.y + d.y
                };
                const auto* tile = map.grid().tile(next);
                if (!tile)
                {
                    continue;
                }
                if (navigation.walkable(map, next))
                {
                    if (landing.x < 0 || next == preferredLanding)
                    {
                        landing = next;
                        selected = head;
                    }
                    if (next == preferredLanding)
                    {
                        preferredFound = true;
                        break;
                    }
                }
                else if (
                    tile->terrain == TerrainType::Water &&
                    nodes.size() < MaximumWaterNodes &&
                    seen.insert(waterKey(next)).second
                )
                {
                    nodes.push_back({next, head, node.depth + 1});
                }
            }
        }
        if (landing.x < 0)
        {
            return {};
        }
        std::vector<SettlementTilePosition> result{landing};
        while (selected != 0)
        {
            result.push_back(nodes[selected].tile);
            selected = nodes[selected].parent;
        }
        std::reverse(result.begin(), result.end());
        return result;
    }
} // namespace Paladin

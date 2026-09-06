#include "world/settlements/objects/jobs/fishery/FisheryJob.h"
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

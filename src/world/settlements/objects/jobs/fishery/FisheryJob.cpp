#include "world/settlements/objects/jobs/fishery/FisheryJob.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace Paladin
{
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
               double(attendingWorkers),
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
    const int radius = std::max(
        policy.baseReach,
        int(std::lround(
            policy.baseReach *
            std::sqrt(double(f.width) * f.height / policy.referenceArea)
        ))
    );
    const int left = std::max(0, f.topLeft.x - radius);
    const int top = std::max(0, f.topLeft.y - radius);
    const int right = std::min(grid.width(), f.topLeft.x + f.width + radius);
    const int bottom = std::min(grid.height(), f.topLeft.y + f.height + radius);
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

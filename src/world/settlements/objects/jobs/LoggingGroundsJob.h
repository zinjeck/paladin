#pragma once
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include "world/settlements/objects/jobs/WorkplaceDefinition.h"
#include <algorithm>
#include <cmath>

namespace Paladin
{
    inline constexpr WorkplaceDefinition LoggingGroundsWorkplace{
        SettlementObjectTypes::LoggingGrounds,
        1,
        1,
        9,
        50
    };
    // Sustainable throughput: one worker per nine tiles, 24 lumber per
    // uninterrupted twelve-hour shift. Tree animation never enters this math.
    inline double loggingProductionPerMinute(int area, int attendingWorkers)
    {
        return std::min(
                   std::max(0, attendingWorkers) * 1.0,
                   std::max(0, area) / 9.0
               ) /
               30.0;
    }
    inline double loggingTreeGrowth(
        double seconds,
        std::uint64_t seed,
        bool working
    )
    {
        if (!working)
        {
            return 1;
        }
        const double phase = std::fmod(seconds + double(seed % 240) / 10, 24.0);
        // Mature while chopping, then stump, then a rapidly growing sapling.
        return phase < 12 ? 1 : phase < 15 ? .12 : .12 + .88 * (phase - 15) / 9;
    }
} // namespace Paladin

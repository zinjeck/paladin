#pragma once
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include "world/settlements/objects/jobs/WorkplaceDefinition.h"
namespace Paladin
{
    inline constexpr WorkplaceDefinition
        WheatFarmWorkplace{SettlementObjectTypes::WheatFarm, 1, 1, 25, 50};
    struct WheatFarmPolicy
    {
        static constexpr double GrowthMinutes = 3 * 1440.;
        static constexpr double HarvestMinutesPerUnit = 8.;
        static constexpr int GrainPerTile = 1;
    };
    // Production and physical input/output processing live in SettlementIndustry.
} // namespace Paladin

#pragma once
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include "world/settlements/objects/jobs/WorkplaceDefinition.h"
namespace Paladin
{
    inline constexpr WorkplaceDefinition
        StockpileWorkplace{SettlementObjectTypes::Stockpile, 2, 2, 4, 250};
    struct StockpileJobPolicy
    {
        int collectionRadius = 96;
        double employeePreferenceMinutes = 30;
    };
} // namespace Paladin

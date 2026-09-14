#pragma once
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include "world/settlements/objects/jobs/WorkplaceDefinition.h"
namespace Paladin
{
    inline constexpr WorkplaceDefinition
        BakeryWorkplace{SettlementObjectTypes::Bakery, 1, 1, 6, 50};
    // Production and physical input/output processing live in SettlementIndustry.
} // namespace Paladin

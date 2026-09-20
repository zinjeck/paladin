#pragma once
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include "world/settlements/objects/jobs/WorkplaceDefinition.h"
namespace Paladin
{
    // A public export/import yard. Staff bring goods here before a caravan can
    // load; imported cargo remains here until citizens or businesses collect
    // it.
    inline constexpr WorkplaceDefinition
        TradeDepotWorkplace{SettlementObjectTypes::TradeDepot, 1, 1, 25, 300};
} // namespace Paladin

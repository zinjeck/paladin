#pragma once
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include "world/settlements/objects/jobs/WorkplaceDefinition.h"
namespace Paladin
{
    inline constexpr WorkplaceDefinition
        PasturelandWorkplace{SettlementObjectTypes::Pastureland, 1, 1, 12, 50};
    // Contained animals and actual attending handlers determine production.
} // namespace Paladin

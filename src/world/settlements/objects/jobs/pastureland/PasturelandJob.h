#pragma once
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include "world/settlements/objects/jobs/WorkplaceDefinition.h"
namespace Paladin
{
inline constexpr WorkplaceDefinition
    PasturelandWorkplace{SettlementObjectTypes::Pastureland, 1, 1, 12, 50};
// Production behavior is reserved for a later pass; staffing and storage are
// active.
} // namespace Paladin

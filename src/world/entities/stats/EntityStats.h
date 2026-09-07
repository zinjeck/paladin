#pragma once
#include <string>
#include <vector>

namespace Paladin
{
    // Future skills/combat values live here, never in living attributes.
    struct EntityStat
    {
        std::string definitionId;
        double value = 0;
    };
    struct EntityStats
    {
        std::vector<EntityStat> values;
    };
} // namespace Paladin

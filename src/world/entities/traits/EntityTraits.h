#pragma once
#include <string>
#include <vector>

namespace Paladin
{
    // Stable catalog keys for future personality/behavior definitions.
    // No traits or behavioral bonuses are invented in this pass.
    struct EntityTraits
    {
        std::vector<std::string> definitionIds;
    };
} // namespace Paladin

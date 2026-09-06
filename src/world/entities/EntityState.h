#pragma once
#include "core/StrongId.h"
#include "world/SettlementTilePosition.h"
#include <string>

namespace Paladin
{
    // Common living-entity data without virtual dispatch or citizen rules.
    // IDs are settlement-scoped; cross-settlement references also need
    // SettlementId.
    struct EntityState
    {
        EntityId id;
        std::string name;
        SettlementTilePosition tilePosition{-1, -1};
        double health = 100;
        double energy = 100;
        double hunger = 0;
    };
} // namespace Paladin

#pragma once
#include "core/StrongId.h"
#include "world/SettlementTilePosition.h"
#include "world/entities/attributes/AttributeModifiers.h"
#include "world/entities/stats/EntityStats.h"
#include "world/entities/traits/EntityTraits.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace Paladin
{
    // Common living-entity data without virtual dispatch or citizen rules.
    // IDs are settlement-scoped; cross-settlement references also need
    // SettlementId.
    struct EntityState : EntityAttributes
    {
        EntityId id;
        std::string name;
        SettlementTilePosition tilePosition{-1, -1};
        EntityTraits traits;
        EntityStats stats;
        AttributeModifiers attributeModifiers;
        void modifyAttributes(
            std::initializer_list<AttributeModifier> modifiers
        )
        {
            attributeModifiers.apply(*this, modifiers);
        }
        // Previous fixed-step visual position; never used for navigation.
        double previousVisualX = 0, previousVisualY = 0;
        bool hasVisualSnapshot = false;
        void captureVisual(double x, double y)
        {
            previousVisualX = x;
            previousVisualY = y;
            hasVisualSnapshot = tilePosition.x >= 0 && tilePosition.y >= 0;
        }
        double renderX(double current, double alpha) const
        {
            return hasVisualSnapshot ? std::lerp(
                                           previousVisualX,
                                           current,
                                           std::clamp(alpha, 0.0, 1.0)
                                       )
                                     : current;
        }
        double renderY(double current, double alpha) const
        {
            return hasVisualSnapshot ? std::lerp(
                                           previousVisualY,
                                           current,
                                           std::clamp(alpha, 0.0, 1.0)
                                       )
                                     : current;
        }
    };
} // namespace Paladin

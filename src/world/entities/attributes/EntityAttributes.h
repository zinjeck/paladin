#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace Paladin
{
    enum class EntityAttribute : std::uint8_t
    {
        Health,
        Happiness,
        Hunger,
        Energy,
        Count
    };
    inline constexpr std::size_t entityAttributeCount =
        std::size_t(EntityAttribute::Count);

    // Living conditions only. The normalized baseline is 1.00 (wellbeing).
    // Hunger is inverse: baseline 1.00 means 0% hunger. No implicit drift.
    // Percent storage preserves existing simulation units, independently of
    // future personality traits, learned skills and combat statistics.
    struct EntityAttributes
    {
        double health = 100, happiness = 100, hunger = 0, energy = 100;
        double& value(EntityAttribute attribute)
        {
            switch (attribute)
            {
            case EntityAttribute::Health:
                return health;
            case EntityAttribute::Happiness:
                return happiness;
            case EntityAttribute::Hunger:
                return hunger;
            default:
                return energy;
            }
        }
        double value(EntityAttribute attribute) const
        {
            return const_cast<EntityAttributes*>(this)->value(attribute);
        }
        double normalized(EntityAttribute attribute) const
        {
            const double fraction =
                std::clamp(value(attribute) / 100, 0.0, 1.0);
            return attribute == EntityAttribute::Hunger ? 1 - fraction
                                                        : fraction;
        }
    };
} // namespace Paladin

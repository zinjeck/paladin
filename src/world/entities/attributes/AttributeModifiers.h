#pragma once
#include "world/entities/attributes/EntityAttributes.h"
#include <cmath>
#include <initializer_list>
#include <string_view>

namespace Paladin
{
    enum class AttributeEffect : std::uint8_t
    {
        Metabolism,
        Meals,
        ParentFeeding,
        FeedingChildren,
        Awake,
        Labor,
        Sleep,
        Exhaustion,
        Starvation,
        HealthyRecovery,
        CareHealth,
        NeglectHealth,
        Comfort,
        HungerDistress,
        IllHealth,
        Homelessness,
        Unemployment,
        Workday,
        Socializing,
        CareHappiness,
        NeglectHappiness,
        Taxes,
        PublicMeals,
        Overcrowding,
        FoodShortage,
        UnheatedHome,
        WinterCold,
        Count
    };
    struct AttributeEffectDefinition
    {
        EntityAttribute attribute;
        std::string_view label;
    };
    inline constexpr std::array attributeEffectDefinitions{
        AttributeEffectDefinition{EntityAttribute::Hunger, "Metabolism"},
        AttributeEffectDefinition{EntityAttribute::Hunger, "Meals eaten"},
        AttributeEffectDefinition{EntityAttribute::Hunger, "Food from parent"},
        AttributeEffectDefinition{EntityAttribute::Hunger, "Feeding children"},
        AttributeEffectDefinition{EntityAttribute::Energy, "Time awake"},
        AttributeEffectDefinition{EntityAttribute::Energy, "Physical work"},
        AttributeEffectDefinition{EntityAttribute::Energy, "Sleep"},
        AttributeEffectDefinition{EntityAttribute::Health, "Exhaustion"},
        AttributeEffectDefinition{EntityAttribute::Health, "Starvation"},
        AttributeEffectDefinition{
            EntityAttribute::Health,
            "Fed and rested recovery"
        },
        AttributeEffectDefinition{EntityAttribute::Health, "Toddler care"},
        AttributeEffectDefinition{EntityAttribute::Health, "Toddler neglect"},
        AttributeEffectDefinition{
            EntityAttribute::Happiness,
            "Fed and housed comfort"
        },
        AttributeEffectDefinition{EntityAttribute::Happiness, "Hunger"},
        AttributeEffectDefinition{EntityAttribute::Happiness, "Poor health"},
        AttributeEffectDefinition{EntityAttribute::Happiness, "Homelessness"},
        AttributeEffectDefinition{EntityAttribute::Happiness, "Unemployment"},
        AttributeEffectDefinition{EntityAttribute::Happiness, "Workday length"},
        AttributeEffectDefinition{EntityAttribute::Happiness, "Conversation"},
        AttributeEffectDefinition{EntityAttribute::Happiness, "Toddler care"},
        AttributeEffectDefinition{
            EntityAttribute::Happiness,
            "Toddler neglect"
        },
        AttributeEffectDefinition{EntityAttribute::Happiness, "Tax policy"},
        AttributeEffectDefinition{
            EntityAttribute::Happiness,
            "Reliance on public meals"
        },
        AttributeEffectDefinition{
            EntityAttribute::Happiness,
            "Housing pressure"
        },
        AttributeEffectDefinition{
            EntityAttribute::Happiness,
            "Food insecurity"
        },
        AttributeEffectDefinition{
            EntityAttribute::Happiness,
            "Home without firewood"
        },
        AttributeEffectDefinition{EntityAttribute::Health, "Winter cold"}
    };
    inline constexpr std::size_t attributeEffectCount =
        std::size_t(AttributeEffect::Count);
    static_assert(attributeEffectDefinitions.size() == attributeEffectCount);
    using AttributeEffectTotals = std::array<double, attributeEffectCount>;
    struct AttributeModifier
    {
        AttributeEffect source;
        double points;
    };

    // Every applied living-condition change is attributed to its real source.
    // Batches clamp once; opposed effects remain visible without ordering bias
    // at 0/100. Only the part actually applied is included in the report.
    struct AttributeModifiers
    {
        AttributeEffectTotals pending{};
        void apply(
            EntityAttributes& values,
            std::initializer_list<AttributeModifier> modifiers
        )
        {
            std::array<double, entityAttributeCount> positive{}, negative{};
            for (const auto& m : modifiers)
            {
                if (!std::isfinite(m.points))
                {
                    continue;
                }
                const auto a = std::size_t(
                    attributeEffectDefinitions[std::size_t(m.source)].attribute
                );
                (m.points >= 0 ? positive[a] : negative[a]) += m.points;
            }
            std::array<double, entityAttributeCount> positiveScale{},
                negativeScale{};
            for (std::size_t a = 0; a < entityAttributeCount; ++a)
            {
                auto& value = values.value(EntityAttribute(a));
                const double next =
                    std::clamp(value + positive[a] + negative[a], 0.0, 100.0);
                const double clipped = value + positive[a] + negative[a] - next;
                positiveScale[a] =
                    clipped > 0 && positive[a] > 0
                        ? std::clamp(1 - clipped / positive[a], 0.0, 1.0)
                        : 1;
                negativeScale[a] =
                    clipped < 0 && negative[a] < 0
                        ? std::clamp(1 - clipped / negative[a], 0.0, 1.0)
                        : 1;
                value = next;
            }
            for (const auto& m : modifiers)
            {
                if (!std::isfinite(m.points))
                {
                    continue;
                }
                const auto i = std::size_t(m.source);
                const auto a =
                    std::size_t(attributeEffectDefinitions[i].attribute);
                pending[i] += m.points * (m.points >= 0 ? positiveScale[a]
                                                        : negativeScale[a]);
            }
        }
    };
} // namespace Paladin

#include "TestFramework.h"
#include "world/entities/attributes/AttributeModifiers.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace
{
    using namespace Paladin;

    void close(double actual, double expected)
    {
        PALADIN_CHECK(std::isfinite(actual));
        PALADIN_CHECK(std::abs(actual - expected) <= 1e-9);
    }

    void conserved(
        const EntityAttributes& before,
        const EntityAttributes& after,
        const AttributeEffectTotals& previous,
        const AttributeEffectTotals& current
    )
    {
        std::array<double, entityAttributeCount> applied{};
        for (std::size_t i = 0; i < attributeEffectCount; ++i)
        {
            PALADIN_CHECK(std::isfinite(current[i]));
            applied[std::size_t(attributeEffectDefinitions[i].attribute)] +=
                current[i] - previous[i];
        }
        for (std::size_t a = 0; a < entityAttributeCount; ++a)
        {
            const auto attribute = EntityAttribute(a);
            PALADIN_CHECK(after.value(attribute) >= 0);
            PALADIN_CHECK(after.value(attribute) <= 100);
            close(applied[a], after.value(attribute) - before.value(attribute));
        }
    }

    void defaultsAndNormalization()
    {
        EntityAttributes values;
        close(values.health, 100);
        close(values.happiness, 100);
        close(values.hunger, 0);
        close(values.energy, 100);
        for (std::size_t a = 0; a < entityAttributeCount; ++a)
        {
            const auto attribute = EntityAttribute(a);
            close(values.normalized(attribute), 1);
            for (double raw : {-25.0, 0.0, 25.0, 75.0, 100.0, 125.0})
            {
                auto copy = values;
                copy.value(attribute) = raw;
                const EntityAttributes& readOnly = copy;
                const double fraction = std::clamp(raw / 100, 0.0, 1.0);
                close(
                    readOnly.normalized(attribute),
                    attribute == EntityAttribute::Hunger ? 1 - fraction
                                                        : fraction
                );
                // Normalization is a query, not a repair of stored values.
                close(readOnly.value(attribute), raw);
                for (std::size_t other = 0; other < entityAttributeCount; ++other)
                {
                    if (other != a)
                    {
                        close(copy.value(EntityAttribute(other)), values.value(EntityAttribute(other)));
                    }
                }
            }
        }
    }

    void everySourceHasTheExpectedTarget()
    {
        // Independent of the production definition table: catch an enum/table
        // reordering that routes a modifier to the wrong living condition.
        constexpr std::array expected{
            EntityAttribute::Hunger, EntityAttribute::Hunger,
            EntityAttribute::Hunger, EntityAttribute::Hunger,
            EntityAttribute::Energy, EntityAttribute::Energy,
            EntityAttribute::Energy,
            EntityAttribute::Health, EntityAttribute::Health,
            EntityAttribute::Health, EntityAttribute::Health,
            EntityAttribute::Health,
            EntityAttribute::Happiness, EntityAttribute::Happiness,
            EntityAttribute::Happiness, EntityAttribute::Happiness,
            EntityAttribute::Happiness, EntityAttribute::Happiness,
            EntityAttribute::Happiness, EntityAttribute::Happiness,
            EntityAttribute::Happiness, EntityAttribute::Happiness,
            EntityAttribute::Happiness, EntityAttribute::Happiness,
            EntityAttribute::Happiness, EntityAttribute::Happiness,
            EntityAttribute::Health
        };
        static_assert(expected.size() == attributeEffectCount);
        for (std::size_t i = 0; i < attributeEffectCount; ++i)
        {
            PALADIN_CHECK(attributeEffectDefinitions[i].attribute == expected[i]);
            PALADIN_CHECK(!attributeEffectDefinitions[i].label.empty());
            EntityAttributes values{50, 50, 50, 50};
            const auto before = values;
            AttributeModifiers modifiers;
            modifiers.apply(values, {{AttributeEffect(i), 2.5}});
            for (std::size_t a = 0; a < entityAttributeCount; ++a)
            {
                close(values.value(EntityAttribute(a)), EntityAttribute(a) == expected[i] ? 52.5 : 50);
            }
            for (std::size_t j = 0; j < attributeEffectCount; ++j)
            {
                close(modifiers.pending[j], j == i ? 2.5 : 0);
            }
            conserved(before, values, {}, modifiers.pending);
        }
    }

    void cappedAttribution()
    {
        EntityAttributes values;
        values.happiness = 99;
        AttributeModifiers modifiers;
        const auto before = values;
        modifiers.apply(values, {
            {AttributeEffect::Comfort, 6},
            {AttributeEffect::Socializing, 3},
            {AttributeEffect::HungerDistress, -2}
        });
        close(values.happiness, 100);
        close(modifiers.pending[std::size_t(AttributeEffect::Comfort)], 2);
        close(modifiers.pending[std::size_t(AttributeEffect::Socializing)], 1);
        close(modifiers.pending[std::size_t(AttributeEffect::HungerDistress)], -2);
        conserved(before, values, {}, modifiers.pending);

        values.happiness = 1;
        modifiers = {};
        const auto low = values;
        modifiers.apply(values, {
            {AttributeEffect::Comfort, 2},
            {AttributeEffect::HungerDistress, -6},
            {AttributeEffect::IllHealth, -3}
        });
        close(values.happiness, 0);
        close(modifiers.pending[std::size_t(AttributeEffect::Comfort)], 2);
        close(modifiers.pending[std::size_t(AttributeEffect::HungerDistress)], -2);
        close(modifiers.pending[std::size_t(AttributeEffect::IllHealth)], -1);
        conserved(low, values, {}, modifiers.pending);

        for (double boundary : {0.0, 100.0})
        {
            values.happiness = boundary;
            modifiers = {};
            modifiers.apply(values, {
                {AttributeEffect::Comfort, 5},
                {AttributeEffect::HungerDistress, -5}
            });
            close(values.happiness, boundary);
            close(modifiers.pending[std::size_t(AttributeEffect::Comfort)], 5);
            close(modifiers.pending[std::size_t(AttributeEffect::HungerDistress)], -5);
        }
    }

    void batchOrderingDoesNotChangeAttribution()
    {
        constexpr std::array effects{
            AttributeModifier{AttributeEffect::Comfort, 10},
            AttributeModifier{AttributeEffect::HungerDistress, -8},
            AttributeModifier{AttributeEffect::Socializing, 6},
            AttributeModifier{AttributeEffect::IllHealth, -4}
        };
        for (double start : {0.0, 50.0, 99.0, 100.0})
        {
            EntityAttributes expected;
            expected.happiness = start;
            AttributeModifiers reference;
            reference.apply(expected, {effects[0], effects[1], effects[2], effects[3]});
            std::array order{0, 1, 2, 3};
            do
            {
                EntityAttributes actual;
                actual.happiness = start;
                AttributeModifiers modifiers;
                const auto before = actual;
                modifiers.apply(actual, {
                    effects[order[0]], effects[order[1]],
                    effects[order[2]], effects[order[3]]
                });
                close(actual.happiness, expected.happiness);
                for (std::size_t i = 0; i < attributeEffectCount; ++i)
                {
                    close(modifiers.pending[i], reference.pending[i]);
                }
                conserved(before, actual, {}, modifiers.pending);
            } while (std::next_permutation(order.begin(), order.end()));
        }
    }

    void accumulationAndInvalidPoints()
    {
        EntityAttributes values{40, 50, 60, 70};
        AttributeModifiers modifiers;
        const auto before = values;
        modifiers.apply(values, {
            {AttributeEffect::HealthyRecovery, 3},
            {AttributeEffect::Starvation, -1},
            {AttributeEffect::Comfort, 6},
            {AttributeEffect::Comfort, 4},
            {AttributeEffect::HungerDistress, -3},
            {AttributeEffect::Metabolism, 5},
            {AttributeEffect::Meals, -15},
            {AttributeEffect::Sleep, 8},
            {AttributeEffect::Awake, -4}
        });
        close(values.health, 42);
        close(values.happiness, 57);
        close(values.hunger, 50);
        close(values.energy, 74);
        close(modifiers.pending[std::size_t(AttributeEffect::Comfort)], 10);
        conserved(before, values, {}, modifiers.pending);

        const auto afterFirstBatch = values;
        const auto firstPending = modifiers.pending;
        modifiers.apply(values, {{AttributeEffect::Meals, -100}});
        close(values.hunger, 0);
        close(modifiers.pending[std::size_t(AttributeEffect::Meals)], -65);
        conserved(afterFirstBatch, values, firstPending, modifiers.pending);
        conserved(before, values, {}, modifiers.pending);

        const auto valid = values;
        const auto validPending = modifiers.pending;
        modifiers.apply(values, {
            {AttributeEffect::Comfort, std::numeric_limits<double>::quiet_NaN()},
            {AttributeEffect::Meals, std::numeric_limits<double>::infinity()},
            {AttributeEffect::Awake, -std::numeric_limits<double>::infinity()},
            {AttributeEffect::HealthyRecovery, 0}
        });
        modifiers.apply(values, {});
        for (std::size_t a = 0; a < entityAttributeCount; ++a)
        {
            close(values.value(EntityAttribute(a)), valid.value(EntityAttribute(a)));
        }
        PALADIN_CHECK(modifiers.pending == validPending);
    }

    void boundedNumericSweep()
    {
        constexpr std::array positive{
            AttributeEffect::HealthyRecovery, AttributeEffect::Comfort,
            AttributeEffect::Metabolism, AttributeEffect::Sleep
        };
        constexpr std::array negative{
            AttributeEffect::Starvation, AttributeEffect::HungerDistress,
            AttributeEffect::Meals, AttributeEffect::Awake
        };
        for (std::size_t a = 0; a < entityAttributeCount; ++a)
        {
            for (double start : {0.0, .001, 1.0, 50.0, 99.0, 99.999, 100.0})
            {
                for (double gain : {0.0, .01, 1.0, 25.0, 100.0, 250.0})
                {
                    for (double loss : {0.0, .01, 1.0, 25.0, 100.0, 250.0})
                    {
                        EntityAttributes values{50, 50, 50, 50};
                        values.value(EntityAttribute(a)) = start;
                        const auto before = values;
                        AttributeModifiers modifiers;
                        modifiers.apply(values, {{positive[a], gain}, {negative[a], -loss}});
                        close(values.value(EntityAttribute(a)), std::clamp(start + gain - loss, 0.0, 100.0));
                        conserved(before, values, {}, modifiers.pending);
                        const double appliedGain = modifiers.pending[std::size_t(positive[a])];
                        const double appliedLoss = modifiers.pending[std::size_t(negative[a])];
                        PALADIN_CHECK(appliedGain >= -1e-9 && appliedGain <= gain + 1e-9);
                        PALADIN_CHECK(appliedLoss <= 1e-9 && appliedLoss >= -loss - 1e-9);
                    }
                }
            }
        }
    }
} // namespace

void runEntityAttributeTests()
{
    defaultsAndNormalization();
    everySourceHasTheExpectedTarget();
    cappedAttribution();
    batchOrderingDoesNotChangeAttribution();
    accumulationAndInvalidPoints();
    boundedNumericSweep();
}

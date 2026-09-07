#include "world/settlements/citizens/SettlementCitizenState.h"
#include <iomanip>
#include <sstream>

namespace Paladin
{
    void SettlementAttributeReport::record(
        double minute,
        double elapsed,
        std::size_t livingCount,
        const AttributeEffectTotals& changes
    )
    {
        const auto hour = std::int64_t(std::floor(minute / 60));
        if (!history_.empty() && hour < history_.back().hour)
        {
            history_.clear();
        }
        while (!history_.empty() && history_.front().hour <= hour - 24)
        {
            history_.pop_front();
        }
        if (history_.empty() || history_.back().hour != hour)
        {
            history_.push_back({hour});
        }
        auto& bucket = history_.back();
        bucket.residentMinutes += livingCount * elapsed;
        bucket.elapsed += elapsed;
        for (std::size_t i = 0; i < changes.size(); ++i)
        {
            bucket.changes[i] += changes[i];
        }
        if (elapsed > 0)
        {
            activeChanges_ = changes;
            activeResidentMinutes_ = livingCount * elapsed;
        }
    }

    std::string SettlementAttributeReport::tooltip(
        EntityAttribute attribute,
        const EntityAttributes& values
    ) const
    {
        constexpr std::array names{"Health", "Happiness", "Hunger", "Energy"};
        std::ostringstream out;
        if (attribute == EntityAttribute::Happiness)
        {
            out << "Happiness";
            bool any = false;
            constexpr std::array ceilingSources{
                AttributeEffect::PublicMeals,
                AttributeEffect::Taxes,
                AttributeEffect::Overcrowding,
                AttributeEffect::FoodShortage
            };
            for (std::size_t i = 0; i < attributeEffectDefinitions.size(); ++i)
            {
                if (attributeEffectDefinitions[i].attribute != attribute)
                {
                    continue;
                }
                const auto source = std::find(
                    ceilingSources.begin(),
                    ceilingSources.end(),
                    AttributeEffect(i)
                );
                const double penalty = source == ceilingSources.end()
                                           ? 0
                                           : happinessPenalties[std::size_t(
                                                 source - ceilingSources.begin()
                                             )];
                if (penalty >= .05)
                {
                    out << "\n"
                        << attributeEffectDefinitions[i].label << ": -"
                        << std::fixed << std::setprecision(1) << penalty
                        << "% maximum";
                    any = true;
                    continue;
                }
                const double rate =
                    activeResidentMinutes_ > 0
                        ? activeChanges_[i] * 1440 / activeResidentMinutes_
                        : 0;
                if (std::abs(rate) < .05)
                {
                    continue;
                }
                out << "\n"
                    << attributeEffectDefinitions[i].label << ": " << std::fixed
                    << std::setprecision(1) << std::showpos << rate
                    << std::noshowpos << "/day";
                any = true;
            }
            if (!any)
            {
                out << "\nNo active modifiers";
            }
            return out.str();
        }
        out << names[std::size_t(attribute)] << " | Base wellbeing: 1.00\n";
        out << std::fixed << std::setprecision(2)
            << "Current wellbeing: " << values.normalized(attribute);
        out
            << (attribute == EntityAttribute::Hunger ? " (1.00 = 0% hunger)\n"
                                                     : " (1.00 = 100%)\n");
        AttributeEffectTotals totals{};
        double exposure = 0, minutes = 0;
        for (const auto& bucket : history_)
        {
            exposure += bucket.residentMinutes;
            minutes += bucket.elapsed;
            for (std::size_t i = 0; i < totals.size(); ++i)
            {
                totals[i] += bucket.changes[i];
            }
        }
        out << "Applied changes / resident, recent " << std::setprecision(1)
            << minutes / 60 << "h (max 24h):\n";
        const double residents = minutes > 0 ? exposure / minutes : 0;
        for (std::size_t i = 0; i < totals.size(); ++i)
        {
            if (attributeEffectDefinitions[i].attribute != attribute)
            {
                continue;
            }
            const double points = residents > 0 ? totals[i] / residents : 0;
            out << attributeEffectDefinitions[i].label << ": " << std::showpos
                << std::setprecision(2)
                << (std::abs(points) < .005 ? 0.0 : points) << std::noshowpos
                << " pp\n";
        }
        out
            << (attribute == EntityAttribute::Hunger
                    ? "Negative changes mean less hunger."
                    : "Positive changes improve this attribute.");
        out << "\nNo base drift; effects stop at 0% / 100%.";
        return out.str();
    }

    EntityAttributes SettlementCitizenState::averageAttributes() const
    {
        EntityAttributes sum{0, 0, 0, 0};
        std::size_t living = 0;
        for (const auto& citizen : citizens_)
        {
            if (citizen.health <= 1e-7)
            {
                continue;
            }
            for (std::size_t a = 0; a < entityAttributeCount; ++a)
            {
                sum.value(EntityAttribute(a)) +=
                    citizen.value(EntityAttribute(a));
            }
            ++living;
        }
        if (!living)
        {
            return {};
        }
        for (std::size_t a = 0; a < entityAttributeCount; ++a)
        {
            sum.value(EntityAttribute(a)) /= living;
        }
        return sum;
    }

    void SettlementCitizenState::recordAttributes(double minute, double elapsed)
    {
        AttributeEffectTotals changes{};
        std::size_t living = 0;
        std::array<double, 4> penalties{};
        EntityAttributes sum{0, 0, 0, 0};
        for (auto& c : citizens_)
        {
            living += c.health > 1e-7;
            if (c.health > 1e-7)
            {
                sum.health += c.health;
                sum.happiness += c.happiness;
                sum.hunger += c.hunger;
                sum.energy += c.energy;
                penalties[0] += c.publicFoodDissatisfaction;
                penalties[1] += std::max(0.0, -c.taxHappinessAdjustment);
                penalties[2] += c.housingHappinessPenalty;
                penalties[3] += c.foodHappinessPenalty;
            }
            for (std::size_t i = 0; i < changes.size(); ++i)
            {
                changes[i] += c.attributeModifiers.pending[i];
                c.attributeModifiers.pending[i] = 0;
            }
        }
        attributeReport_.record(minute, elapsed, living, changes);
        for (std::size_t a = 0; a < entityAttributeCount; ++a)
        {
            if (living)
            {
                sum.value(EntityAttribute(a)) /= living;
            }
        }
        attributeReport_.average = living ? sum : EntityAttributes{};
        attributeReport_.living = living;
        for (std::size_t i = 0; i < penalties.size(); ++i)
        {
            attributeReport_.happinessPenalties[i] =
                living ? penalties[i] / living : 0;
        }
    }

    bool SettlementCitizenState::spawnImmigrants(std::uint64_t count)
    {
        const auto average = averageAttributes();
        const auto first = citizens_.size();
        if (!appendCitizens(count, false))
        {
            return false;
        }
        for (auto i = first; i < citizens_.size(); ++i)
        {
            // Deliberately copy only these four values. New identity, sex,
            // adulthood, empty job/relationships/traits/stats are independent.
            static_cast<EntityAttributes&>(citizens_[i]) = average;
        }
        attributeReport_.average = averageAttributes();
        attributeReport_.living = citizens_.size();
        return true;
    }

    void SettlementCitizen::enforceHappinessModifiers()
    {
        double ceiling = 100;
        const auto cap = [&](AttributeEffect source, double penalty)
        {
            ceiling = std::max(0.0, ceiling - std::max(0.0, penalty));
            modifyAttributes({{source, std::min(0.0, ceiling - happiness)}});
        };
        cap(AttributeEffect::PublicMeals, publicFoodDissatisfaction);
        cap(AttributeEffect::Taxes, -taxHappinessAdjustment);
        cap(AttributeEffect::Overcrowding, housingHappinessPenalty);
        cap(AttributeEffect::FoodShortage, foodHappinessPenalty);
    }
} // namespace Paladin

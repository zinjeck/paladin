#include "world/settlements/SettlementImmigration.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Paladin
{
    bool ImmigrationPolicy::isValid() const noexcept
    {
        for (const double value :
             {minimumHappiness,
              idealHappiness,
              minimumFoodDays,
              idealFoodDays,
              baseApplicantsPerDay,
              applicantsPerResidentPerDay,
              waitingPoolDays,
              applicantDepartureDays,
              assessmentMinutes,
              housingHappinessPenalty,
              foodHappinessPenalty,
              happinessAdjustmentPerDay})
        {
            if (!std::isfinite(value) || value < 0)
            {
                return false;
            }
        }
        return idealHappiness > minimumHappiness && idealHappiness <= 100 &&
               idealFoodDays > minimumFoodDays && waitingPoolDays > 0 &&
               applicantDepartureDays > 0 && assessmentMinutes > 0;
    }

    void SettlementImmigration::assess(
        const SettlementMap& map,
        const SettlementCitizenState& citizens
    )
    {
        conditions_ = {};
        if (!map.logistics.founded() || !policy.isValid())
        {
            return;
        }
        double happiness = 0;
        std::size_t unhoused = 0;
        const auto& needs = map.activities.policy;
        for (const auto& c : citizens.citizens())
        {
            if (c.health <= 1e-7)
            {
                continue;
            }
            ++conditions_.residents;
            happiness += c.happiness;
            unhoused += !c.homeId;
            const double share = !c.child ? 1
                                 : c.ageYears < needs.independentEatingAge
                                     ? needs.nursingFoodShare
                                     : needs.dependentFoodShare;
            conditions_.foodPerDay += needs.hungerPerDay /
                                      std::max(1.0, needs.mealRestoration) *
                                      share;
        }
        for (const auto& inventory : map.logistics.inventories())
        {
            // Reserves, not loose piles, construction deliveries or products
            // still awaiting collection. Empty extra stores do not help.
            if (inventory.kind != InventoryKind::Keep &&
                inventory.kind != InventoryKind::Stockpile)
            {
                continue;
            }
            for (const auto& goods : inventory.goods)
            {
                const auto* definition =
                    SettlementResourceCatalog::definition(goods.resource);
                if (definition && definition->edible)
                {
                    conditions_.storedFood += goods.amount;
                }
            }
        }
        if (!conditions_.residents)
        {
            return;
        }
        conditions_.happiness = happiness / conditions_.residents;
        conditions_.unhousedShare = double(unhoused) / conditions_.residents;
        conditions_.foodDays =
            conditions_.foodPerDay > 0
                ? conditions_.storedFood / conditions_.foodPerDay
                : policy.idealFoodDays;
        const auto ramp = [](double value, double minimum, double ideal)
        { return std::clamp((value - minimum) / (ideal - minimum), 0.0, 1.0); };
        conditions_.attraction = ramp(
                                     conditions_.happiness,
                                     policy.minimumHappiness,
                                     policy.idealHappiness
                                 ) *
                                 ramp(
                                     conditions_.foodDays,
                                     policy.minimumFoodDays,
                                     policy.idealFoodDays
                                 );
        conditions_.applicantsPerDay =
            conditions_.attraction *
            (policy.baseApplicantsPerDay +
             conditions_.residents * policy.applicantsPerResidentPerDay);
    }

    void SettlementImmigration::advance(
        const SettlementMap& map,
        const SettlementCitizenState& citizens,
        double minute,
        double elapsed
    )
    {
        if (!std::isfinite(minute) || !std::isfinite(elapsed) || elapsed <= 0 ||
            !policy.isValid())
        {
            return;
        }
        // Hourly assessments, never a per-frame scan of tiles/citizens/stores.
        while (elapsed > 1e-9)
        {
            if (untilAssessment_ <= 1e-9)
            {
                assess(map, citizens);
                untilAssessment_ = policy.assessmentMinutes;
            }
            const double dt = std::min(elapsed, untilAssessment_);
            const double target =
                conditions_.applicantsPerDay * policy.waitingPoolDays;
            if (applicants_ <= target)
            {
                applicants_ = std::min(
                    target,
                    applicants_ + conditions_.applicantsPerDay * dt / 1440
                );
            }
            else
            {
                applicants_ =
                    target +
                    (applicants_ - target) *
                        std::exp(-dt / (policy.applicantDepartureDays * 1440));
            }
            untilAssessment_ -= dt;
            elapsed -= dt;
        }
    }

    std::uint64_t SettlementImmigration::available() const noexcept
    {
        // Bounded admissions also obey the existing citizen creation limit.
        return std::uint64_t(
            std::clamp(std::floor(applicants_ + 1e-9), 0.0, 100000.0)
        );
    }

    bool SettlementImmigration::admit(
        SettlementMap& map,
        SettlementCitizenState& citizens,
        std::uint64_t count,
        double minute
    )
    {
        if (!count || count > available() || !map.logistics.founded() ||
            !policy.isValid())
        {
            return false;
        }
        // A destroyed keep must not create stranded, unpositioned residents.
        bool keepExists = false;
        for (const auto& inventory : map.logistics.inventories())
        {
            keepExists |= inventory.kind == InventoryKind::Keep &&
                          map.objectState().completedObject(inventory.objectId);
        }
        if (!keepExists || !citizens.spawnImmigrants(count))
        {
            return false;
        }
        applicants_ = std::max(0.0, applicants_ - count);
        citizens.placeUnpositionedCitizens(map);
        // Count real housing vacancies immediately, not an hour of temporary
        // homelessness while newcomers wait for the next AI update.
        map.activities.synchronizeHomes(map, citizens);
        citizens.recordPopulation(minute);
        assess(map, citizens);
        untilAssessment_ = policy.assessmentMinutes;
        return true;
    }
} // namespace Paladin

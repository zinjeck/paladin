#pragma once
#include "simulation/systems/SettlementActivitySystem.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include <algorithm>

namespace Paladin
{
    // Average meal-equivalents required by the existing feeding model.
    // A child's share is paid through a parent's hunger, not a second full
    // adult meal. Keep reserves and UI forecasts on this same policy.
    inline double citizenFoodPerDay(const SettlementCitizen& citizen,
                                   const CitizenSimulationPolicy& policy)
    {
        if (citizen.health <= 1e-7) { return 0; }
        const double share = !citizen.child ? 1.0
            : citizen.ageYears < policy.independentEatingAge
                ? policy.nursingFoodShare : policy.dependentFoodShare;
        return std::max(0.0, policy.hungerPerDay) /
               std::max(1.0, policy.mealRestoration) * std::max(0.0, share);
    }
} // namespace Paladin

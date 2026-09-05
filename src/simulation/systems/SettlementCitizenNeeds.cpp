#include "simulation/systems/SettlementActivitySystem.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace Paladin
{
void SettlementActivitySystem::needs(SettlementCitizen& c, double elapsed)
{
    const double days = elapsed / 1440;
    const double before = c.hunger;
    c.hunger = std::min(100.0, before + policy.hungerPerDay * days);
    // Integral of a linear severity curve: 75 -> 100 hunger takes half a
    // day, over which a healthy citizen loses exactly 100 health.
    const auto primitive = [&](double hunger)
    {
        const double above = std::max(0.0, hunger - policy.starvationThreshold);
        return above * above / (2 * (100 - policy.starvationThreshold));
    };
    const double risingDays = (c.hunger - before) / policy.hungerPerDay;
    const double damage =
        400 * ((primitive(c.hunger) - primitive(before)) / policy.hungerPerDay +
               std::max(0.0, days - risingDays));
    c.health = std::max(0.0, c.health - damage);
    if (c.hunger < policy.foodSeekThreshold)
    {
        c.health =
            std::min(100.0, c.health + policy.healthRecoveryPerDay * days);
    }
    if (c.homeId)
    {
        c.homelessMinutes = 0;
    }
    else
    {
        c.homelessMinutes += elapsed;
    }
    const double hungerPressure = std::max(0.0, (c.hunger - 25) / 75) * 12;
    const double healthPressure = (100 - c.health) / 100 * 24;
    const double homelessPressure =
        c.homeId ? 0
                 : std::min(16.0, .5 + std::pow(c.homelessMinutes / 1440, 2));
    const double recovery =
        c.hunger < 50 && c.homeId ? policy.happinessRecoveryPerDay : 0;
    c.happiness = std::clamp(
        c.happiness + days * (recovery - hungerPressure - healthPressure -
                              homelessPressure),
        0.0,
        100.0
    );
}
void SettlementActivitySystem::assignHomes(
    const SettlementMap& map,
    SettlementCitizenState& citizens
)
{
    if (housingTopology_ == map.objectState().navigationVersion() &&
        housedPopulation_ == citizens.citizens_.size())
    {
        return;
    }
    housingTopology_ = map.objectState().navigationVersion();
    housedPopulation_ = citizens.citizens_.size();
    std::unordered_map<SettlementObjectId, int, StrongIdHash> occupants;
    for (auto& c : citizens.citizens_)
    {
        const auto* home = map.objectState().completedObject(c.homeId);
        if (!home || home->objectTypeId != SettlementObjectTypes::House)
        {
            c.homeId = {};
        }
        else
        {
            ++occupants[c.homeId];
        }
    }
    for (const auto& object : map.objectState().completedObjects())
    {
        if (object.objectTypeId != SettlementObjectTypes::House)
        {
            continue;
        }
        for (auto& c : citizens.citizens_)
        {
            if (occupants[object.id] >= 4)
            {
                break;
            }
            if (!c.homeId)
            {
                c.homeId = object.id;
                c.homelessMinutes = 0;
                ++occupants[object.id];
            }
        }
    }
}
const char* SettlementActivitySystem::activityLabel(const SettlementCitizen& c)
{
    if (c.hunger > 75)
    {
        return "Starving";
    }
    switch (c.task.kind)
    {
    case CitizenTaskKind::Eat:
        return "Finding food";
    case CitizenTaskKind::Haul:
        return c.task.delivering ? "Delivering" : "Collecting goods";
    case CitizenTaskKind::Gather:
        return "Gathering";
    case CitizenTaskKind::Demolish:
        return "Demolishing";
    case CitizenTaskKind::Build:
        return "Constructing";
    case CitizenTaskKind::Work:
        return c.path.empty() ? "Working" : "Going to work";
    case CitizenTaskKind::Home:
        return c.path.empty() ? "At home" : "Going home";
    default:
        return "Idle";
    }
}
} // namespace Paladin

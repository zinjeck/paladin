#include "simulation/systems/SettlementActivitySystem.h"
#include "world/Season.h"
#include "world/generation/GenerationNoise.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include "world/settlements/objects/SettlementDoor.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace Paladin
{
void SettlementCitizenState::recordPopulation(double minute)
{
    if (!std::isfinite(minute))
    {
        return;
    }
    // Record on the simulation clock, even when no panel is visible.
    const double boundary = std::floor(minute / 240) * 240;
    if (populationHistory_.empty())
    {
        populationHistory_.push_back({minute, citizens_.size()});
    }
    else if (boundary > populationHistory_.back().gameMinute)
    {
        populationHistory_.push_back({boundary, citizens_.size()});
    }
    while (populationHistory_.size() > 97)
    {
        populationHistory_.pop_front();
    }
}

void SettlementActivitySystem::planMeal(
    SettlementCitizen& c,
    const SettlementMap& map
)
{
    const auto random = GenerationNoise::mix(
        c.id.value() ^ map.instanceId() ^ GenerationNoise::mix(++c.mealSequence)
    );
    const double fraction = double(random >> 11) / 9007199254740992.0;
    c.foodSeekHunger =
        policy.foodSeekThreshold +
        fraction * (policy.urgentFoodThreshold - policy.foodSeekThreshold);
}
void SettlementActivitySystem::planSleep(
    SettlementMap& map,
    SettlementCitizen& c,
    double minute
)
{
    const auto cycle = std::int64_t(std::floor((minute + 720) / 1440));
    if (cycle == c.sleepCycle)
        return;
    c.sleepCycle = cycle;
    c.sleptMinutes = 0;
    const double day = double(cycle - 1) * 1440;
    const auto& season =
        seasonDefinition(seasonAtMinute(std::max(0.0, day + 720)));
    double earliest = day + season.sunsetMinute;
    double latest =
        day + 1440 + season.sunriseMinute - policy.requiredSleepMinutes;
    if (c.workplaceId && policy.shiftEndMinute > policy.shiftStartMinute)
    {
        earliest = std::max(earliest, day + policy.shiftEndMinute);
        latest = std::min(
            latest,
            day + 1440 + policy.shiftStartMinute - policy.requiredSleepMinutes
        );
    }
    latest = std::max(earliest, latest);
    const auto random = GenerationNoise::mix(
        c.id.value() ^ map.instanceId() ^
        GenerationNoise::mix(std::uint64_t(cycle))
    );
    c.sleepStartMinute =
        earliest + (latest - earliest) * double(random % 10001) / 10000;
}
bool SettlementActivitySystem::shouldSleep(
    const SettlementCitizen& c,
    double minute
) const
{
    if (!c.homeId || c.sleptMinutes >= policy.requiredSleepMinutes ||
        (c.workplaceId && policy.isWorkTime(minute)))
        return false;
    return minute >= c.sleepStartMinute;
}
bool SettlementActivitySystem::enterHome(
    SettlementMap& map,
    SettlementCitizen& c
)
{
    const auto* home = map.objectState().completedObject(c.homeId);
    if (!home || home->objectTypeId != SettlementObjectTypes::House)
        return false;
    if (c.insideHome)
        return home->footprint.contains(c.tilePosition);
    if (!home->door)
        return false;
    const auto entrance = outsideDoor(home->footprint, *home->door);
    if (c.tilePosition != entrance)
        return false;
    c.homeEntrance = entrance;
    const auto interior = *home->door;
    c.insideHome = interior == c.tilePosition;
    c.destination = interior;
    if (interior != c.tilePosition)
    {
        c.path = {interior};
        c.pathIndex = 0;
        c.stepProgress = 0;
        c.explicitMovement = true;
        return false;
    }
    return true;
}
bool SettlementActivitySystem::chooseSleep(
    SettlementMap& map,
    SettlementCitizenState& citizens,
    SettlementCitizen& c,
    double minute
)
{
    if (!c.homeId || !shouldSleep(c, minute))
        return false;
    if (c.task.kind == CitizenTaskKind::Sleep)
        return true;
    if (c.insideHome && c.task.object == c.homeId)
    {
        c.path.clear();
        c.pathIndex = 0;
        c.stepProgress = 0;
        c.destination = c.tilePosition;
        c.task = {};
        c.task.kind = CitizenTaskKind::Sleep;
        c.task.object = c.homeId;
        c.activity = CitizenActivity::Sleeping;
        return true;
    }
    if (c.carriedAmount > 0 || c.task.kind == CitizenTaskKind::Eat)
        return false;
    auto planned = c;
    bool atHome = false;
    if (const auto* home = map.objectState().completedObject(c.homeId))
    {
        atHome = route(map, citizens, planned, home->footprint, false);
        if (!atHome && routeBudgetLimited_)
            return false;
    }
    if (!atHome)
        return false;
    finish(map, c, minute);
    if (atHome)
    {
        c.path = std::move(planned.path);
        c.pathIndex = planned.pathIndex;
        c.stepProgress = planned.stepProgress;
        c.stepDuration = planned.stepDuration;
        c.destination = planned.destination;
        c.explicitMovement = planned.explicitMovement;
    }
    else
        c.destination = c.tilePosition;
    c.task.kind = CitizenTaskKind::Sleep;
    c.task.object = c.homeId;
    c.task.startedMinute = minute;
    c.activity = CitizenActivity::ReturningHome;
    return true;
}

void SettlementActivitySystem::needs(SettlementCitizen& c, double elapsed)
{
    const double days = elapsed / 1440;
    const double before = c.hunger;
    c.hunger = std::min(100.0, before + policy.hungerPerDay * days);
    // Scale damage with depletion: reaching 100 from 75 costs 100 health.
    const auto primitive = [&](double hunger)
    {
        const double above = std::max(0.0, hunger - policy.starvationThreshold);
        return above * above / (2 * (100 - policy.starvationThreshold));
    };
    const double risingDays = (c.hunger - before) / policy.hungerPerDay;
    const double damage =
        (8 * policy.hungerPerDay) *
        ((primitive(c.hunger) - primitive(before)) / policy.hungerPerDay +
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
    const int workHours =
        (policy.shiftEndMinute - policy.shiftStartMinute) / 60;
    const double workdayEffect =
        c.workplaceId ? std::clamp(12.0 - workHours, -2.0, 3.0) : 0;
    c.happiness = std::clamp(
        c.happiness + days * (recovery + workdayEffect - hungerPressure -
                              healthPressure - homelessPressure),
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
        else if (occupants[c.homeId] < 4)
        {
            ++occupants[c.homeId];
        }
        else
            c.homeId = {};
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
std::string SettlementActivitySystem::activityLabel(const SettlementCitizen& c)
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
        return c.path.empty()
                   ? (c.activity == CitizenActivity::Fishing ? "Fishing"
                                                             : "Working")
                   : "Going to work";
    case CitizenTaskKind::Break:
        return c.breakReturning ? "Returning from break" : "On break";
    case CitizenTaskKind::Talk:
        return "Talking to " + c.task.partnerName;
    case CitizenTaskKind::Sleep:
        return c.insideHome && c.path.empty() ? "Sleeping" : "Going to sleep";
    case CitizenTaskKind::Home:
        return c.insideHome ? "At home" : "Going home";
    default:
        return "Idle";
    }
}
} // namespace Paladin

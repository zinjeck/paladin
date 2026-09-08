#include "simulation/systems/SettlementActivitySystem.h"
#include "world/Season.h"
#include "world/generation/GenerationNoise.h"
#include "world/settlements/SettlementHomeBeds.h"
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
            c.id.value() ^ map.instanceId() ^
            GenerationNoise::mix(++c.mealSequence)
        );
        const double fraction = double(random >> 11) / 9007199254740992.0;
        c.foodSeekHunger =
            policy.foodSeekThreshold +
            fraction * (policy.urgentFoodThreshold - policy.foodSeekThreshold);
    }
    void SettlementActivitySystem::planSleep(
        SettlementMap& map,
        SettlementCitizen& c,
        double /*minute*/
    )
    {
        if (c.restThreshold > 0)
        {
            return;
        }
        const auto random =
            GenerationNoise::mix(c.id.value() ^ map.instanceId());
        c.restThreshold = policy.fatigueEnergy - double(random % 501) / 100;
    }
    bool SettlementActivitySystem::shouldSleep(
        const SettlementCitizen& c,
        double minute
    ) const
    {
        const bool shift = c.workplaceId && policy.isWorkTime(minute);
        if (c.task.kind == CitizenTaskKind::Sleep)
        {
            return c.energy < policy.fullRestEnergy &&
                   (!shift || c.energy < policy.fatigueEnergy);
        }
        // Forecasts and leisure preferences may never initiate nearly-full
        // rest. Only an already sleeping citizen can continue recovering
        // above 50.
        if (c.energy >= policy.fatigueEnergy)
        {
            return false;
        }
        if (c.energy <= policy.criticalRestEnergy)
        {
            return true;
        }
        if (shift)
        {
            return false;
        }
        const auto& season = seasonDefinition(seasonAtMinute(minute));
        const double time = std::fmod(minute, 1440.0);
        const double recoveryMinutes =
            (policy.fullRestEnergy - c.energy) / policy.sleepEnergyPerMinute;
        if (c.workplaceId && policy.shiftEndMinute > policy.shiftStartMinute)
        {
            const double workMinutes =
                policy.shiftEndMinute - policy.shiftStartMinute;
            const double offMinutes = 1440 - workMinutes;
            const double sinceShiftEnd =
                std::fmod(time - policy.shiftEndMinute + 1440, 1440.0);
            const double untilShift = offMinutes - sinceShiftEnd;
            // Allocate available leisure before bed, without delaying needed
            // recovery. Changing the workday or energy rates changes this
            // window.
            const double leisure =
                std::max(
                    0.0,
                    offMinutes -
                        std::max(policy.requiredSleepMinutes, recoveryMinutes)
                ) *
                policy.postWorkLeisureShare;
            if (sinceShiftEnd < leisure && c.energy > policy.fatigueEnergy)
            {
                return false;
            }
            const double nextShiftCost =
                policy.awakeEnergyPerMinute * (untilShift + workMinutes) +
                policy.workEnergyPerMinute * workMinutes;
            if (untilShift <=
                    std::max(policy.requiredSleepMinutes, recoveryMinutes) &&
                c.energy - nextShiftCost < policy.fatigueEnergy)
            {
                return true;
            }
        }
        const bool night =
            time < season.sunriseMinute || time >= season.sunsetMinute;
        return c.energy <=
               (night ? c.restThreshold : policy.fatigueEnergy - 10);
    }
    bool SettlementActivitySystem::enterHome(
        SettlementMap& map,
        const SettlementCitizenState& citizens,
        SettlementCitizen& c
    )
    {
        const auto* home = map.objectState().completedObject(c.homeId);
        if (!home || home->objectTypeId != SettlementObjectTypes::House)
        {
            return false;
        }
        if (c.insideHome)
        {
            return home->footprint.contains(c.tilePosition);
        }
        if (!home->door)
        {
            return false;
        }
        const auto entrance = outsideDoor(home->footprint, *home->door);
        if (c.tilePosition != entrance)
        {
            return false;
        }
        c.homeEntrance = entrance;
        const auto interior = *home->door;
        c.insideHome = interior == c.tilePosition;
        c.destination = interior;
        if (interior != c.tilePosition)
        {
            c.path = {interior};
            c.pathIndex = 0;
            c.stepProgress = 0;
            c.stepDuration = citizens.navigation_.stepCost(
                map, c.tilePosition, interior, citizens.movementPolicy
            );
            c.explicitMovement = true;
            return false;
        }
        return true;
    }
    SettlementTilePosition SettlementActivitySystem::sleepingPosition(
        const SettlementMap& map,
        const SettlementCitizenState& citizens,
        const SettlementCitizen& c
    ) const
    {
        const auto* home = map.objectState().completedObject(c.homeId);
        if (!home)
        {
            return {-1, -1};
        }
        if (c.bedHomeId == home->id && c.bedSlot >= 0 && c.bedSlot < 4)
        {
            return homeBedPosition(*home, c.bedSlot);
        }
        auto best = c.tilePosition;
        int bestScore = -1000000;
        const auto& f = home->footprint;
        for (int y = f.topLeft.y; y < f.topLeft.y + f.height; ++y)
        {
            for (int x = f.topLeft.x; x < f.topLeft.x + f.width; ++x)
            {
                const SettlementTilePosition p{x, y};
                int reserved = 0, occupied = 0;
                for (const auto& other : citizens.citizens())
                {
                    if (other.id == c.id)
                    {
                        continue;
                    }
                    occupied += other.tilePosition == p;
                    reserved += other.task.kind == CitizenTaskKind::Sleep &&
                                other.homeId == c.homeId &&
                                other.task.target == p;
                }
                // Reservations dominate temporary occupancy. Prefer clear
                // interior tiles, leaving the doorway free. Sharing is a last
                // resort only.
                int score = -reserved * 10000 - occupied * 1000;
                if (home->door)
                {
                    score += (std::abs(x - home->door->x) +
                              std::abs(y - home->door->y)) *
                             10;
                }
                if (p == c.tilePosition)
                {
                    ++score;
                }
                if (score > bestScore)
                {
                    best = p;
                    bestScore = score;
                }
            }
        }
        return best;
    }
    bool SettlementActivitySystem::chooseSleep(
        SettlementMap& map,
        SettlementCitizenState& citizens,
        SettlementCitizen& c,
        double minute
    )
    {
        if (!shouldSleep(c, minute))
        {
            return false;
        }
        if (c.task.kind == CitizenTaskKind::Sleep)
        {
            return true;
        }
        if (c.insideHome && !c.path.empty())
        {
            return false;
        }
        if (c.insideHome)
        {
            const auto* home = map.objectState().completedObject(c.homeId);
            if (home && home->footprint.contains(c.tilePosition) &&
                c.carriedAmount == 0 && c.task.kind != CitizenTaskKind::Eat)
            {
                finish(map, c, minute);
                c.destination = c.tilePosition;
                c.sleptMinutes = 0;
                c.task.kind = CitizenTaskKind::Sleep;
                c.task.object = c.homeId;
                c.task.target = sleepingPosition(map, citizens, c);
                c.activity = CitizenActivity::ReturningHome;
                return true;
            }
        }
        if (c.carriedAmount > 0 || c.task.kind == CitizenTaskKind::Eat)
        {
            return false;
        }
        auto planned = c;
        bool atHome = false;
        if (const auto* home = map.objectState().completedObject(c.homeId))
        {
            atHome = route(map, citizens, planned, home->footprint, false);
            if (!atHome && routeBudgetLimited_)
            {
                return false;
            }
        }
        // A housed resident waits for a route to their bed; only homeless
        // citizens use the existing outdoor sleeping fallback.
        if (!atHome && map.objectState().completedObject(c.homeId))
        {
            return false;
        }
        if (!atHome)
        {
            // Bounded local search. Reserve a distinct, reachable outdoor spot.
            bool found = false;
            for (int radius = 0; radius <= 4 && !found; ++radius)
            {
                for (int y = -radius; y <= radius && !found; ++y)
                {
                    for (int x = -radius; x <= radius && !found; ++x)
                    {
                        if (std::max(std::abs(x), std::abs(y)) != radius)
                        {
                            continue;
                        }
                        const SettlementTilePosition target{
                            c.tilePosition.x + x,
                            c.tilePosition.y + y
                        };
                        if (!citizens.navigation_.walkable(map, target))
                        {
                            continue;
                        }
                        const bool occupied = std::any_of(
                            citizens.citizens().begin(),
                            citizens.citizens().end(),
                            [&](const auto& other)
                            {
                                return other.id != c.id &&
                                       (other.tilePosition == target ||
                                        (other.task.kind ==
                                             CitizenTaskKind::Sleep &&
                                         other.task.target == target));
                            }
                        );
                        if (occupied)
                        {
                            continue;
                        }
                        planned = c;
                        found =
                            route(map, citizens, planned, {target, 1, 1}, true);
                        if (!found && routeBudgetLimited_)
                        {
                            return false;
                        }
                    }
                }
            }
            if (!found)
            {
                // Sharing is permitted only when the citizen has no way off
                // this single tile. Crowding alone does not justify stacking.
                bool confined =
                    citizens.navigation_.walkable(map, c.tilePosition);
                for (int y = -1; y <= 1; ++y)
                {
                    for (int x = -1; x <= 1; ++x)
                    {
                        if ((x || y) &&
                            citizens.navigation_.walkable(
                                map,
                                {c.tilePosition.x + x, c.tilePosition.y + y}
                            ))
                        {
                            confined = false;
                        }
                    }
                }
                if (!confined)
                {
                    return false;
                }
                planned = c;
                planned.path.clear();
                planned.pathIndex = 0;
                planned.stepProgress = 0;
                planned.destination = c.tilePosition;
            }
        }
        finish(map, c, minute);
        {
            c.path = std::move(planned.path);
            c.pathIndex = planned.pathIndex;
            c.stepProgress = planned.stepProgress;
            c.stepDuration = planned.stepDuration;
            c.destination = planned.destination;
            c.explicitMovement = planned.explicitMovement;
        }
        c.sleptMinutes = 0;
        c.task.kind = CitizenTaskKind::Sleep;
        c.task.object = atHome ? c.homeId : SettlementObjectId{};
        c.task.target =
            atHome ? sleepingPosition(map, citizens, c) : c.destination;
        c.task.startedMinute = minute;
        c.activity = CitizenActivity::ReturningHome;
        return true;
    }

    void SettlementActivitySystem::needs(
        const SettlementMap& map,
        SettlementCitizen& c,
        double elapsed,
        double minute
    )
    {
        const double days = elapsed / 1440;
        const double cold = c.homeId ? map.heating.coldFraction(c.homeId) : 1;
        const bool winter = seasonAtMinute(minute) == Season::Winter;
        c.modifyAttributes(
            {{AttributeEffect::UnheatedHome, c.homeId ? -36 * days * cold : 0},
             {AttributeEffect::WinterCold, winter ? -18 * days * cold : 0}}
        );
        const bool sleeping = c.task.kind == CitizenTaskKind::Sleep &&
                              c.activity == CitizenActivity::Sleeping &&
                              c.path.empty();
        if (!sleeping)
        {
            const bool labor = c.task.kind == CitizenTaskKind::AnimalWork ||
                               c.task.kind == CitizenTaskKind::Haul ||
                               c.task.kind == CitizenTaskKind::Gather ||
                               c.task.kind == CitizenTaskKind::Demolish ||
                               c.task.kind == CitizenTaskKind::Build ||
                               (c.task.kind == CitizenTaskKind::Work &&
                                policy.isWorkTime(minute));
            c.modifyAttributes(
                {{AttributeEffect::Awake,
                  -policy.awakeEnergyPerMinute * elapsed},
                 {AttributeEffect::Labor,
                  labor ? -policy.workEnergyPerMinute * elapsed : 0}}
            );
        }
        c.modifyAttributes(
            {{AttributeEffect::Exhaustion,
              -policy.fatigueHealthPerDay * days *
                  std::max(
                      0.0,
                      (policy.fatigueEnergy - c.energy) / policy.fatigueEnergy
                  )}}
        );
        const double before = c.hunger;
        c.modifyAttributes(
            {{AttributeEffect::Metabolism, policy.hungerPerDay * days}}
        );
        // Scale damage with depletion: reaching 100 from 75 costs 100 health.
        const auto primitive = [&](double hunger)
        {
            const double above =
                std::max(0.0, hunger - policy.starvationThreshold);
            return above * above / (2 * (100 - policy.starvationThreshold));
        };
        const double depletion = std::max(1e-9, policy.hungerPerDay);
        const double risingDays = (c.hunger - before) / depletion;
        const double damage =
            (8 * policy.hungerPerDay) *
            ((primitive(c.hunger) - primitive(before)) / depletion +
             std::max(0.0, days - risingDays));
        c.modifyAttributes({{AttributeEffect::Starvation, -damage}});
        if ((!c.child || c.ageYears >= policy.independentEatingAge) &&
            c.hunger < policy.foodSeekThreshold &&
            c.energy >= policy.fatigueEnergy)
        {
            c.modifyAttributes(
                {{AttributeEffect::HealthyRecovery,
                  policy.healthRecoveryPerDay * days * (winter ? 1 - cold : 1)}}
            );
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
            c.homeId
                ? 0
                : std::min(16.0, .5 + std::pow(c.homelessMinutes / 1440, 2));
        const double recovery =
            c.hunger < 50 && c.homeId ? policy.happinessRecoveryPerDay : 0;
        const int workHours =
            (policy.shiftEndMinute - policy.shiftStartMinute) / 60;
        const double unemploymentPressure =
            (c.child || c.workplaceId) ? 0 : policy.unemploymentHappinessPerDay;
        const double workdayEffect =
            c.workplaceId ? std::clamp(12.0 - workHours, -2.0, 3.0) : 0;
        c.modifyAttributes(
            {{AttributeEffect::Comfort, recovery * days},
             {AttributeEffect::Workday, workdayEffect * days},
             {AttributeEffect::HungerDistress, -hungerPressure * days},
             {AttributeEffect::IllHealth, -healthPressure * days},
             {AttributeEffect::Homelessness, -homelessPressure * days},
             {AttributeEffect::Unemployment, -unemploymentPressure * days}}
        );
        c.enforceHappinessModifiers();
    }
    std::string SettlementActivitySystem::activityLabel(
        const SettlementCitizen& c
    )
    {
        if (c.hunger > 75)
        {
            return "Starving";
        }
        switch (c.task.kind)
        {
        case CitizenTaskKind::Eat:
            return "Finding food";
        case CitizenTaskKind::AnimalWork:
            return c.task.delivering ? "Herding livestock"
                                     : "Working with animal";
        case CitizenTaskKind::FamilyMeal:
            return c.child ? "Going to parent for food" : "Feeding child";
        case CitizenTaskKind::Haul:
            return c.task.delivering ? "Delivering" : "Collecting goods";
        case CitizenTaskKind::Gather:
            return "Gathering";
        case CitizenTaskKind::Demolish:
            return "Demolishing";
        case CitizenTaskKind::Build:
            return "Constructing";
        case CitizenTaskKind::Work:
            if (c.task.animal)
            {
                return c.path.empty() ? "Tending livestock"
                                      : "Going to livestock";
            }
            return c.path.empty()
                       ? (c.activity == CitizenActivity::Fishing ? "Fishing"
                                                                 : "Working")
                       : "Going to work";
        case CitizenTaskKind::Break:
            return c.breakReturning ? "Returning from break" : "On break";
        case CitizenTaskKind::Talk:
            return (c.path.empty() ? (c.child ? "Playing with " : "Talking to ")
                                   : "Meeting ") +
                   c.task.partnerName;
        case CitizenTaskKind::Sleep:
            return c.activity == CitizenActivity::Sleeping && c.path.empty()
                       ? "Sleeping"
                       : "Going to sleep";
        case CitizenTaskKind::Home:
        case CitizenTaskKind::Care:
            if (c.task.kind == CitizenTaskKind::Care)
            {
                if (!c.path.empty() && !c.insideHome)
                {
                    return "Going home";
                }
                return c.child ? "At home" : "Taking care of child";
            }
            if (c.task.endMinute > 0)
            {
                return c.child ? (c.path.empty() ? "Playing nearby"
                                                 : "Walking nearby")
                               : (c.path.empty() ? "Idling nearby"
                                                 : "Walking nearby");
            }
            if (!c.path.empty())
            {
                return "Going home";
            }
            return c.insideHome ? "At home" : "Waiting to go home";
        default:
            return "Idle";
        }
    }
} // namespace Paladin

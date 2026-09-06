#include "simulation/systems/SettlementActivitySystem.h"
#include "world/generation/GenerationNoise.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include "world/settlements/objects/SettlementDoor.h"
#include <algorithm>
#include <cmath>

namespace Paladin
{
    namespace
    {
        void useRoute(SettlementCitizen& c, const SettlementCitizen& planned)
        {
            c.path = planned.path;
            c.pathIndex = planned.pathIndex;
            c.stepProgress = planned.stepProgress;
            c.stepDuration = planned.stepDuration;
            c.destination = planned.destination;
            c.explicitMovement = planned.explicitMovement;
        }
        int separation(SettlementTilePosition a, SettlementTilePosition b)
        {
            return std::abs(a.x - b.x) + std::abs(a.y - b.y);
        }
    } // namespace
    double SettlementActivitySystem::routeMinutes(
        const SettlementMap& map,
        const SettlementCitizenState& citizens,
        const SettlementCitizen& planned
    ) const
    {
        double cost = 0;
        auto from = planned.tilePosition;
        for (std::size_t i = planned.pathIndex; i < planned.path.size(); ++i)
        {
            cost += citizens.navigation_.stepCost(
                map,
                from,
                planned.path[i],
                citizens.movementPolicy
            );
            from = planned.path[i];
        }
        return std::max(0.0, cost - planned.stepProgress) /
               citizens.movementPolicy.tilesPerGameMinute;
    }
    void SettlementActivitySystem::planBreak(
        SettlementMap& map,
        SettlementCitizen& c,
        double minute
    )
    {
        const auto day = std::int64_t(std::floor(minute / 1440));
        if (c.breakDay != day)
        {
            c.breakDay = day;
            c.breakTaken = false;
            c.breakUntil = 0;
            c.breakEmployer = {};
            c.breakObject = {};
        }
        if (c.breakEmployer != c.workplaceId && !c.breakTaken)
        {
            c.breakEmployer = c.workplaceId;
            const double span = std::max(
                0.0,
                policy.shiftEndMinute - policy.shiftStartMinute -
                    policy.workBreakMinutes - 20
            );
            const auto random = GenerationNoise::mix(
                c.id.value() ^ map.instanceId() ^ std::uint64_t(day)
            );
            c.breakDue = day * 1440 + policy.shiftStartMinute + 10 +
                         double(random % 10001) / 10000 * span;
        }
        if (c.breakUntil > 0 &&
            (c.workplaceId != c.breakEmployer || !policy.isWorkTime(minute)))
        {
            c.breakUntil = 0;
        }
    }
    void SettlementActivitySystem::startBreak(
        SettlementMap& map,
        SettlementCitizen& c,
        double minute
    )
    {
        if (c.breakTaken || !c.workplaceId || minute < c.breakDue ||
            !policy.isWorkTime(minute) || c.carriedAmount > 0 ||
            !c.path.empty() || c.task.kind != CitizenTaskKind::Work)
        {
            return;
        }
        if (std::fmod(minute, 1440.0) + policy.workBreakMinutes >
            policy.shiftEndMinute)
        {
            return;
        }
        const auto* job = map.employment().workplace(c.workplaceId);
        if (!job || !job->operational)
        {
            return;
        }
        const auto object = job->objectId;
        finish(map, c, minute);
        c.breakTaken = true;
        c.breakUntil = minute + policy.workBreakMinutes;
        c.breakEmployer = c.workplaceId;
        c.breakObject = object;
        c.breakAnchor = c.tilePosition;
        c.breakReturning = false;
        c.nextBreakWander = minute;
    }
    bool SettlementActivitySystem::breakTripFits(
        SettlementMap& map,
        SettlementCitizenState& citizens,
        const SettlementCitizen& c,
        const SettlementCitizen& planned,
        double minute,
        double stay
    )
    {
        if (c.breakUntil <= minute)
        {
            return false;
        }
        auto returning = planned;
        returning.tilePosition = planned.destination;
        returning.path.clear();
        returning.pathIndex = 0;
        returning.stepProgress = 0;
        if (!route(map, citizens, returning, {c.breakAnchor, 1, 1}, true) ||
            returning.destination != c.breakAnchor)
        {
            return false;
        }
        return routeMinutes(map, citizens, planned) +
                   routeMinutes(map, citizens, returning) + stay + 1 <=
               c.breakUntil - minute;
    }
    bool SettlementActivitySystem::manageBreak(
        SettlementMap& map,
        SettlementCitizenState& citizens,
        SettlementCitizen& c,
        double minute
    )
    {
        if (c.breakUntil <= 0)
        {
            return false;
        }
        if (c.breakReturning)
        {
            if (c.path.empty() && c.tilePosition == c.breakAnchor &&
                minute >= c.breakUntil)
            {
                c.breakUntil = 0;
                finish(map, c, minute);
                return false;
            }
            return true;
        }
        auto returning = c;
        if (!route(map, citizens, returning, {c.breakAnchor, 1, 1}, true))
        {
            return true;
        }
        const double returnTime = routeMinutes(map, citizens, returning);
        if (minute + returnTime + 1 >= c.breakUntil)
        {
            finish(map, c, minute);
            useRoute(c, returning);
            c.breakReturning = true;
            c.task.kind = CitizenTaskKind::Break;
            c.activity = CitizenActivity::ReturningHome;
            return true;
        }
        if (c.task.kind == CitizenTaskKind::Eat ||
            c.task.kind == CitizenTaskKind::Talk)
        {
            return true;
        }
        if (c.path.empty() && chooseSocial(map, citizens, c, minute))
        {
            return true;
        }
        if (c.task.kind == CitizenTaskKind::None)
        {
            c.task.kind = CitizenTaskKind::Break;
            c.destination = c.tilePosition;
        }
        c.activity = CitizenActivity::OnBreak;
        if (!c.path.empty() || minute < c.nextBreakWander)
        {
            return true;
        }
        c.nextBreakWander = minute + 4;
        const auto random =
            GenerationNoise::mix(c.id.value() ^ ++c.choiceSequence);
        const SettlementTilePosition target{
            c.breakAnchor.x + int(random % 5) - 2,
            c.breakAnchor.y + int((random >> 8) % 5) - 2
        };
        auto planned = c;
        if (route(map, citizens, planned, {target, 1, 1}, true) &&
            breakTripFits(map, citizens, c, planned, minute, 3))
        {
            useRoute(c, planned);
        }
        return true;
    }
    bool SettlementActivitySystem::chooseSocial(
        SettlementMap& map,
        SettlementCitizenState& citizens,
        SettlementCitizen& c,
        double minute
    )
    {
        const auto available = [&](const SettlementCitizen& person)
        {
            if (person.health <= 0 || person.hunger >= person.foodSeekHunger ||
                person.youngDependents > 0 ||
                (person.child &&
                 person.ageYears < policy.independentEatingAge) ||
                !person.path.empty() || person.carriedAmount ||
                minute < person.nextSocialMinute)
            {
                return false;
            }
            if (shouldSleep(person, minute))
            {
                return false;
            }
            if (person.workplaceId && policy.isWorkTime(minute) &&
                person.breakUntil <= minute)
            {
                return false;
            }
            if (!person.child && !person.workplaceId &&
                policy.isWorkTime(minute) &&
                (!map.commandState().commands().empty() ||
                 !map.objectState().constructionSites().empty()))
            {
                return false;
            }
            return person.task.kind == CitizenTaskKind::None ||
                   person.task.kind == CitizenTaskKind::Home ||
                   person.task.kind == CitizenTaskKind::Break;
        };
        if (!available(c))
        {
            return false;
        }
        c.nextSocialMinute = minute + 5;
        std::vector<std::size_t> candidates;
        for (std::size_t i = 0; i < citizens.citizens_.size(); ++i)
        {
            if (citizens.citizens_[i].id == c.spouseId)
            {
                candidates.insert(candidates.begin(), i);
            }
            else if (
                candidates.size() < 64 && separation(
                                              c.tilePosition,
                                              citizens.citizens_[i].tilePosition
                                          ) <= policy.leisureRadius
            )
            {
                candidates.push_back(i);
            }
        }
        for (auto index : candidates)
        {
            auto& other = citizens.citizens_[index];
            if (other.id == c.id || !available(other) ||
                separation(c.tilePosition, other.tilePosition) >
                    policy.leisureRadius * 2)
            {
                continue;
            }
            auto planned = c;
            auto meeting = other.tilePosition;
            auto waitingPlan = other;
            if (other.insideHome)
            {
                const auto* home =
                    map.objectState().completedObject(other.homeId);
                if (!home || !home->door)
                {
                    continue;
                }
                meeting = outsideDoor(home->footprint, *home->door);
                if (!route(map, citizens, waitingPlan, {meeting, 1, 1}, true))
                {
                    continue;
                }
            }
            if (!route(map, citizens, planned, {meeting, 1, 1}, false))
            {
                continue;
            }
            const double travel = routeMinutes(map, citizens, planned);
            double duration =
                2 + GenerationNoise::mix(
                        c.id.value() ^ other.id.value() ^ ++c.choiceSequence
                    ) % 14;
            if (c.breakUntil > 0)
            {
                while (
                    duration >= 2 &&
                    !breakTripFits(map, citizens, c, planned, minute, duration))
                {
                    --duration;
                }
            }
            if (other.breakUntil > 0)
            {
                auto waiting = other;
                while (duration >= 2 && !breakTripFits(
                                            map,
                                            citizens,
                                            other,
                                            waiting,
                                            minute,
                                            duration + travel
                                        ))
                {
                    --duration;
                }
            }
            if (duration < 2 || travel > 8)
            {
                continue;
            }
            finish(map, c, minute);
            finish(map, other, minute);
            useRoute(c, planned);
            useRoute(other, waitingPlan);
            c.task.kind = other.task.kind = CitizenTaskKind::Talk;
            c.task.partner = other.id;
            other.task.partner = c.id;
            c.task.partnerName = other.name;
            other.task.partnerName = c.name;
            c.task.startedMinute = other.task.startedMinute = minute;
            c.task.laborMinutes = other.task.laborMinutes = duration;
            other.destination = waitingPlan.destination;
            c.nextSocialMinute = other.nextSocialMinute =
                minute + duration + 15;
            c.activity = other.activity = CitizenActivity::Talking;
            return true;
        }
        return false;
    }
} // namespace Paladin

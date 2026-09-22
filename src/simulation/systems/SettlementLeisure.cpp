#include "simulation/systems/SettlementActivitySystem.h"
#include "world/generation/GenerationNoise.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include "world/settlements/objects/SettlementDoor.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace Paladin
{
    namespace
    {
        int separation(SettlementTilePosition a, SettlementTilePosition b)
        {
            return std::abs(a.x - b.x) + std::abs(a.y - b.y);
        }
    } // namespace
    double SettlementActivitySystem::routeMinutes(
        const SettlementMap& map,
        const SettlementCitizenState& citizens,
        const CitizenRoutePlan& planned
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
        const auto day = std::int64_t(std::floor(
            (minute + policy.solarTimeOffsetMinutes) / 1440));
        if (c.breakDay != day)
        {
            c.breakDay = day;
            c.breakTaken = false;
            c.breakUntil = 0;
            c.breakEmployer = {};
            c.breakObject = {};
        }
        if (pastureWorkerAvailableForGeneralLabor(map, c))
        {
            // An empty-pasture assignment behaves as unemployed general labor,
            // so it does not create a workplace shift break.
            c.breakUntil = 0;
            c.breakEmployer = {};
            c.breakObject = {};
            return;
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
            c.breakDue = day * 1440 - policy.solarTimeOffsetMinutes +
                         policy.shiftStartMinute + 10 +
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
        if (c.breakTaken || !c.workplaceId ||
            pastureWorkerAvailableForGeneralLabor(map, c) ||
            minute < c.breakDue || !policy.isWorkTime(minute) ||
            c.carriedAmount > 0 || !c.path.empty() ||
            c.task.kind != CitizenTaskKind::Work)
        {
            return;
        }
        if (policy.localMinute(minute) + policy.workBreakMinutes >
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
        const CitizenRoutePlan& planned,
        double minute,
        double stay
    )
    {
        if (c.breakUntil <= minute)
        {
            return false;
        }
        auto returning = planned.fromPosition(planned.destination);
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
        CitizenRoutePlan returning(c);
        if (!route(map, citizens, returning, {c.breakAnchor, 1, 1}, true))
        {
            return true;
        }
        const double returnTime = routeMinutes(map, citizens, returning);
        if (minute + returnTime + 1 >= c.breakUntil)
        {
            finish(map, c, minute);
            returning.applyTo(c);
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
        CitizenRoutePlan planned(c);
        if (route(map, citizens, planned, {target, 1, 1}, true) &&
            breakTripFits(map, citizens, c, planned, minute, 3))
        {
            planned.applyTo(c);
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
            const bool generalLabor =
                !person.workplaceId ||
                pastureWorkerAvailableForGeneralLabor(map, person);
            if (!generalLabor && policy.isWorkTime(minute) &&
                person.breakUntil <= minute)
            {
                return false;
            }
            if (!person.child && generalLabor && policy.isWorkTime(minute) &&
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
        struct SocialCandidate
        {
            std::size_t index;
            double score;
            bool spouse;
        };
        std::array<SocialCandidate, 65> candidates;
        std::size_t count = 0;
        bool spouseIncluded = false;
        const auto append = [&](std::size_t index, bool spouse, int distance)
        {
            candidates[count++] = {
                index,
                distance - policy.familiarityPreference *
                               c.familiarityWith(citizens.citizens_[index].id) -
                    (spouse ? 4.0 : 0.0),
                spouse
            };
            spouseIncluded |= spouse;
        };
        for (std::size_t i = 0; i < citizens.citizens_.size() && count < 64; ++i)
        {
            const auto& person = citizens.citizens_[i];
            const bool spouse = person.id == c.spouseId;
            const int distance = separation(c.tilePosition, person.tilePosition);
            if (spouse || distance <= policy.leisureRadius)
            {
                append(i, spouse, distance);
            }
        }
        // Preserve the existing extra spouse candidate after the 64-person
        // limit without continuing through the rest of a large population.
        if (!spouseIncluded)
        {
            if (const auto* spouse = citizens.citizen(c.spouseId))
            {
                append(std::size_t(spouse - citizens.citizens_.data()), true,
                       separation(c.tilePosition, spouse->tilePosition));
            }
        }
        std::sort(
            candidates.begin(),
            candidates.begin() + count,
            [](const auto& a, const auto& b)
            {
                if (a.score != b.score) { return a.score < b.score; }
                if (a.spouse != b.spouse) { return a.spouse; }
                return a.index < b.index;
            }
        );
        for (const auto& candidate : std::span(candidates).first(count))
        {
            auto& other = citizens.citizens_[candidate.index];
            if (other.id == c.id || other.child != c.child ||
                !available(other) ||
                separation(c.tilePosition, other.tilePosition) >
                    policy.leisureRadius * 2)
            {
                continue;
            }
            CitizenRoutePlan planned(c);
            auto meeting = other.tilePosition;
            CitizenRoutePlan waitingPlan(other);
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
            if (!childRouteIsLocal(map, c, planned) ||
                !childRouteIsLocal(map, other, waitingPlan) ||
                !inChildNeighborhood(map, other, planned.destination))
            {
                continue;
            }
            const double travel = routeMinutes(map, citizens, planned);
            double duration =
                2 + double(GenerationNoise::mix(
                        c.id.value() ^ other.id.value() ^ ++c.choiceSequence
                    ) % 14);
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
                CitizenRoutePlan waiting(other);
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
            planned.applyTo(c);
            waitingPlan.applyTo(other);
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

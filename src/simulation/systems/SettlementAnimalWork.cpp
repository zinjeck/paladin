#include "simulation/systems/SettlementActivitySystem.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include <algorithm>
#include <cmath>

namespace Paladin
{
    namespace
    {
        void adoptRoute(SettlementCitizen& citizen, SettlementCitizen& planned)
        {
            citizen.path = std::move(planned.path);
            citizen.pathIndex = planned.pathIndex;
            citizen.stepProgress = planned.stepProgress;
            citizen.stepDuration = planned.stepDuration;
            citizen.destination = planned.destination;
            citizen.explicitMovement = planned.explicitMovement;
        }
    } // namespace
    bool SettlementActivitySystem::chooseAnimalWork(
        SettlementMap& map,
        SettlementCitizenState& citizens,
        SettlementCitizen& c,
        double minute
    )
    {
        if (!map.animals.hasOrders())
        {
            return false;
        }
        std::vector<EntityId> candidates;
        for (const auto& a : map.animals.all())
        {
            if (a.health > 0 && !a.handler && a.order != AnimalOrder::None)
            {
                candidates.push_back(a.id);
            }
        }
        std::stable_sort(
            candidates.begin(),
            candidates.end(),
            [&](auto a, auto b)
            {
                const auto distance = [&](EntityId id)
                {
                    const auto p = map.animals.find(id)->tilePosition;
                    return std::abs(p.x - c.tilePosition.x) +
                           std::abs(p.y - c.tilePosition.y);
                };
                return distance(a) < distance(b);
            }
        );
        for (const auto id : candidates)
        {
            const auto* animal = map.animals.find(id);
            std::vector<SettlementObjectId> pastures;
            if (animal->order == AnimalOrder::Gather)
            {
                for (const auto& object : map.objectState().completedObjects())
                {
                    if (object.objectTypeId ==
                            SettlementObjectTypes::Pastureland &&
                        map.animals.capacity(object.footprint) >=
                            map.animals.usedSpace(object.id) +
                                animalSpecies(animal->species)->pastureSpace)
                    {
                        pastures.push_back(object.id);
                    }
                }
                if (pastures.empty())
                {
                    continue;
                }
            }
            auto planned = c;
            if (!route(
                    map,
                    citizens,
                    planned,
                    {animal->tilePosition, 1, 1},
                    true
                ))
            {
                if (routeBudgetLimited_)
                {
                    return false;
                }
                continue;
            }
            if (planned.destination != animal->tilePosition)
            {
                continue;
            }
            // Prove a route to an actual pasture before claiming the animal.
            SettlementObjectId pasture;
            for (const auto candidate : pastures)
            {
                auto delivery = planned;
                delivery.tilePosition = animal->tilePosition;
                delivery.insideHome = false;
                delivery.path.clear();
                if (route(
                        map,
                        citizens,
                        delivery,
                        map.objectState().completedObject(candidate)->footprint,
                        true
                    ))
                {
                    pasture = candidate;
                    break;
                }
                if (routeBudgetLimited_)
                {
                    return false;
                }
            }
            if (animal->order == AnimalOrder::Gather && !pasture)
            {
                continue;
            }
            finish(map, c, minute);
            if (!map.animals.reserve(id, c.id, pasture, map))
            {
                continue;
            }
            adoptRoute(c, planned);
            c.task.kind = CitizenTaskKind::AnimalWork;
            c.task.animal = id;
            c.task.startedMinute = minute;
            c.task.object = pasture;
            c.activity = CitizenActivity::TravelingToWork;
            return true;
        }
        return false;
    }
    void SettlementActivitySystem::executeAnimalWork(
        SettlementMap& map,
        SettlementCitizenState& citizens,
        SettlementCitizen& c,
        double minute,
        double elapsed
    )
    {
        const auto* animal = map.animals.find(c.task.animal);
        if (!animal || animal->handler != c.id || animal->health <= 0 ||
            animal->tilePosition != c.tilePosition)
        {
            finish(map, c, minute);
            return;
        }
        if (animal->order == AnimalOrder::Hunt)
        {
            c.task.laborMinutes += elapsed;
            if (c.task.laborMinutes >= map.animals.policy.huntMinutes)
            {
                map.animals.hunt(c.task.animal, c.id, map, minute);
                finish(map, c, minute);
            }
            return;
        }
        if (c.task.delivering)
        {
            map.animals.contain(c.task.animal, c.id, map);
            finish(map, c, minute);
            return;
        }
        const auto* pasture = map.objectState().completedObject(c.task.object);
        if (!pasture)
        {
            finish(map, c, minute);
            return;
        }
        auto planned = c;
        if (!route(map, citizens, planned, pasture->footprint, true))
        {
            if (!routeBudgetLimited_)
            {
                finish(map, c, minute);
            }
            return;
        }
        adoptRoute(c, planned);
        c.task.delivering = true;
    }
    bool SettlementActivitySystem::choosePastureWork(
        SettlementMap& map,
        SettlementCitizenState& citizens,
        SettlementCitizen& c,
        double minute
    )
    {
        const auto* job = map.employment().workplace(c.workplaceId);
        if (!job || !job->operational)
        {
            return false;
        }
        std::vector<EntityId> candidates;
        for (const auto& a : map.animals.all())
        {
            if (a.health > 0 && a.pasture == job->objectId && !a.handler &&
                a.order == AnimalOrder::None &&
                (!a.tender || a.tender == c.id) &&
                minute - a.lastTendedMinute >=
                    map.animals.policy.tendingCooldownMinutes)
            {
                candidates.push_back(a.id);
            }
        }
        std::stable_sort(
            candidates.begin(),
            candidates.end(),
            [&](auto left, auto right)
            {
                const auto score = [&](EntityId id)
                {
                    const auto& a = *map.animals.find(id);
                    return a.lastTendedMinute +
                           .1 * (std::abs(a.tilePosition.x - c.tilePosition.x) +
                                 std::abs(a.tilePosition.y - c.tilePosition.y));
                };
                return score(left) < score(right);
            }
        );
        for (const auto id : candidates)
        {
            auto* a = map.animals.find(id);
            constexpr SettlementTilePosition
                neighbors[]{{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (const auto d : neighbors)
            {
                const SettlementTilePosition target{
                    a->tilePosition.x + d.x,
                    a->tilePosition.y + d.y
                };
                if (!job->footprint.contains(target))
                {
                    continue;
                }
                if (std::any_of(
                        map.animals.all().begin(),
                        map.animals.all().end(),
                        [&](const auto& other)
                        {
                            return other.health > 0 &&
                                   other.tilePosition == target;
                        }
                    ))
                {
                    continue;
                }
                auto planned = c;
                if (!route(map, citizens, planned, {target, 1, 1}, true) ||
                    planned.destination != target)
                {
                    if (routeBudgetLimited_)
                    {
                        return false;
                    }
                    continue;
                }
                map.animals.release(c.id);
                adoptRoute(c, planned);
                c.task = {};
                c.task.kind = CitizenTaskKind::Work;
                c.task.object = job->objectId;
                c.task.animal = id;
                c.task.workTile = target;
                c.task.startedMinute = minute;
                a->tender = c.id;
                a->tendingReservedUntil =
                    minute + map.animals.policy.tendingReservationMinutes;
                c.activity = CitizenActivity::TravelingToWork;
                return true;
            }
        }
        return false;
    }

    void SettlementActivitySystem::executePastureWork(
        SettlementMap& map,
        SettlementCitizenState& citizens,
        SettlementCitizen& c,
        double minute,
        double elapsed
    )
    {
        auto* animal = map.animals.find(c.task.animal);
        if (!animal || animal->health <= 0 || animal->tender != c.id ||
            animal->pasture != c.task.object ||
            std::abs(animal->tilePosition.x - c.tilePosition.x) +
                    std::abs(animal->tilePosition.y - c.tilePosition.y) !=
                1)
        {
            finish(map, c, minute);
            return;
        }
        c.task.laborMinutes += elapsed;
        animal->lastTendedMinute = minute;
        animal->tendingReservedUntil =
            minute + map.animals.policy.tendingReservationMinutes;
        if (c.task.laborMinutes >= map.animals.policy.tendingMinutes)
        {
            if (!choosePastureWork(map, citizens, c, minute))
            {
                finish(map, c, minute);
            }
        }
    }
} // namespace Paladin

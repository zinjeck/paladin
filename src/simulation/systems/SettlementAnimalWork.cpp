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
                        std::int64_t(object.footprint.width) *
                                object.footprint.height >=
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
} // namespace Paladin

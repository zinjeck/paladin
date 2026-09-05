#include "simulation/systems/SettlementActivitySystem.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace Paladin
{
namespace
{
bool workingHours(double minute, const CitizenSimulationPolicy& policy)
{
    const double time = std::fmod(minute, 1440.0);
    return time >= policy.shiftStartMinute && time < policy.shiftEndMinute;
}
} // namespace
void SettlementActivitySystem::finish(
    SettlementMap& map,
    SettlementCitizen& c,
    double minute
)
{
    // Cancellation, dismissal, starvation interruption and death all use
    // the same physical exit. Cargo is never erased with its old task.
    if (c.carriedAmount > 0)
    {
        map.logistics
            .drop(c.tilePosition, c.carriedResource, c.carriedAmount, minute);
    }
    map.logistics.release(c.id);
    c.carriedAmount = 0;
    c.carriedResource.clear();
    c.task = {};
    c.activity = CitizenActivity::Idle;
    c.assignedCommandId = {};
    c.path.clear();
    c.pathIndex = 0;
    c.stepProgress = 0;
    c.explicitMovement = false;
    c.idleWait = -1;
    c.nextDecisionMinute = 0;
}
void SettlementActivitySystem::tick(
    SettlementMap& map,
    SettlementCitizenState& citizens,
    double minute,
    double elapsed
)
{
    if (!std::isfinite(elapsed) || elapsed <= 0)
    {
        return;
    }
    map.logistics.synchronize(map.objectState(), minute);
    if (!map.logistics.founded())
    {
        return;
    }
    citizens.placeUnpositionedCitizens(map);
    // Equal substeps preserve needs, schedules and physical deliveries in
    // every presented/unpresented settlement. Presentation never runs AI.
    while (elapsed > 1e-9)
    {
        const double untilMinute = 1 - (minute - std::floor(minute));
        const double dt = std::min(elapsed, std::max(1e-6, untilMinute));
        step(map, citizens, minute, dt);
        minute += dt;
        elapsed -= dt;
    }
}
void SettlementActivitySystem::step(
    SettlementMap& map,
    SettlementCitizenState& citizens,
    double minute,
    double elapsed
)
{
    map.logistics.synchronize(map.objectState(), minute);
    map.employment().synchronize(map.objectState(), citizens);
    assignHomes(map, citizens);
    map.commandState().pruneInvalid(map, citizens);
    pathCredit_ = std::min(
        double(policy.pathsPerMinute),
        pathCredit_ + elapsed * policy.pathsPerMinute
    );
    pathsRemaining_ = std::size_t(pathCredit_);
    const auto initialPaths = pathsRemaining_;
    for (auto& c : citizens.citizens_)
    {
        if (c.health <= 1e-7)
        {
            map.employment().citizenDeparted(c.workplaceId);
            c.workplaceId = {};
            finish(map, c, minute);
            continue;
        }
        needs(c, elapsed);
        const auto* cargoDefinition =
            SettlementResourceCatalog::definition(c.carriedResource);
        if (c.health > 0 && c.hunger >= policy.foodSeekThreshold &&
            c.carriedAmount > 0 && cargoDefinition && cargoDefinition->edible &&
            map.logistics.consumeCarriedUnit(c.id))
        {
            --c.carriedAmount;
            c.hunger = std::max(0.0, c.hunger - policy.mealRestoration);
            if (c.carriedAmount == 0)
            {
                finish(map, c, minute);
            }
        }
        if (c.health <= 1e-7)
        {
            map.employment().citizenDeparted(c.workplaceId);
            c.workplaceId = {};
            finish(map, c, minute);
            continue;
        }
        const bool shift = workingHours(minute, policy);
        const auto* w = map.employment().workplace(c.workplaceId);
        bool valid = true;
        if (c.task.kind == CitizenTaskKind::Work)
        {
            valid =
                shift && w && w->operational && w->objectId == c.task.object;
        }
        if (c.task.kind == CitizenTaskKind::Home)
        {
            valid = !shift && c.homeId == c.task.object &&
                    map.objectState().completedObject(c.homeId);
        }
        if (c.task.kind == CitizenTaskKind::Build)
        {
            const auto* site = map.objectState().constructionSite(c.task.site);
            valid = shift && !c.workplaceId && site &&
                    site->footprint.contains(c.task.workTile);
        }
        if (c.task.kind == CitizenTaskKind::Haul)
        {
            valid =
                map.logistics.reservation(c.id) &&
                map.logistics.inventory(c.task.destination) &&
                (c.task.delivering || map.logistics.inventory(c.task.source));
            const auto* destination =
                map.logistics.inventory(c.task.destination);
            if (!c.task.delivering)
            {
                valid = valid && shift &&
                        (!c.workplaceId ||
                         (w && destination &&
                          w->objectTypeId == SettlementObjectTypes::Stockpile &&
                          w->objectId == destination->objectId));
            }
        }
        if (c.task.kind == CitizenTaskKind::Gather ||
            c.task.kind == CitizenTaskKind::Demolish)
        {
            valid = shift && !c.workplaceId;
            bool designated = c.task.kind == CitizenTaskKind::Gather &&
                              !c.task.command && c.task.site &&
                              map.objectState().constructionSite(c.task.site) &&
                              map.objectState()
                                  .constructionSite(c.task.site)
                                  ->footprint.contains(c.task.workTile);
            for (const auto& command : map.commandState().commands())
            {
                if (command.id != c.task.command)
                {
                    continue;
                }
                for (const auto& target : command.targets)
                {
                    if (target.footprint.contains(c.task.workTile) &&
                        target.objectId == c.task.object)
                    {
                        designated = true;
                        break;
                    }
                }
            }
            valid = valid && designated;
        }
        if (!valid)
        {
            finish(map, c, minute);
        }
    }
    std::erase_if(
        citizens.citizens_,
        [](const auto& c) { return c.health <= 1e-7; }
    );
    decisionCredit_ = std::min(
        double(policy.decisionsPerMinute),
        decisionCredit_ + elapsed * policy.decisionsPerMinute
    );
    const auto count =
        std::min(citizens.citizens_.size(), std::size_t(decisionCredit_));
    decisionCredit_ -= count;
    for (std::size_t i = 0; i < count; ++i)
    {
        auto& c =
            citizens.citizens_[decisionCursor_++ % citizens.citizens_.size()];
        if (!map.grid().isValidPosition(c.tilePosition))
        {
            continue;
        }
        if (c.task.kind != CitizenTaskKind::Eat &&
            c.hunger >= policy.foodSeekThreshold &&
            (c.carriedAmount == 0 || c.hunger > policy.starvationThreshold) &&
            (minute >= c.nextDecisionMinute ||
             c.observedLogisticsVersion != map.logistics.version()))
        {
            if (chooseFood(map, citizens, c, minute))
            {
                continue;
            }
            c.observedLogisticsVersion = map.logistics.version();
            c.nextDecisionMinute = minute + policy.retryMinutes;
        }
        if (c.task.kind == CitizenTaskKind::None)
        {
            decide(map, citizens, c, minute);
        }
    }
    citizens.tickMovement(map, elapsed);
    for (auto& c : citizens.citizens_)
    {
        execute(map, citizens, c, minute, elapsed);
    }
    produce(map, citizens, minute, elapsed);
    pathCredit_ -= double(initialPaths - pathsRemaining_);
    map.employment().record(minute, citizens);
    ++citizens.version_;
}
void SettlementActivitySystem::decide(
    SettlementMap& map,
    SettlementCitizenState& citizens,
    SettlementCitizen& c,
    double minute
)
{
    if (minute < c.nextWorkCheckMinutes)
    {
        return;
    }
    c.nextWorkCheckMinutes = minute + policy.retryMinutes;
    if (workingHours(minute, policy))
    {
        if (const auto* w = map.employment().workplace(c.workplaceId))
        {
            const auto workplace = *w;
            if (!workplace.operational)
            {
                return;
            }
            if (workplace.objectTypeId == SettlementObjectTypes::Stockpile &&
                chooseHaul(
                    map,
                    citizens,
                    c,
                    minute,
                    map.logistics.forObject(workplace.objectId)
                ))
            {
                return;
            }
            if (route(map, citizens, c, workplace.footprint, true))
            {
                c.task.kind = CitizenTaskKind::Work;
                c.task.object = workplace.objectId;
                c.task.startedMinute = minute;
                c.activity = CitizenActivity::TravelingToWork;
            }
            return;
        }
        if (chooseConstruction(map, citizens, c, minute) ||
            chooseCommand(map, citizens, c, minute) ||
            chooseHaul(map, citizens, c, minute))
        {
            return;
        }
    }
    else if (const auto* home = map.objectState().completedObject(c.homeId))
    {
        if (route(map, citizens, c, home->footprint, false))
        {
            c.task.kind = CitizenTaskKind::Home;
            c.task.object = c.homeId;
            c.activity = CitizenActivity::ReturningHome;
        }
    }
}
void SettlementActivitySystem::execute(
    SettlementMap& map,
    SettlementCitizenState& citizens,
    SettlementCitizen& c,
    double minute,
    double elapsed
)
{
    if (c.task.kind == CitizenTaskKind::None || !c.path.empty())
    {
        return;
    }
    if (c.tilePosition != c.destination)
    {
        finish(map, c, minute);
        return;
    }
    if (c.task.kind == CitizenTaskKind::Eat)
    {
        if (map.logistics.pickUp(c.id))
        {
            c.hunger = std::max(0.0, c.hunger - policy.mealRestoration);
        }
        finish(map, c, minute);
    }
    else if (c.task.kind == CitizenTaskKind::Haul)
    {
        if (!c.task.delivering)
        {
            const auto* claim = map.logistics.reservation(c.id);
            if (!claim)
            {
                finish(map, c, minute);
                return;
            }
            const auto copy = *claim;
            const auto* destination =
                map.logistics.inventory(c.task.destination);
            if (!destination)
            {
                finish(map, c, minute);
                return;
            }
            const auto footprint = destination->footprint;
            if (!route(map, citizens, c, footprint, true))
            {
                if (pathsRemaining_ > 0)
                {
                    finish(map, c, minute);
                }
                return;
            }
            if (!map.logistics.pickUp(c.id))
            {
                finish(map, c, minute);
                return;
            }
            c.carriedResource = copy.resource;
            c.carriedAmount = copy.amount;
            c.task.delivering = true;
        }
        else
        {
            const auto* inventory = map.logistics.inventory(c.task.destination);
            const auto siteId =
                inventory ? inventory->siteId : ConstructionSiteId{};
            if (map.logistics.deliver(c.id))
            {
                c.carriedAmount = 0;
                if (siteId)
                {
                    const auto* delivered =
                        map.logistics.inventory(c.task.destination);
                    map.objectState().deliverMaterials(
                        siteId,
                        c.carriedResource,
                        delivered ? delivered->amount(c.carriedResource) : 0
                    );
                }
            }
            finish(map, c, minute);
        }
    }
    else if (c.task.kind == CitizenTaskKind::Build)
    {
        const auto* site = map.objectState().constructionSite(c.task.site);
        if (!site)
        {
            finish(map, c, minute);
            return;
        }
        const auto copy = *site;
        const bool road = copy.objectTypeId == SettlementObjectTypes::Road;
        const auto object = map.objectState().build(
            c.task.site,
            elapsed,
            road ? policy.roadMinutes : policy.constructionMinutes,
            c.tilePosition
        );
        if (object)
        {
            map.logistics.consumeSite(c.task.site);
            map.logistics.synchronize(map.objectState(), minute);
            finish(map, c, minute);
            return;
        }
        c.task.laborMinutes += elapsed;
        if (!road && c.task.laborMinutes >= 8 && pathsRemaining_ > 0)
        {
            c.task.laborMinutes = 0;
            ++c.choiceSequence;
            const auto p = SettlementTilePosition{
                copy.footprint.topLeft.x +
                    int(c.choiceSequence % copy.footprint.width),
                copy.footprint.topLeft.y +
                    int((c.choiceSequence / copy.footprint.width) %
                        copy.footprint.height)
            };
            route(map, citizens, c, {p, 1, 1}, true);
        }
    }
    else if (
        c.task.kind == CitizenTaskKind::Gather ||
        c.task.kind == CitizenTaskKind::Demolish
    )
    {
        c.task.laborMinutes += elapsed;
        if (c.task.laborMinutes < (c.task.kind == CitizenTaskKind::Gather
                                       ? policy.gatheringMinutes
                                       : policy.demolitionMinutes))
        {
            return;
        }
        if (c.task.kind == CitizenTaskKind::Gather)
        {
            const auto feature = map.naturalFeatures().at(c.task.workTile);
            if (feature.kind != NaturalFeatureKind::None)
            {
                map.logistics.drop(
                    c.task.workTile,
                    feature.kind == NaturalFeatureKind::Tree
                        ? SettlementResourceTypes::Lumber
                        : SettlementResourceTypes::Stone,
                    4,
                    minute
                );
                map.naturalFeatures().set(
                    c.task.workTile,
                    NaturalFeatureKind::None
                );
            }
        }
        else if (c.task.site)
        {
            const auto* site = map.objectState().constructionSite(c.task.site);
            if (site)
            {
                map.objectState().cancelConstructionWithin(site->footprint);
            }
            map.logistics.synchronize(map.objectState(), minute);
        }
        else
        {
            const auto* object =
                map.objectState().completedObject(c.task.object);
            std::vector<ResourceAmount> salvage;
            if (object)
            {
                if (const auto* definition =
                        SettlementObjectCatalog::definition(
                            object->objectTypeId
                        ))
                {
                    for (const auto& cost :
                         definition->constructionResourceCosts)
                    {
                        salvage.push_back(
                            {std::string(cost.resourceId),
                             int(cost.requiredAmount / 2)}
                        );
                    }
                }
            }
            if (map.objectState().demolish(c.task.object, c.task.workTile))
            {
                for (const auto& goods : salvage)
                {
                    map.logistics.drop(
                        c.task.workTile,
                        goods.resource,
                        goods.amount,
                        minute
                    );
                }
            }
            map.logistics.synchronize(map.objectState(), minute);
        }
        finish(map, c, minute);
    }
    else if (c.task.kind == CitizenTaskKind::Home)
    {
        c.activity = CitizenActivity::AtHome;
    }
    else if (c.task.kind == CitizenTaskKind::Work)
    {
        c.activity = CitizenActivity::AtWork;
        const auto* object = map.objectState().completedObject(c.task.object);
        if (!object)
        {
            finish(map, c, minute);
            return;
        }
        if (object->objectTypeId == SettlementObjectTypes::Stockpile)
        {
            // A staffed stockpile uses the same hauling plan, restricted to its
            // own storage.
            if (minute >= c.task.startedMinute + policy.retryMinutes)
            {
                finish(map, c, minute);
            }
            return;
        }
    }
}
void SettlementActivitySystem::produce(
    SettlementMap& map,
    const SettlementCitizenState& citizens,
    double minute,
    double elapsed
)
{
    std::unordered_map<SettlementObjectId, int, StrongIdHash> attendance;
    for (const auto& c : citizens.citizens())
    {
        if (c.task.kind == CitizenTaskKind::Work && c.path.empty() &&
            c.tilePosition == c.destination)
        {
            ++attendance[c.task.object];
        }
    }
    for (const auto& [objectId, workers] : attendance)
    {
        const auto* object = map.objectState().completedObject(objectId);
        if (!object ||
            object->objectTypeId != SettlementObjectTypes::FishingGrounds)
        {
            continue;
        }
        const double rate = fisheryProductionPerMinute(
            object->productionWater.size(),
            workers,
            policy.fishery
        );
        const auto inventory = map.logistics.forObject(objectId);
        const int space = map.logistics.freeSpace(inventory);
        if (space <= 0)
        {
            continue;
        }
        const int fish = std::min(
            space,
            int(map.objectState().accrueProduction(objectId, elapsed * rate))
        );
        if (fish > 0)
        {
            map.logistics
                .add(inventory, SettlementResourceTypes::Fish, fish, minute);
        }
    }
}

} // namespace Paladin

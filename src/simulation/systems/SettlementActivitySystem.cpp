#include "simulation/systems/SettlementActivitySystem.h"
#include "world/generation/GenerationNoise.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include "world/settlements/objects/SettlementDoor.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace Paladin
{
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
            map.logistics.drop(
                c.tilePosition,
                c.carriedResource,
                c.carriedAmount,
                minute
            );
        }
        map.logistics.release(c.id);
        jobBoard_.releaseClaim(c.task);
        c.carriedAmount = 0;
        c.carriedResource.clear();
        c.haulDeliveryPath.clear();
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
        citizens.recordPopulation(minute);
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
        families_.update(map, citizens, policy, *this, minute, elapsed);
        map.employment().synchronize(map.objectState(), citizens);
        map.commerce.update(map, citizens, minute, elapsed);
        assignHomes(map, citizens);
        if (jobBoard_.refreshCommandBoard(map))
        {
            map.commandState().pruneInvalid(map, citizens);
        }
        pathCredit_ = std::min(
            double(policy.pathsPerMinute),
            pathCredit_ + elapsed * policy.pathsPerMinute
        );
        // Accumulated time credit must not become a burst of expensive searches
        // when many workers finish together. Long advances still make progress
        // through the same bounded simulation substeps.
        pathsRemaining_ =
            std::min(std::size_t(pathCredit_), policy.maximumPathsPerStep);
        const auto initialPaths = pathsRemaining_;
        for (auto& c : citizens.citizens_)
        {
            if (c.health <= 1e-7)
            {
                map.commerce.citizenDeparted(c.id);
                map.employment().citizenDeparted(c.workplaceId);
                c.workplaceId = {};
                finish(map, c, minute);
                continue;
            }
            if (c.foodSeekHunger < policy.foodSeekThreshold)
            {
                planMeal(c, map);
            }
            planSleep(map, c, minute);
            planBreak(map, c, minute);
            needs(c, elapsed, minute);
            const auto* cargoDefinition =
                SettlementResourceCatalog::definition(c.carriedResource);
            if (c.health > 0 && c.hunger >= c.foodSeekHunger &&
                c.carriedAmount > 0 && cargoDefinition &&
                cargoDefinition->edible &&
                map.logistics.consumeCarriedUnit(c.id))
            {
                --c.carriedAmount;
                c.hunger = std::max(0.0, c.hunger - policy.mealRestoration);
                planMeal(c, map);
                if (c.carriedAmount == 0)
                {
                    finish(map, c, minute);
                }
            }
            if (c.health <= 1e-7)
            {
                map.commerce.citizenDeparted(c.id);
                map.employment().citizenDeparted(c.workplaceId);
                c.workplaceId = {};
                finish(map, c, minute);
                continue;
            }
            const bool shift = policy.isWorkTime(minute);
            const auto* w = map.employment().workplace(c.workplaceId);
            bool valid = true;
            if (c.task.kind == CitizenTaskKind::FamilyMeal)
            {
                const auto* other = citizens.citizen(c.task.partner);
                valid =
                    other && other->health > 0 && minute < c.task.endMinute &&
                    other->task.kind == CitizenTaskKind::FamilyMeal &&
                    other->task.partner == c.id &&
                    (c.child
                         ? (other->id == c.motherId || other->id == c.fatherId)
                         : (other->child && (other->motherId == c.id ||
                                             other->fatherId == c.id)));
            }
            if (c.task.kind == CitizenTaskKind::Work)
            {
                valid = c.youngDependents == 0 && shift && w &&
                        w->operational && w->objectId == c.task.object;
            }
            if (c.task.kind == CitizenTaskKind::Sleep)
            {
                // An actual rest block survives clock/day boundaries.
                valid = shouldSleep(c, minute) &&
                        (!c.task.object || c.task.object == c.homeId);
            }
            if (c.task.kind == CitizenTaskKind::Break)
            {
                valid = c.breakUntil > 0 && c.workplaceId == c.breakEmployer;
            }
            if (c.task.kind == CitizenTaskKind::Talk)
            {
                const auto* other = citizens.citizen(c.task.partner);
                valid = other && other->task.kind == CitizenTaskKind::Talk &&
                        other->task.partner == c.id &&
                        (!c.workplaceId || !shift || c.breakUntil > minute) &&
                        (c.child || !shift || c.workplaceId ||
                         (map.commandState().commands().empty() &&
                          map.objectState().constructionSites().empty()));
            }
            if (c.task.kind == CitizenTaskKind::Care)
            {
                valid =
                    (c.youngDependents > 0 ||
                     (c.child && c.ageYears < policy.independentEatingAge)) &&
                    c.homeId == c.task.object;
            }
            if (c.task.kind == CitizenTaskKind::Home)
            {
                valid = (!c.workplaceId || !shift) &&
                        c.homeId == c.task.object &&
                        map.objectState().completedObject(c.homeId);
            }
            if (c.task.kind == CitizenTaskKind::Build)
            {
                const auto* site =
                    map.objectState().constructionSite(c.task.site);
                valid = !c.workplaceId && site &&
                        site->footprint.contains(c.task.workTile);
            }
            if (c.task.kind == CitizenTaskKind::Haul)
            {
                valid = map.logistics.reservation(c.id) &&
                        map.logistics.inventory(c.task.destination) &&
                        (c.task.delivering ||
                         map.logistics.inventory(c.task.source));
                const auto* destination =
                    map.logistics.inventory(c.task.destination);
                if (!c.task.delivering)
                {
                    valid =
                        valid &&
                        (!c.workplaceId ||
                         (shift && w && destination &&
                          (w->objectTypeId ==
                               SettlementObjectTypes::Stockpile ||
                           w->objectTypeId == SettlementObjectTypes::Market) &&
                          w->objectId == destination->objectId));
                }
            }
            if (c.task.kind == CitizenTaskKind::Gather ||
                c.task.kind == CitizenTaskKind::Demolish)
            {
                valid = !c.workplaceId;
                bool designated =
                    c.task.kind == CitizenTaskKind::Gather && !c.task.command &&
                    c.task.site &&
                    map.objectState().constructionSite(c.task.site) &&
                    map.objectState()
                        .constructionSite(c.task.site)
                        ->footprint.contains(c.task.workTile);
                designated = designated || map.commandState().contains(
                                               map,
                                               c.task.command,
                                               c.task.workTile,
                                               c.task.object,
                                               c.task.site
                                           );
                valid = valid && designated;
            }
            if (c.youngDependents > 0 &&
                (c.task.kind == CitizenTaskKind::Build ||
                 c.task.kind == CitizenTaskKind::Gather ||
                 c.task.kind == CitizenTaskKind::Demolish ||
                 c.task.kind == CitizenTaskKind::Haul ||
                 c.task.kind == CitizenTaskKind::Talk ||
                 c.task.kind == CitizenTaskKind::Break))
            {
                valid = false;
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
        jobBoard_.resetClaims();
        for (const auto& citizen : citizens.citizens_)
        {
            jobBoard_.claim(citizen.task);
        }
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
                citizens
                    .citizens_[decisionCursor_++ % citizens.citizens_.size()];
            if (!map.grid().isValidPosition(c.tilePosition))
            {
                continue;
            }
            if (c.exitingHomeId && !c.path.empty())
            {
                continue;
            }
            startBreak(map, c, minute);
            const bool preSleepMeal =
                shouldSleep(c, minute) &&
                c.task.kind != CitizenTaskKind::Sleep &&
                c.hunger + policy.hungerPerDay *
                               std::max(
                                   policy.requiredSleepMinutes,
                                   (policy.fullRestEnergy - c.energy) /
                                       policy.sleepEnergyPerMinute
                               ) /
                               1440 >=
                    policy.urgentFoodThreshold;
            if ((!c.child || c.ageYears >= policy.independentEatingAge) &&
                c.task.kind != CitizenTaskKind::Eat &&
                (c.task.kind != CitizenTaskKind::FamilyMeal ||
                 (!c.child && c.hunger >= policy.urgentFoodThreshold)) &&
                (c.hunger >= c.foodSeekHunger || preSleepMeal) &&
                (c.carriedAmount == 0 ||
                 c.hunger >= policy.urgentFoodThreshold) &&
                (c.task.kind != CitizenTaskKind::Sleep ||
                 c.hunger >= policy.urgentFoodThreshold) &&
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
            if (c.task.kind == CitizenTaskKind::FamilyMeal)
            {
                continue;
            }
            if (manageBreak(map, citizens, c, minute))
            {
                continue;
            }
            if (chooseSleep(map, citizens, c, minute))
            {
                continue;
            }
            if (c.youngDependents > 0 ||
                (c.child && c.ageYears < policy.independentEatingAge))
            {
                if (c.task.kind != CitizenTaskKind::Care &&
                    c.task.kind != CitizenTaskKind::Eat &&
                    c.task.kind != CitizenTaskKind::Sleep)
                {
                    const auto* home =
                        map.objectState().completedObject(c.homeId);
                    if (home)
                    {
                        auto planned = c;
                        if (c.insideHome || route(
                                                map,
                                                citizens,
                                                planned,
                                                home->footprint,
                                                false
                                            ))
                        {
                            finish(map, c, minute);
                            c.path = std::move(planned.path);
                            c.pathIndex = planned.pathIndex;
                            c.stepProgress = planned.stepProgress;
                            c.stepDuration = planned.stepDuration;
                            c.destination = planned.destination;
                            c.task.kind = CitizenTaskKind::Care;
                            c.task.object = c.homeId;
                        }
                    }
                }
                continue;
            }
            if (c.task.kind == CitizenTaskKind::Home && c.path.empty())
            {
                chooseSocial(map, citizens, c, minute);
            }
            if (c.task.kind == CitizenTaskKind::None ||
                (c.task.kind == CitizenTaskKind::Home &&
                 (!c.workplaceId || policy.isWorkTime(minute))))
            {
                decide(map, citizens, c, minute);
            }
        }
        citizens.tickMovement(map, elapsed);
        for (auto& c : citizens.citizens_)
        {
            execute(map, citizens, c, minute, elapsed);
        }
        assignHomes(map, citizens);
        produce(map, citizens, minute, elapsed);
        pathCredit_ -= double(initialPaths - pathsRemaining_);
        citizens.recordPopulation(minute + elapsed);
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
        if (!c.child && !c.workplaceId)
        {
            if (!policy.isWorkTime(minute) &&
                chooseSocial(map, citizens, c, minute))
            {
                return;
            }
            if (chooseConstruction(map, citizens, c, minute) ||
                chooseCommand(map, citizens, c, minute) ||
                chooseHaul(map, citizens, c, minute))
            {
                return;
            }
        }
        else if (policy.isWorkTime(minute))
        {
            if (const auto* w = map.employment().workplace(c.workplaceId))
            {
                const auto workplace = *w;
                if (!workplace.operational)
                {
                    return;
                }
                if ((workplace.objectTypeId ==
                         SettlementObjectTypes::Stockpile ||
                     workplace.objectTypeId == SettlementObjectTypes::Market) &&
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
                chooseWork(map, citizens, c, minute);
                return;
            }
        }
        // A queued or claimed job is not necessarily executable. Poll for
        // actual work without cancelling the current home activity or
        // restarting routes.
        if (c.task.kind == CitizenTaskKind::Home)
        {
            return;
        }
        if (chooseSocial(map, citizens, c, minute))
        {
            return;
        }
        if (!c.workplaceId || !policy.isWorkTime(minute))
        {
            if (const auto* home = map.objectState().completedObject(c.homeId))
            {
                if (route(map, citizens, c, home->footprint, false))
                {
                    c.observedLogisticsVersion = map.logistics.version();
                    c.task.kind = CitizenTaskKind::Home;
                    c.task.object = c.homeId;
                    c.activity = CitizenActivity::ReturningHome;
                }
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
        if (c.task.kind == CitizenTaskKind::FamilyMeal)
        {
            if (!c.child)
            {
                return;
            }
            auto parent = std::find_if(
                citizens.citizens_.begin(),
                citizens.citizens_.end(),
                [&](const auto& p) { return p.id == c.task.partner; }
            );
            if (parent == citizens.citizens_.end())
            {
                finish(map, c, minute);
                return;
            }
            if (parent->insideHome && !c.insideHome)
            {
                enterHome(map, c);
                return;
            }
            const bool together =
                (parent->insideHome && c.insideHome &&
                 parent->homeId == c.homeId) ||
                (!parent->insideHome && !c.insideHome &&
                 std::abs(parent->tilePosition.x - c.tilePosition.x) +
                         std::abs(parent->tilePosition.y - c.tilePosition.y) <=
                     1);
            if (together && parent->hunger < policy.urgentFoodThreshold)
            {
                const double share = std::max(.01, policy.dependentFoodShare);
                const double fed = std::min(
                    {c.hunger,
                     policy.mealRestoration,
                     std::max(
                         0.0,
                         policy.urgentFoodThreshold - parent->hunger
                     ) / share}
                );
                c.hunger -= fed;
                parent->hunger += fed * share;
                planMeal(c, map);
                finish(map, *parent, minute);
                finish(map, c, minute);
            }
            return;
        }
        if (c.task.kind == CitizenTaskKind::Eat)
        {
            const auto* source = map.logistics.inventory(c.task.source);
            const bool market = source && source->kind == InventoryKind::Market;
            const auto marketId =
                source ? source->objectId : SettlementObjectId{};
            const bool publicFood =
                source && (source->kind == InventoryKind::Keep ||
                           source->kind == InventoryKind::Stockpile);
            const auto price =
                source ? map.commerce.mealPrice(map, *source) : 0;
            if (price < 0 ||
                (price > 0 && !map.commerce.canBuyMeal(c, citizens, price)) ||
                (market && !map.commerce.marketOpen(map, citizens, marketId)))
            {
                finish(map, c, minute);
                return;
            }
            const auto* mealReservation = map.logistics.reservation(c.id);
            const std::string mealResource =
                mealReservation ? mealReservation->resource : "";
            if (map.logistics.pickUp(c.id))
            {
                map.commerce
                    .recordFlow(c.task.source, {}, mealResource, 1, c.id);
                if (price > 0)
                {
                    map.commerce.buyMeal(marketId, c, citizens, price);
                }
                map.commerce.recordMeal(c, publicFood && price == 0);
                c.hunger = std::max(0.0, c.hunger - policy.mealRestoration);
                planMeal(c, map);
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
                const bool market = destination->kind == InventoryKind::Market;
                const auto* sourceInventory =
                    map.logistics.inventory(copy.source);
                if (!sourceInventory ||
                    map.commerce.affordableTradeUnits(
                        *sourceInventory,
                        *destination,
                        copy.amount
                    ) < copy.amount ||
                    (market && sourceInventory &&
                     sourceInventory->kind == InventoryKind::Keep &&
                     !map.commerce.keepFoodSalesEnabled))
                {
                    finish(map, c, minute);
                    return;
                }
                const bool cached = c.haulDeliveryTopology ==
                                        map.objectState().navigationVersion() &&
                                    (!c.haulDeliveryPath.empty() ||
                                     c.tilePosition == c.haulDeliveryTarget);
                if (cached)
                {
                    c.path = std::move(c.haulDeliveryPath);
                    c.pathIndex = 0;
                    c.stepProgress = 0;
                    c.destination = c.haulDeliveryTarget;
                    c.explicitMovement = !c.path.empty();
                    if (!c.path.empty())
                    {
                        c.stepDuration = citizens.navigation_.stepCost(
                            map,
                            c.tilePosition,
                            c.path.front(),
                            citizens.movementPolicy
                        );
                    }
                }
                if (!cached && !route(map, citizens, c, footprint, true))
                {
                    if (pathsRemaining_ > 0)
                    {
                        finish(map, c, minute);
                    }
                    return;
                }
                // Picking up the last groundpile unit can erase/reallocate
                // inventories.
                const auto sourceForTrade = *sourceInventory;
                const auto destinationForTrade = *destination;
                if (!map.logistics.pickUp(c.id))
                {
                    finish(map, c, minute);
                    return;
                }
                c.carriedResource = copy.resource;
                map.commerce
                    .buyGoods(sourceForTrade, destinationForTrade, copy.amount);
                c.carriedAmount = copy.amount;
                c.task.delivering = true;
            }
            else
            {
                const auto* inventory =
                    map.logistics.inventory(c.task.destination);
                const auto siteId =
                    inventory ? inventory->siteId : ConstructionSiteId{};
                if (map.logistics.deliver(c.id))
                {
                    map.commerce.recordFlow(
                        c.task.source,
                        c.task.destination,
                        c.carriedResource,
                        c.carriedAmount
                    );
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
            const auto footprint = site->footprint;
            const bool road = site->objectTypeId == SettlementObjectTypes::Road;
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
                    footprint.topLeft.x +
                        int(c.choiceSequence % footprint.width),
                    footprint.topLeft.y +
                        int((c.choiceSequence / footprint.width) %
                            footprint.height)
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
                const auto* site =
                    map.objectState().constructionSite(c.task.site);
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
                                 int((std::uint64_t(cost.requiredAmount) *
                                          (definition->constructionCostPerTile
                                               ? std::uint64_t(
                                                     object->footprint.width
                                                 ) * object->footprint.height
                                               : 1) +
                                      cost.referenceArea - 1) /
                                     cost.referenceArea / 2)}
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
        else if (c.task.kind == CitizenTaskKind::Sleep)
        {
            if (c.task.object && !enterHome(map, c))
            {
                if (!map.objectState().completedObject(c.homeId))
                {
                    finish(map, c, minute);
                }
                return;
            }
            const auto* home = map.objectState().completedObject(c.task.object);
            if (c.task.object &&
                (!home || !home->footprint.contains(c.tilePosition)))
            {
                return;
            }
            const bool occupied = std::any_of(
                citizens.citizens().begin(),
                citizens.citizens().end(),
                [&](const auto& other)
                {
                    return other.id != c.id &&
                           other.task.kind == CitizenTaskKind::Sleep &&
                           other.homeId == c.homeId &&
                           other.task.target == c.task.target &&
                           other.id.value() < c.id.value();
                }
            );
            if (home && (!home->footprint.contains(c.task.target) || occupied))
            {
                c.task.target = sleepingPosition(map, citizens, c);
            }
            if (c.tilePosition != c.task.target)
            {
                // Houses have a walkable interior; movement still crosses each
                // tile, after entering through the physical door.
                auto p = c.tilePosition;
                c.path.clear();
                while (p != c.task.target)
                {
                    if (p.x != c.task.target.x)
                    {
                        p.x += p.x < c.task.target.x ? 1 : -1;
                    }
                    else
                    {
                        p.y += p.y < c.task.target.y ? 1 : -1;
                    }
                    c.path.push_back(p);
                }
                c.pathIndex = 0;
                c.stepProgress = 0;
                c.destination = c.task.target;
                c.explicitMovement = true;
                c.activity = CitizenActivity::ReturningHome;
                return;
            }
            c.activity = CitizenActivity::Sleeping;
            const double restingMinutes = std::min(
                elapsed,
                std::max(0.0, policy.fullRestEnergy - c.energy) /
                    policy.sleepEnergyPerMinute
            );
            c.energy = std::min(
                policy.fullRestEnergy,
                c.energy + policy.sleepEnergyPerMinute * restingMinutes
            );
            c.sleptMinutes += restingMinutes;
            if (c.energy >= policy.fullRestEnergy - 1e-7)
            {
                c.task = {};
                c.task.kind =
                    home ? CitizenTaskKind::Home : CitizenTaskKind::None;
                c.task.object = home ? c.homeId : SettlementObjectId{};
                c.activity =
                    home ? CitizenActivity::AtHome : CitizenActivity::Idle;
                c.observedLogisticsVersion = map.logistics.version();
            }
        }
        else if (c.task.kind == CitizenTaskKind::Care)
        {
            if (enterHome(map, c))
            {
                c.activity = CitizenActivity::AtHome;
            }
        }
        else if (c.task.kind == CitizenTaskKind::Home)
        {
            if (c.task.endMinute > minute)
            {
                return;
            }
            if (c.task.endMinute > 0)
            {
                const auto* home = map.objectState().completedObject(c.homeId);
                if (home && route(map, citizens, c, home->footprint, false))
                {
                    c.task.endMinute = 0;
                }
                return;
            }
            if (!enterHome(map, c))
            {
                return;
            }
            c.activity = CitizenActivity::AtHome;
            if (minute >= c.nextHomeWander)
            {
                const auto* home = map.objectState().completedObject(c.homeId);
                const auto random =
                    GenerationNoise::mix(c.id.value() ^ ++c.choiceSequence);
                c.nextHomeWander = minute + 3 + random % 7;
                if (home && home->door)
                {
                    const auto outside =
                        outsideDoor(home->footprint, *home->door);
                    const SettlementTilePosition nearby{
                        outside.x +
                            int(random % (2 * policy.leisureRadius + 1)) -
                            policy.leisureRadius,
                        outside.y +
                            int((random >> 8) %
                                (2 * policy.leisureRadius + 1)) -
                            policy.leisureRadius
                    };
                    if (route(map, citizens, c, {nearby, 1, 1}, true))
                    {
                        c.task.endMinute = minute + 30 + random % 31;
                        return;
                    }
                }
                const SettlementTilePosition target{
                    c.tilePosition.x + int(random % 3) - 1,
                    c.tilePosition.y + int((random >> 8) % 3) - 1
                };
                if (home && home->footprint.contains(target) &&
                    target != c.tilePosition)
                {
                    c.path = {target};
                    c.pathIndex = 0;
                    c.stepProgress = 0;
                    c.destination = target;
                    c.explicitMovement = true;
                }
            }
        }
        else if (c.task.kind == CitizenTaskKind::Break)
        {
            c.activity = CitizenActivity::OnBreak;
        }
        else if (c.task.kind == CitizenTaskKind::Talk)
        {
            auto other = std::find_if(
                citizens.citizens_.begin(),
                citizens.citizens_.end(),
                [&](const auto& person) { return person.id == c.task.partner; }
            );
            if (other == citizens.citizens_.end() ||
                other->task.partner != c.id ||
                (c.task.endMinute == 0 && minute - c.task.startedMinute > 12))
            {
                finish(map, c, minute);
                return;
            }
            if (other->path.empty() && c.task.endMinute == 0 &&
                std::abs(other->tilePosition.x - c.tilePosition.x) <= 1 &&
                std::abs(other->tilePosition.y - c.tilePosition.y) <= 1)
            {
                c.task.endMinute = other->task.endMinute =
                    minute + c.task.laborMinutes;
            }
            c.activity = CitizenActivity::Talking;
            if (c.task.endMinute > minute && other->path.empty() &&
                std::abs(other->tilePosition.x - c.tilePosition.x) <= 1 &&
                std::abs(other->tilePosition.y - c.tilePosition.y) <= 1)
            {
                c.happiness = std::min(
                    std::max(
                        0.0,
                        100.0 - c.publicFoodDissatisfaction +
                            std::min(0.0, c.taxHappinessAdjustment)
                    ),
                    c.happiness + policy.talkingHappinessPerMinute * elapsed
                );
            }
            if (c.task.endMinute > 0 && minute >= c.task.endMinute)
            {
                finish(map, c, minute);
            }
        }
        else if (c.task.kind == CitizenTaskKind::Work)
        {
            c.activity = CitizenActivity::AtWork;
            const auto* object =
                map.objectState().completedObject(c.task.object);
            if (!object)
            {
                finish(map, c, minute);
                return;
            }
            if (object->objectTypeId == SettlementObjectTypes::FishingGrounds)
            {
                c.activity = CitizenActivity::Fishing;
            }
            if (object->objectTypeId == SettlementObjectTypes::Market)
            {
                if (minute >= c.task.startedMinute + policy.retryMinutes)
                {
                    c.task.startedMinute = minute;
                    chooseHaul(
                        map,
                        citizens,
                        c,
                        minute,
                        map.logistics.forObject(c.task.object)
                    );
                }
                return;
            }
            if (object->objectTypeId == SettlementObjectTypes::Stockpile)
            {
                // A staffed stockpile uses the same hauling plan, restricted to
                // its own storage.
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
            if (c.breakUntil > minute && c.workplaceId == c.breakEmployer &&
                policy.isWorkTime(minute))
            {
                ++attendance[c.breakObject];
            }
            else if (
                c.task.kind == CitizenTaskKind::Work && c.path.empty() &&
                c.tilePosition == c.destination
            )
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
                int(
                    map.objectState().accrueProduction(objectId, elapsed * rate)
                )
            );
            if (fish > 0)
            {
                map.commerce.recordFlow(
                    {},
                    inventory,
                    SettlementResourceTypes::Fish,
                    fish
                );
                map.logistics.add(
                    inventory,
                    SettlementResourceTypes::Fish,
                    fish,
                    minute
                );
            }
        }
    }

} // namespace Paladin

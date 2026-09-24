#include "simulation/systems/SettlementActivitySystem.h"
#include "world/generation/GenerationNoise.h"
#include "world/settlements/SettlementHomeBeds.h"
#include "world/settlements/SettlementIndustry.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include "world/settlements/objects/SettlementDoor.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include "world/settlements/objects/jobs/LoggingGroundsJob.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace Paladin
{
    bool SettlementActivitySystem::chooseBurial(SettlementMap& map,SettlementCitizenState& citizens,SettlementCitizen& c,double minute)
    {
        const auto* w=map.employment().workplace(c.workplaceId);
        const auto* yard=w ? map.objectState().completedObject(w->objectId) : nullptr;
        if(!yard || yard->objectTypeId!=SettlementObjectTypes::Graveyard) return false;
        CitizenRemains* nearest=nullptr; int distance=97;
        for(auto& r:citizens.remains_)
        {
            if(r.buried || r.carrier || routeFailed(c,RouteFailureDomain::Workplace,
                (std::uint64_t(1)<<63)|r.sequence,c.tilePosition,map,minute)) continue;
            const int d=std::abs(r.position.x-c.tilePosition.x)+std::abs(r.position.y-c.tilePosition.y);
            if(d<distance) { distance=d; nearest=&r; }
        }
        if(!nearest) return false;
        SettlementTilePosition slot{-1,-1};
        for(int y=yard->footprint.topLeft.y+1;y<yard->footprint.topLeft.y+yard->footprint.height && slot.x<0;y+=2)
            for(int x=yard->footprint.topLeft.x+1;x<yard->footprint.topLeft.x+yard->footprint.width;x+=2)
            {
                const SettlementTilePosition p{x,y};
                if(citizens.navigation_.walkable(map,p) && std::none_of(citizens.remains_.begin(),citizens.remains_.end(),[&](const auto& r){return r.grave==p;})) {slot=p;break;}
            }
        if(slot.x<0) return false;
        CitizenRoutePlan trip(c);
        const auto failed=[&]() {
            if(!routeBudgetLimited_) rememberRouteFailure(c,RouteFailureDomain::Workplace,
                (std::uint64_t(1)<<63)|nearest->sequence,c.tilePosition,map,minute);
            return false;
        };
        if(!route(map,citizens,trip,{nearest->position,1,1},true)) return failed();
        auto delivery=trip.fromPosition(trip.destination);
        if(!route(map,citizens,delivery,{slot,1,1},true)) return failed();
        finish(map,c,minute); trip.applyTo(c);
        c.haulDeliveryPath=std::move(delivery.path); c.haulDeliveryTarget=slot;
        c.haulDeliveryTopology=map.objectState().navigationVersion();
        c.task.kind=CitizenTaskKind::Burial; c.task.remainsSequence=nearest->sequence;
        c.task.object=yard->id; c.task.target=nearest->position;
        nearest->carrier=c.id; nearest->grave=slot; nearest->graveyard=yard->id;
        c.activity=CitizenActivity::TravelingToWork;
        return true;
    }
    void SettlementActivitySystem::executeBurial(SettlementMap& map,SettlementCitizenState& citizens,SettlementCitizen& c,double minute,double elapsed)
    {
        auto it=std::find_if(citizens.remains_.begin(),citizens.remains_.end(),[&](const auto& r){return r.sequence==c.task.remainsSequence && r.carrier==c.id;});
        if(it==citizens.remains_.end()) {finish(map,c,minute);return;}
        auto& r=*it;
        if(r.pickedUp) r.position=c.tilePosition;
        if(!c.path.empty()) return;
        if(!r.pickedUp)
        {
            if(c.tilePosition!=r.position) {r.carrier={};r.grave={-1,-1};finish(map,c,minute);return;}
            CitizenRoutePlan trip(c);
            if(c.haulDeliveryTopology==map.objectState().navigationVersion())
            {
                trip.path=std::move(c.haulDeliveryPath);trip.pathIndex=0;trip.stepProgress=0;
                trip.destination=r.grave;trip.explicitMovement=!trip.path.empty();
                if(!trip.path.empty()) trip.stepDuration=citizens.navigation_.stepCost(map,c.tilePosition,trip.path.front(),citizens.movementPolicy);
            }
            else if(!route(map,citizens,trip,{r.grave,1,1},true))
            {
                if(!routeBudgetLimited_) {r.carrier={};r.grave={-1,-1};finish(map,c,minute);}
                return;
            }
            trip.applyTo(c); r.pickedUp=true; c.activity=CitizenActivity::Hauling; return;
        }
        if(c.tilePosition!=r.grave) {r.carrier={};r.pickedUp=false;r.grave={-1,-1};finish(map,c,minute);return;}
        c.activity=CitizenActivity::AtWork; c.workAnimationMinutes+=elapsed;
        c.task.laborMinutes+=elapsed;
        if(c.task.laborMinutes>=20)
        {r.buried=true;r.pickedUp=false;r.carrier={};r.position=r.grave;finish(map,c,minute);}
    }

    void SettlementActivitySystem::cancelDepotTasks(
        SettlementMap& map, SettlementCitizenState& citizens,
        SettlementObjectId depot, double minute)
    {
        const auto destination = map.logistics.forObject(depot);
        for (auto& person : citizens.citizens_)
        {
            if (destination && person.task.kind == CitizenTaskKind::Haul &&
                person.task.destination == destination)
            { finish(map, person, minute); }
        }
    }
    void SettlementActivitySystem::cancelConstructionTasks(
        SettlementMap& map, SettlementCitizenState& citizens, double minute)
    {
        for (auto& person : citizens.citizens_)
        {
            if ((person.task.site &&
                 !map.objectState().constructionSite(person.task.site)) ||
                (person.task.kind == CitizenTaskKind::Haul &&
                 person.task.destination &&
                 !map.logistics.inventory(person.task.destination)))
            {
                // Same exit as interruption: release reservations and drop any
                // already-carried cargo, including while the clock is paused.
                finish(map, person, minute);
            }
        }
    }

    void SettlementActivitySystem::retireCitizen(
        SettlementMap& map,
        SettlementCitizenState& citizens,
        SettlementCitizen& deceased,
        double minute
    )
    {
        citizens.recordDeath(deceased, minute);
        citizens.rememberAncestry(deceased);
        for (auto& survivor : citizens.citizens_)
        {
            survivor.familiarities.erase(deceased.id);
        }
        map.commerce.citizenDeparted(deceased.id);
        map.employment().citizenDeparted(deceased.workplaceId);
        deceased.workplaceId = {};
        finish(map, deceased, minute);
    }

    void SettlementActivitySystem::advanceInactiveLifecycle(
        SettlementMap& map,
        SettlementCitizenState& citizens,
        double minute,
        double elapsed
    )
    {
        if (!map.logistics.founded())
        {
            return;
        }
        citizens.remainsMinute_=minute+elapsed;
        std::erase_if(citizens.remains_,[&](const auto& r){return !r.buried && !r.carrier && minute+elapsed-r.diedMinute>=3*1440;});
        families_.update(map, citizens, policy, *this, minute, elapsed);
        bool died = false;
        for (auto& person : citizens.citizens_)
        {
            if (person.health <= 1e-7)
            {
                retireCitizen(map, citizens, person, minute);
                died = true;
            }
        }
        if (died)
        {
            citizens.recordAttributes(minute, 0);
            std::erase_if(
                citizens.citizens_,
                [](const auto& c) { return c.health <= 1e-7; }
            );
            ++citizens.familyVersion_;
            ++citizens.version_;
        }
    }

    void SettlementActivitySystem::assignHomes(
        SettlementMap& map,
        SettlementCitizenState& citizens
    )
    {
        families_.assignHomes(map, citizens, *this);
        if(bedsTopology_!=map.objectState().navigationVersion() || bedsFamily_!=citizens.familyVersion_ || bedsPopulation_!=citizens.citizens_.size())
        {
            assignHomeBeds(map,citizens.citizens_);
            bedsTopology_=map.objectState().navigationVersion(); bedsFamily_=citizens.familyVersion_; bedsPopulation_=citizens.citizens_.size();
        }
    }

    bool SettlementActivitySystem::pastureWorkerAvailableForGeneralLabor(
        const SettlementMap& map,
        const SettlementCitizen& citizen
    ) const noexcept
    {
        if (!citizen.workplaceId)
        {
            return false;
        }
        const auto* workplace = map.employment().workplace(citizen.workplaceId);
        return workplace && workplace->operational &&
               workplace->objectTypeId == SettlementObjectTypes::Pastureland &&
               map.animals.containedCount(workplace->objectId) == 0;
    }

    void SettlementActivitySystem::finish(
        SettlementMap& map,
        SettlementCitizen& c,
        double minute
    )
    {
        if (c.inFishingBoat && c.health > 0)
        {
            if (c.boatReturning)
            {
                return;
            }
            c.boatReturning = true;
            const bool finishEdge =
                c.pathIndex < c.path.size() && c.stepProgress > 0;
            const auto routeStart =
                finishEdge ? c.path[c.pathIndex] : c.tilePosition;
            auto returning =
                map.fishingBoats.returnRoute(map, routeStart, c.boatLanding);
            if (finishEdge)
            {
                returning.insert(returning.begin(), routeStart);
            }
            c.path = std::move(returning);
            c.pathIndex = 0;
            if (!finishEdge)
            {
                c.stepProgress = 0;
            }
            c.stepDuration = 1;
            c.destination = c.path.empty() ? c.tilePosition : c.path.back();
            c.explicitMovement = !c.path.empty();
            c.nextDecisionMinute = minute + policy.retryMinutes;
            return;
        }
        c.inFishingBoat = false;
        c.boatReturning = false;
        c.boatFishery = {};
        c.boatRoute.clear();
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
        if (c.task.animal)
        {
            map.animals.release(c.id);
        }
        jobBoard_.releaseClaim(c.task);
        c.carriedAmount = 0;
        c.carriedResource.clear();
        c.haulDeliveryPath.clear();
        c.task = {};
        c.activity = CitizenActivity::Idle;
        c.assignedCommandId = {};
        const bool finishStep =
            c.health > 0 && c.pathIndex < c.path.size() && c.stepProgress > 0 &&
            SettlementNavigation{}
                .canStep(map, c.tilePosition, c.path[c.pathIndex]);
        if (finishStep)
        {
            const auto endpoint = c.path[c.pathIndex];
            c.path.assign(1, endpoint);
            c.pathIndex = 0;
            c.destination = endpoint;
            c.explicitMovement = true;
        }
        else
        {
            c.path.clear();
            c.pathIndex = 0;
            c.stepProgress = 0;
            c.explicitMovement = false;
        }
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
            map.immigration.advance(map, citizens, minute, dt);
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
        citizens.remainsMinute_=minute+elapsed;
        for(auto& remains:citizens.remains_)
        {
            if(!remains.carrier) continue;
            const auto* worker=citizens.citizen(remains.carrier);
            if(!worker || worker->task.kind!=CitizenTaskKind::Burial || worker->task.remainsSequence!=remains.sequence)
            { if(worker && remains.pickedUp) remains.position=worker->tilePosition; remains.carrier={}; remains.pickedUp=false; remains.grave={-1,-1}; }
        }
        std::erase_if(citizens.remains_,[&](const auto& r){return !r.buried && !r.carrier && minute-r.diedMinute>=3*1440;});
        map.logistics.synchronize(map.objectState(), minute);
        families_.update(map, citizens, policy, *this, minute, elapsed);
        map.employment().synchronize(map.objectState(), citizens);
        map.commerce.update(map, citizens, minute, elapsed);
        assignHomes(map, citizens);
        std::unordered_set<SettlementObjectId, StrongIdHash> occupiedHomes;
        for (const auto& c : citizens.citizens())
        {
            if (c.homeId && c.health > 0)
            {
                occupiedHomes.insert(c.homeId);
            }
        }
        const auto fuelBefore = map.heating.lumberBurned();
        map.heating.advance(map.logistics, occupiedHomes, minute, elapsed);
        map.commerce.recordConsumption(
            SettlementResourceTypes::Lumber,
            int(map.heating.lumberBurned() - fuelBefore)
        );
        map.naturalFeatures().regrow(map.grid(), map.objectState(), minute);
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
        bool citizenDied = false;
        // Both pre-existing deaths and deaths caused by needs take the same
        // cleanup path, before the dense citizen array is compacted.
        const auto handleDeath = [&](SettlementCitizen& deceased)
        {
            citizenDied = true;
            retireCitizen(map, citizens, deceased, minute);
        };
        for (auto& c : citizens.citizens_)
        {
            if (c.health <= 1e-7)
            {
                handleDeath(c);
                continue;
            }
            if (c.militaryDeployed)
            {
                continue;
            }
            if (c.foodSeekHunger < policy.foodSeekThreshold)
            {
                planMeal(c, map);
            }
            planSleep(map, c, minute);
            planBreak(map, c, minute);
            needs(map, c, elapsed, minute);
            const auto& conditions = map.immigration.conditions();
            const auto& immigration = map.immigration.policy;
            const auto adjustPressure =
                [&](double& current, double target, AttributeEffect source)
            {
                const double step =
                    std::max(0.0, immigration.happinessAdjustmentPerDay) *
                    (source == AttributeEffect::FoodShortage && target > current
                         ? 1.25
                         : 1.0) *
                    elapsed / 1440;
                const double change = std::clamp(target - current, -step, step);
                current += change;
                c.modifyAttributes({{source, -change}});
            };
            adjustPressure(
                c.housingHappinessPenalty,
                conditions.unhousedShare * immigration.housingHappinessPenalty,
                AttributeEffect::Overcrowding
            );
            adjustPressure(
                c.foodHappinessPenalty,
                std::clamp(
                    1 - conditions.foodDays /
                            std::max(.01, immigration.minimumFoodDays),
                    0.0,
                    1.0
                ) * immigration.foodHappinessPenalty,
                AttributeEffect::FoodShortage
            );
            c.enforceHappinessModifiers();
            if (c.inFishingBoat && c.health > 0)
            {
                const auto* workplace =
                    map.employment().workplace(c.workplaceId);
                const bool breakDue =
                    !c.breakTaken && minute >= c.breakDue &&
                    policy.localMinute(minute) + policy.workBreakMinutes <=
                        policy.shiftEndMinute;
                if (!workplace || !workplace->operational ||
                    workplace->objectId != c.boatFishery ||
                    !policy.isWorkTime(minute) ||
                    (!c.boatReturning && c.hunger >= c.foodSeekHunger &&
                     boatMealAvailable(map, citizens, c, minute)) ||
                    shouldSleep(c, minute) || breakDue ||
                    (c.path.empty() && c.tilePosition != c.destination))
                {
                    finish(map, c, minute);
                }
                if (c.boatReturning && c.path.empty())
                {
                    if (citizens.navigation_.walkable(map, c.tilePosition))
                    {
                        c.inFishingBoat = false;
                        c.boatReturning = false;
                        c.boatFishery = {};
                        c.boatRoute.clear();
                        if (breakDue && policy.isWorkTime(minute))
                        {
                            startBreak(map, c, minute);
                        }
                        else
                        {
                            finish(map, c, minute);
                        }
                    }
                    else if (minute >= c.nextDecisionMinute)
                    {
                        c.boatReturning = false;
                        finish(map, c, minute);
                    }
                }
                if (c.inFishingBoat)
                {
                    if (!c.boatReturning && c.path.empty() &&
                        minute >= c.nextDecisionMinute)
                    {
                        const auto hash = GenerationNoise::mix(
                            c.id.value() ^ ++c.choiceSequence
                        );
                        c.nextDecisionMinute = minute + 12 + hash % 19;
                        const auto* fishery =
                            map.objectState().completedObject(c.boatFishery);
                        constexpr SettlementTilePosition
                            steps[]{{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
                        for (int i = 0; fishery && i < 4; ++i)
                        {
                            const auto d = steps[(hash + i) % 4];
                            const SettlementTilePosition p{
                                c.tilePosition.x + d.x,
                                c.tilePosition.y + d.y
                            };
                            const auto* tile = map.grid().tile(p);
                            if (!tile || tile->terrain != TerrainType::Water ||
                                !std::binary_search(
                                    fishery->productionWater.begin(),
                                    fishery->productionWater.end(),
                                    p,
                                    [](auto a, auto b)
                                    {
                                        return std::pair{a.y, a.x} <
                                               std::pair{b.y, b.x};
                                    }
                                ) ||
                                std::any_of(
                                    citizens.citizens().begin(),
                                    citizens.citizens().end(),
                                    [&](const auto& other)
                                    {
                                        return other.id != c.id &&
                                               other.inFishingBoat &&
                                               (other.tilePosition == p ||
                                                other.destination == p);
                                    }
                                ))
                            {
                                continue;
                            }
                            c.path = {p};
                            c.pathIndex = 0;
                            c.stepProgress = 0;
                            c.stepDuration = 3;
                            c.destination = p;
                            c.explicitMovement = true;
                            break;
                        }
                    }
                    continue;
                }
            }
            const auto* cargoDefinition =
                SettlementResourceCatalog::definition(c.carriedResource);
            if (c.health > 0 && c.hunger >= c.foodSeekHunger &&
                c.carriedAmount > 0 && cargoDefinition &&
                map.logistics.canEat(c.carriedResource) &&
                map.logistics.consumeCarriedUnit(c.id))
            {
                map.commerce.recordConsumption(c.carriedResource, 1);
                --c.carriedAmount;
                c.modifyAttributes(
                    {{AttributeEffect::Meals, -policy.mealRestoration}}
                );
                planMeal(c, map);
                if (c.carriedAmount == 0)
                {
                    finish(map, c, minute);
                }
            }
            if (c.health <= 1e-7)
            {
                handleDeath(c);
                continue;
            }
            const bool shift = policy.isWorkTime(minute);
            const auto* w = map.employment().workplace(c.workplaceId);
            const bool dormantPasture =
                pastureWorkerAvailableForGeneralLabor(map, c);
            const bool generalLabor = !c.workplaceId || dormantPasture;
            const bool activeWorkplace = c.workplaceId && !dormantPasture;
            bool valid = true;
            if (c.task.kind == CitizenTaskKind::AnimalWork)
            {
                const auto* animal = map.animals.find(c.task.animal);
                const bool assignedGatherer =
                    w && w->operational &&
                    w->objectTypeId == SettlementObjectTypes::Pastureland &&
                    animal && animal->order == AnimalOrder::Gather &&
                    animal->reservedPasture == w->objectId;
                valid = !c.child && (generalLabor || assignedGatherer) &&
                        !c.youngDependents && animal && animal->health > 0 &&
                        animal->handler == c.id &&
                        animal->order != AnimalOrder::None &&
                        (animal->order != AnimalOrder::Gather ||
                         map.objectState().completedObject(
                             animal->reservedPasture
                         ));
            }
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
            if(c.task.kind==CitizenTaskKind::Burial)
                valid=w && w->operational && w->objectTypeId==SettlementObjectTypes::Graveyard;
            if (c.task.kind == CitizenTaskKind::Work)
            {
                valid = c.youngDependents == 0 && shift && activeWorkplace &&
                        w && w->operational && w->objectId == c.task.object;
                if(valid && w->objectTypeId==SettlementObjectTypes::Graveyard && std::any_of(citizens.remains_.begin(),citizens.remains_.end(),[](const auto& r){return !r.buried && !r.carrier;})) valid=false;
            }
            if (c.task.kind == CitizenTaskKind::Sleep)
            {
                // An actual rest block survives clock/day boundaries.
                valid = shouldSleep(c, minute) &&
                        (!c.task.object || c.task.object == c.homeId);
            }
            if (c.task.kind == CitizenTaskKind::Break)
            {
                valid = activeWorkplace && c.breakUntil > 0 &&
                        c.workplaceId == c.breakEmployer;
            }
            if (c.task.kind == CitizenTaskKind::Talk)
            {
                const auto* other = citizens.citizen(c.task.partner);
                valid = other && other->task.kind == CitizenTaskKind::Talk &&
                        other->task.partner == c.id &&
                        (!activeWorkplace || !shift || c.breakUntil > minute) &&
                        (c.child || !shift || activeWorkplace ||
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
                valid = (generalLabor || !shift) && c.homeId == c.task.object &&
                        map.objectState().completedObject(c.homeId);
            }
            if (c.task.kind == CitizenTaskKind::Build)
            {
                const auto* site =
                    map.objectState().constructionSite(c.task.site);
                valid = generalLabor && site &&
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
                        ((destination &&
                          destination->kind == InventoryKind::Home &&
                          !c.child) ||
                         generalLabor ||
                         (shift && w && destination &&
                          (w->objectTypeId ==
                               SettlementObjectTypes::Stockpile ||
                           w->objectTypeId == SettlementObjectTypes::Market ||
                           w->objectTypeId ==
                               SettlementObjectTypes::TradeDepot) &&
                          w->objectId == destination->objectId));
                }
            }
            if (c.task.kind == CitizenTaskKind::Gather ||
                c.task.kind == CitizenTaskKind::Demolish)
            {
                valid = generalLabor;
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
        // Capture terminal health/needs changes before deceased entities leave.
        if (citizenDied)
        {
            citizens.recordAttributes(minute, 0);
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
            if (c.militaryDeployed || c.inFishingBoat ||
                !map.grid().isValidPosition(c.tilePosition))
            {
                continue;
            }
            if (manageToddler(map, citizens, c, minute))
            {
                continue;
            }
            if (c.exitingHomeId && !c.path.empty())
            {
                continue;
            }
            startBreak(map, c, minute);
            if (c.task.kind == CitizenTaskKind::None && c.explicitMovement &&
                !c.path.empty())
            {
                continue;
            }
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
                        CitizenRoutePlan planned(c);
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
            // An occupied pasture must still be able to gather its second and
            // later animals. Do not wait forever in the generic tending task.
            if (c.task.kind == CitizenTaskKind::Work &&
                map.animals.hasOrders() && policy.isWorkTime(minute) &&
                minute >= c.nextWorkCheckMinutes)
            {
                const auto* job = map.employment().workplace(c.workplaceId);
                if (job && job->operational &&
                    job->objectTypeId == SettlementObjectTypes::Pastureland)
                {
                    c.nextWorkCheckMinutes = minute + policy.retryMinutes;
                    if (chooseAnimalWork(map, citizens, c, minute))
                    {
                        continue;
                    }
                }
            }
            if (c.task.kind == CitizenTaskKind::Home && c.path.empty())
            {
                chooseSocial(map, citizens, c, minute);
            }
            if (c.task.kind == CitizenTaskKind::None ||
                (c.task.kind == CitizenTaskKind::Home &&
                 (!c.workplaceId ||
                  pastureWorkerAvailableForGeneralLabor(map, c) ||
                  policy.isWorkTime(minute))))
            {
                decide(map, citizens, c, minute);
            }
        }
        // A full sweep otherwise returns to the same first citizen forever.
        // Rotate priority as well as coverage so late arrivals share path
        // credit.
        if (count && count == citizens.citizens_.size())
        {
            ++decisionCursor_;
        }
        citizens.tickMovement(
            map,
            elapsed,
            [&](const SettlementCitizen& c, SettlementTilePosition next)
            {
                if (c.task.kind == CitizenTaskKind::AnimalWork &&
                    c.task.delivering)
                {
                    map.animals.follow(c.task.animal, c.id, next, map);
                }
            }
        );
        map.animals.tick(map, citizens, minute, elapsed);
        for (auto& c : citizens.citizens_)
        {
            if (!c.militaryDeployed)
            {
                if (c.path.empty() && (c.task.kind == CitizenTaskKind::Gather ||
                                       c.task.kind == CitizenTaskKind::Build ||
                                       c.task.kind == CitizenTaskKind::Work))
                {
                    c.workAnimationMinutes += elapsed;
                }
                execute(map, citizens, c, minute, elapsed);
            }
        }
        assignHomes(map, citizens);
        produce(map, citizens, minute, elapsed);
        pathCredit_ -= double(initialPaths - pathsRemaining_);
        citizens.recordPopulation(minute + elapsed);
        citizens.recordAttributes(minute, elapsed);
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
        // Lack of shared path credit is not a failed job search. Retry on the
        // next turn instead of synchronizing every worker to a five-minute
        // poll.
        struct RetryWhenBudgetExhausted
        {
            SettlementCitizen& citizen;
            const std::size_t& remaining;
            double minute;
            ~RetryWhenBudgetExhausted()
            {
                if (!remaining && (citizen.task.kind == CitizenTaskKind::None ||
                                   citizen.task.kind == CitizenTaskKind::Home))
                {
                    citizen.nextWorkCheckMinutes = minute;
                }
            }
        } retry{c, pathsRemaining_, minute};
        c.nextWorkCheckMinutes = minute + policy.retryMinutes;
        const bool generalLabor =
            !c.workplaceId || pastureWorkerAvailableForGeneralLabor(map, c);
        if (!c.child && c.homeId)
        {
            const auto home = map.logistics.forObject(c.homeId);
            const auto* fuel = map.logistics.inventory(home);
            if (fuel && fuel->amount(SettlementResourceTypes::Lumber) <= 2 &&
                chooseHaul(map, citizens, c, minute, home))
            {
                return;
            }
        }
        if (!c.child && generalLabor)
        {
            if (!policy.isWorkTime(minute) &&
                chooseSocial(map, citizens, c, minute))
            {
                return;
            }
            if (chooseConstruction(map, citizens, c, minute) ||
                chooseAnimalWork(map, citizens, c, minute) ||
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
                     workplace.objectTypeId == SettlementObjectTypes::Market ||
                     workplace.objectTypeId ==
                         SettlementObjectTypes::TradeDepot) &&
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
                if (workplace.objectTypeId ==
                        SettlementObjectTypes::Pastureland &&
                    chooseAnimalWork(map, citizens, c, minute))
                {
                    return;
                }
                if(workplace.objectTypeId==SettlementObjectTypes::Graveyard && chooseBurial(map,citizens,c,minute)) return;
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
        if (generalLabor || !policy.isWorkTime(minute))
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
        if(c.task.kind==CitizenTaskKind::Burial)
        { executeBurial(map,citizens,c,minute,elapsed); return; }
        if (c.inFishingBoat)
        {
            c.activity = c.path.empty() ? CitizenActivity::Fishing
                                        : CitizenActivity::TravelingToWork;
            return;
        }
        if (c.task.kind == CitizenTaskKind::AnimalWork)
        {
            executeAnimalWork(map, citizens, c, minute, elapsed);
            return;
        }
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
            auto* parent = citizens.mutableCitizen(c.task.partner);
            if (!parent)
            {
                finish(map, c, minute);
                return;
            }
            if (parent->insideHome && !c.insideHome)
            {
                enterHome(map, citizens, c);
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
                c.modifyAttributes({{AttributeEffect::ParentFeeding, -fed}});
                parent->modifyAttributes(
                    {{AttributeEffect::FeedingChildren, fed * share}}
                );
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
                (!source || !map.commerce.canAccessMeal(map, *source, c, citizens)) ||
                (market && !map.commerce.marketOpen(map, citizens, marketId)))
            {
                finish(map, c, minute);
                return;
            }
            const auto* mealReservation = map.logistics.reservation(c.id);
            const std::string mealResource =
                mealReservation ? mealReservation->resource : "";
            // Recheck at the actual meal, not only when the route was planned:
            // fresh ordinary food may have arrived while this person walked.
            if (!mealReservation || mealReservation->pickedUp ||
                !source || source->amount(mealResource) < mealReservation->amount ||
                !map.logistics.canEat(mealResource))
            {
                finish(map, c, minute);
                c.nextWorkCheckMinutes = minute;
                return;
            }
            if (source && map.commerce.payForMeal(map, *source, c, citizens) &&
                map.logistics.pickUp(c.id))
            {
                map.commerce
                    .recordFlow(c.task.source, {}, mealResource, 1, c.id);
                map.commerce.recordMeal(c, publicFood && price == 0);
                c.modifyAttributes(
                    {{AttributeEffect::Meals, -policy.mealRestoration}}
                );
                planMeal(c, map);
            }
            finish(map, c, minute);
        }
        else if (c.task.kind == CitizenTaskKind::Haul)
        {
            const auto* target = map.logistics.inventory(c.task.destination);
            const auto* reserved = map.logistics.reservation(c.id);
            if (target && target->kind == InventoryKind::TradeDepot &&
                (!reserved || map.trade.exportTarget(target->objectId, reserved->resource) <=
                                  target->amount(reserved->resource)))
            {
                finish(map, c, minute);
                return;
            }
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
                    !map.logistics.importsMaySupply(*sourceInventory, destination->kind) ||
                    map.commerce.affordableTradeUnits(
                        *sourceInventory,
                        *destination,
                        copy.amount,
                        &citizens,
                        copy.resource, c.task.treasuryPurchase
                    ) < copy.amount ||
                    (market && sourceInventory &&
                     sourceInventory->kind == InventoryKind::Keep))
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
                // The reservation and source are rechecked immediately before
                // payment. Commerce changes no inventories, so pickup can then
                // commit without granting goods on a failed purchase.
                if (copy.pickedUp ||
                    sourceForTrade.amount(copy.resource) < copy.amount ||
                    !map.commerce.buyGoods(
                        sourceForTrade,
                        destinationForTrade,
                        copy.amount,
                        &citizens,
                        copy.resource, c.task.treasuryPurchase
                    ))
                {
                    finish(map, c, minute);
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
                        c.carriedAmount, {}, c.task.treasuryPurchase
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
            const double required =
                road ? policy.roadMinutes : policy.constructionMinutes;
            double labor = elapsed;
            if (!road &&
                site->objectTypeId != SettlementObjectTypes::Pastureland &&
                site->laborMinutes + labor >= required &&
                std::any_of(
                    map.animals.all().begin(),
                    map.animals.all().end(),
                    [&](const auto& animal)
                    {
                        return animal.health > 0 &&
                               footprint.contains(animal.tilePosition);
                    }
                ))
            {
                // Keep the unfinished site escapable until its last animal
                // has left. Workers can continue all other construction.
                labor = std::min(
                    labor,
                    std::max(0., required - site->laborMinutes - .001)
                );
            }
            const auto object =
                map.objectState()
                    .build(c.task.site, labor, required, c.tilePosition);
            if (object)
            {
                if (const auto* delivered = map.logistics.inventory(
                        map.logistics.forSite(c.task.site)
                    ))
                {
                    for (const auto& goods : delivered->goods)
                    {
                        map.commerce.recordConsumption(
                            goods.resource,
                            goods.amount
                        );
                    }
                }
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
            const auto* workTerrain = map.grid().tile(c.task.workTile);
            const bool miningMountain =
                c.task.kind == CitizenTaskKind::Gather && workTerrain &&
                workTerrain->terrain == TerrainType::Mountain;
            c.task.laborMinutes += elapsed;
            if (c.task.laborMinutes < (miningMountain
                                           ? policy.mountainMiningMinutes
                                       : c.task.kind == CitizenTaskKind::Gather
                                           ? policy.gatheringMinutes
                                           : policy.demolitionMinutes))
            {
                return;
            }
            if (c.task.kind == CitizenTaskKind::Gather)
            {
                if (miningMountain)
                {
                    auto* tile = map.grid().tile(c.task.workTile);
                    tile->terrain = TerrainType::Land;
                    tile->rockFloor = true;
                    tile->caveInterior = tile->relief != ReliefType::Hills;
                    map.grid().markExcavated(c.task.workTile);
                    map.objectState().terrainChanged();
                    map.logistics.drop(
                        c.task.workTile,
                        SettlementResourceTypes::Stone,
                        4,
                        minute
                    );
                    map.commerce.recordProduction(
                        SettlementResourceTypes::Stone,
                        4
                    );
                }
                const auto feature = map.naturalFeatures().at(c.task.workTile);
                if (feature.kind != NaturalFeatureKind::None)
                {
                    map.logistics.drop(
                        c.task.workTile,
                        feature.kind == NaturalFeatureKind::Tree
                            ? SettlementResourceTypes::Lumber
                        : feature.kind == NaturalFeatureKind::Wheat
                            ? SettlementResourceTypes::Wheat
                            : SettlementResourceTypes::Stone,
                        4,
                        minute
                    );
                    map.commerce.recordProduction(
                        feature.kind == NaturalFeatureKind::Tree
                            ? SettlementResourceTypes::Lumber
                        : feature.kind == NaturalFeatureKind::Wheat
                            ? SettlementResourceTypes::Wheat
                            : SettlementResourceTypes::Stone,
                        4
                    );
                    map.naturalFeatures().harvest(c.task.workTile, minute);
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
            if (c.task.object && !enterHome(map, citizens, c))
            {
                if (!map.objectState().completedObject(c.homeId))
                {
                    finish(map, c, minute);
                }
                return;
            }
            const auto* home = map.objectState().completedObject(c.task.object);
            if (home && c.bedHomeId == home->id && c.bedSlot >= 0)
            {
                c.task.target = homeBedPosition(*home, c.bedSlot);
            }
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
                if (home && home->door)
                {
                    appendRoomPath(
                        c.path,
                        p,
                        c.task.target,
                        home->footprint,
                        *home->door
                    );
                }
                else
                {
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
                }
                c.pathIndex = 0;
                c.stepProgress = 0;
                c.stepDuration = citizens.navigation_.stepCost(
                    map,
                    c.tilePosition,
                    c.path.front(),
                    citizens.movementPolicy
                );
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
            c.modifyAttributes(
                {{AttributeEffect::Sleep,
                  policy.sleepEnergyPerMinute * restingMinutes}}
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
            if (enterHome(map, citizens, c))
            {
                c.activity = CitizenActivity::AtHome;
                if (!c.child && c.youngDependents > 0 &&
                    minute >= c.nextHomeWander)
                {
                    const auto* home =
                        map.objectState().completedObject(c.homeId);
                    const auto random =
                        GenerationNoise::mix(c.id.value() ^ ++c.choiceSequence);
                    constexpr SettlementTilePosition
                        offsets[]{{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
                    c.nextHomeWander = minute + policy.childcareWanderMinutes;
                    for (int i = 0; home && i < 4; ++i)
                    {
                        const auto offset = offsets[(random + i) % 4];
                        const SettlementTilePosition next{
                            c.tilePosition.x + offset.x,
                            c.tilePosition.y + offset.y
                        };
                        if (!buildingInterior(
                                 home->footprint,
                                 home->objectTypeId
                            )
                                 .contains(next))
                        {
                            continue;
                        }
                        c.path = {next};
                        c.pathIndex = 0;
                        c.stepProgress = 0;
                        c.stepDuration = citizens.navigation_.stepCost(
                            map,
                            c.tilePosition,
                            next,
                            citizens.movementPolicy
                        );
                        c.destination = next;
                        c.explicitMovement = true;
                        break;
                    }
                }
            }
        }
        else if (c.task.kind == CitizenTaskKind::Home)
        {
            if (c.child && c.ageYears < policy.independentEatingAge)
            {
                return;
            }
            if (c.task.endMinute > minute)
            {
                return;
            }
            if (c.task.endMinute > 0)
            {
                c.task.endMinute = 0;
                const auto* home = map.objectState().completedObject(c.homeId);
                if (home && route(map, citizens, c, home->footprint, false))
                {
                    c.task.endMinute = 0;
                }
                return;
            }
            if (!enterHome(map, citizens, c))
            {
                // A failed/invalidated return route must retry, not leave a
                // permanent Home task standing outside with no path.
                if (minute >= c.nextDecisionMinute)
                {
                    const auto* home =
                        map.objectState().completedObject(c.homeId);
                    if (home)
                    {
                        route(map, citizens, c, home->footprint, false);
                    }
                    c.nextDecisionMinute = minute + policy.retryMinutes;
                }
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
                    const int radius = c.child ? policy.childNeighborhoodRadius
                                               : policy.leisureRadius;
                    const SettlementTilePosition nearby{
                        outside.x + int(random % (2 * radius + 1)) - radius,
                        outside.y + int((random >> 8) % (2 * radius + 1)) -
                            radius
                    };
                    CitizenRoutePlan planned(c);
                    if (route(map, citizens, planned, {nearby, 1, 1}, true) &&
                        childRouteIsLocal(map, c, planned))
                    {
                        planned.applyTo(c);
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
                    c.stepDuration = citizens.navigation_.stepCost(
                        map,
                        c.tilePosition,
                        target,
                        citizens.movementPolicy
                    );
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
            auto* other = citizens.mutableCitizen(c.task.partner);
            if (!other ||
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
                c.modifyAttributes(
                    {{AttributeEffect::Socializing,
                      policy.talkingHappinessPerMinute * elapsed}}
                );
                c.enforceHappinessModifiers();
                if (c.id < other->id)
                {
                    const double gain =
                        policy.familiarityPerTalkMinute *
                        std::min(elapsed, c.task.endMinute - minute);
                    c.familiarities[other->id] = std::min(
                        policy.maximumFamiliarity,
                        c.familiarityWith(other->id) + gain
                    );
                    other->familiarities[c.id] = c.familiarities[other->id];
                }
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
            if (const auto* mine = miningJob(object->objectTypeId))
            {
                const auto inventory = map.logistics.forObject(object->id);
                const auto* site = map.mining.find(object->id);
                if (map.logistics.receivable(inventory, mine->resource) > 0 &&
                    (!site || !site->exhausted))
                {
                    c.activity = CitizenActivity::Mining;
                }
                const auto& footprint = object->footprint;
                bool atEntrance = false;
                for (int entrance = 0;
                     entrance < quarryEntranceCount(footprint.width);
                     ++entrance)
                {
                    atEntrance |= c.tilePosition == quarryEntrance(
                                                        footprint.topLeft,
                                                        footprint.width,
                                                        entrance,
                                                        footprint.height
                                                    );
                }
                if (mine->tunnels && map.mining.depth(*object) >= .98 &&
                    !atEntrance)
                {
                    finish(map, c, minute);
                    return;
                }
                if (mine->tunnels && map.mining.depth(*object) >= .98)
                {
                    // Workers pass through a shared entrance to separate
                    // subterranean work faces; they do not stack on the
                    // surface.
                    c.activity = CitizenActivity::UndergroundMining;
                }
            }
            if (object->objectTypeId == SettlementObjectTypes::FishingGrounds)
            {
                if (!c.boatRoute.empty())
                {
                    c.inFishingBoat = true;
                    c.boatReturning = false;
                    c.boatFishery = object->id;
                    c.boatLanding = c.tilePosition;
                    c.path = std::move(c.boatRoute);
                    c.pathIndex = 0;
                    c.stepProgress = 0;
                    c.stepDuration = 1;
                    c.destination = c.path.back();
                    c.explicitMovement = true;
                }
                c.activity = CitizenActivity::Fishing;
            }
            if (object->objectTypeId == SettlementObjectTypes::Pastureland)
            {
                executePastureWork(map, citizens, c, minute, elapsed);
                return;
            }
            if (object->objectTypeId == SettlementObjectTypes::Market ||
                object->objectTypeId == SettlementObjectTypes::TradeDepot)
            {
                // An idle depot attendant must notice a newly funded order
                // without needing a shift change or reassignment.
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
            if (c.militaryDeployed)
            {
                continue;
            }
            if (caregivingAtWorkTime(map, c, minute))
            {
                const auto* workplace =
                    map.employment().workplace(c.workplaceId);
                ++attendance[workplace->objectId];
                continue;
            }
            if (policy.isWorkTime(minute) && c.breakUntil <= minute &&
                c.task.kind == CitizenTaskKind::Work)
            {
                const auto* object =
                    map.objectState().completedObject(c.task.object);
                if (object &&
                    object->objectTypeId == SettlementObjectTypes::Pastureland)
                {
                    // The first contained animal activates the pasture. From
                    // then on every physically present assigned worker counts
                    // as productive husbandry, whether or not this minute's
                    // duty is a specific tending interaction.
                    if (map.animals.containedCount(object->id) == 0 ||
                        !object->footprint.contains(c.tilePosition))
                    {
                        continue;
                    }
                }
                else if (
                    c.boatReturning || !c.path.empty() ||
                    c.tilePosition != c.destination
                )
                {
                    continue;
                }
                ++attendance[c.task.object];
            }
        }
        for (const auto& [objectId, workers] : attendance)
        {
            if (produceIndustry(map, objectId, workers, minute, elapsed))
            {
                continue;
            }
            const auto* object = map.objectState().completedObject(objectId);
            if (object &&
                object->objectTypeId == SettlementObjectTypes::Pastureland)
            {
                map.animals.produce(map, objectId, workers, minute, elapsed);
                continue;
            }
            if (object &&
                object->objectTypeId == SettlementObjectTypes::LoggingGrounds)
            {
                const auto inventory = map.logistics.forObject(objectId);
                const int space = map.logistics.freeSpace(inventory);
                if (space > 0)
                {
                    const double rate = loggingProductionPerMinute(
                        object->footprint.width * object->footprint.height,
                        workers
                    );
                    const int lumber = std::min(
                        space,
                        int(map.objectState()
                                .accrueProduction(objectId, elapsed * rate))
                    );
                    if (lumber > 0 && map.logistics.add(
                                          inventory,
                                          SettlementResourceTypes::Lumber,
                                          lumber,
                                          minute
                                      ))
                    {
                        map.commerce.recordFlow(
                            {},
                            inventory,
                            SettlementResourceTypes::Lumber,
                            lumber
                        );
                    }
                }
                continue;
            }
            if (!object ||
                object->objectTypeId != SettlementObjectTypes::FishingGrounds)
            {
                continue;
            }
            const double rate = fisheryProductionPerMinute(
                map.fishingBoats.productiveWater(map, *object),
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
            if (fish > 0 && map.logistics.add(
                                inventory,
                                SettlementResourceTypes::Fish,
                                fish,
                                minute
                            ))
            {
                map.commerce.recordFlow(
                    {},
                    inventory,
                    SettlementResourceTypes::Fish,
                    fish
                );
            }
        }
    }

} // namespace Paladin

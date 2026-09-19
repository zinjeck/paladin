#include "simulation/MilitarySystem.h"
#include "simulation/BattleSystem.h"
#include "simulation/WorldLandNavigation.h"
#include "simulation/CitizenshipSystem.h"
#include "simulation/WorldShipmentSystem.h"
#include "world/World.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/SettlementIndustry.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace Paladin
{
    const char* militaryResultText(MilitaryResult r) noexcept
    {
        switch (r)
        {
        case MilitaryResult::InBattle: return "Resolve the current battle or retreat first.";
        case MilitaryResult::Success: return "Order accepted.";
        case MilitaryResult::NotOwned: return "This is not your realm's unit or city.";
        case MilitaryResult::NoBarracksEmployee: return "Hire an unassigned adult at a completed barracks first.";
        case MilitaryResult::ReturnHome: return "Stop in a friendly city to organize the unit.";
        case MilitaryResult::PersonnelOrigin: return "Discharge these soldiers in their own city with a free barracks seat.";
        case MilitaryResult::EmptyUnit: return "Assign soldiers before marching.";
        case MilitaryResult::InvalidDestination: return "Choose a land destination, not water or solid mountains.";
        case MilitaryResult::NoLandRoute: return "No passable land route was found.";
        case MilitaryResult::UnitLimit: return "This realm has reached its unit limit.";
        default: return "The selected unit no longer exists.";
        }
    }
    SettlementMap* MilitarySystem::map(World& world, SettlementId id)
    {
        auto* city = world.settlement(id);
        return city ? city->simulationState().localMap_.get() : nullptr;
    }
    SettlementCitizen* MilitarySystem::person(World& world, const Soldier& soldier)
    {
        auto* city = world.settlement(soldier.homeSettlementId());
        if (!city) return nullptr;
        auto& people = city->simulationState().citizens_.citizens_;
        const auto it = std::find_if(people.begin(), people.end(), [&](const auto& c)
        { return c.id == soldier.sourceCitizenId(); });
        return it == people.end() ? nullptr : &*it;
    }
    bool MilitarySystem::presentAt(const World& world, const Army& unit, SettlementId cityId) noexcept
    {
        const auto* city = world.settlement(cityId);
        return city && !unit.moving() && unit.position() == city->position();
    }
    SettlementId MilitarySystem::stationAt(const World& world, const Army& unit) noexcept
    {
        if (unit.moving()) return {};
        for (const auto& city : world.settlements())
            if (city.ownerRealmId() == unit.ownerRealmId() && presentAt(world, unit, city.id()))
                return city.id();
        return {};
    }
    bool MilitarySystem::canOrganize(const World& world, const Army& unit) noexcept
    {
        const auto* city = world.settlement(stationAt(world, unit));
        return !unit.engagedOpponent_ && city && city->simulationState().localMap();
    }
    std::size_t MilitarySystem::available(const World& world, SettlementId id) noexcept
    {
        const auto* city = world.settlement(id);
        const auto* local = city ? city->simulationState().localMap() : nullptr;
        if (!local) return 0;
        // Read the actual payroll, including changes made while paused, rather
        // than last tick's cached Soldier entities. Dead/dismissed staff aren't reserves.
        std::size_t count = 0;
        for (const auto& c : city->simulationState().citizens().citizens())
        {
            const auto* w = local->employment().workplace(c.workplaceId);
            if (city->simulationState().citizens().militaryEligible(c) && !c.militaryDeployed && !c.militaryUnitId &&
                w && w->operational && w->objectTypeId == SettlementObjectTypes::Barracks &&
                local->objectState().completedObject(w->objectId)) ++count;
        }
        return count;
    }
    std::size_t MilitarySystem::releasable(const World& world, const Army& unit) noexcept
    {
        const auto station = stationAt(world, unit);
        const auto* city = world.settlement(station);
        const auto* local = city ? city->simulationState().localMap() : nullptr;
        if (!local) return 0;
        std::size_t freeSeats = 0, employed = 0, field = 0;
        for (const auto& w : local->employment().workplaces())
            if (w.operational && w.objectTypeId == SettlementObjectTypes::Barracks &&
                local->objectState().completedObject(w.objectId))
                freeSeats += w.maximumCapacity - std::min(w.capacity,w.maximumCapacity);
        for (const auto id : unit.soldiers())
        {
            const auto* s = world.soldier(id);
            if (!s || s->homeSettlementId() != station) continue;
            const auto* c = city->simulationState().citizens().citizen(s->sourceCitizenId());
            const auto* w = c ? local->employment().workplace(c->workplaceId) : nullptr;
            if (w && w->operational && w->objectTypeId == SettlementObjectTypes::Barracks)
                ++employed;
            else ++field;
        }
        return employed + std::min(field,freeSeats);
    }
    void MilitarySystem::synchronize(World& world, double minute)
    {
        CitizenshipSystem::synchronize(world);
        std::unordered_set<SoldierId, StrongIdHash> retained;
        std::unordered_map<SoldierId, ArmyId, StrongIdHash> roster;
        retained.reserve(world.soldiers().size());
        roster.reserve(world.soldiers().size());
        for (const auto& unit : world.armies())
            for (const auto id : unit.soldiers()) roster.try_emplace(id,unit.id());
        for (auto& city : world.settlements())
        {
            auto* local = map(world, city.id());
            auto& people = city.simulationState().citizens_;
            if (local) local->employment().synchronize(local->objectState(), people);
            for (auto& c : people.citizens_)
            {
                const auto* w = local ? local->employment().workplace(c.workplaceId) : nullptr;
                auto* fieldSoldier = world.soldiers_.find(c.soldierId);
                const auto* fieldUnit = fieldSoldier ? world.army(fieldSoldier->unitId()) : nullptr;
                const auto membership = roster.find(c.soldierId);
                if (c.militaryDeployed && c.health > 0 && !c.child && fieldSoldier && fieldUnit &&
                    fieldSoldier->home_ == city.id() && fieldSoldier->person_ == c.id &&
                    membership != roster.end() && membership->second == fieldUnit->id())
                {
                    // The source pair preserves the one person's family/savings
                    // identity, NOT city membership, payroll or army allegiance.
                    // Destroying/capturing the old barracks cannot delete field troops.
                    c.militaryUnitId = fieldUnit->id();
                    retained.insert(c.soldierId);
                    continue;
                }
                if (!local)
                {
                    c.soldierId = {}; c.militaryUnitId = {}; c.militaryDeployed = false;
                    continue;
                }
                if (c.militaryUnitId)
                {
                    const auto* unit = world.army(c.militaryUnitId);
                    if (unit && unit->ownerRealmId() != city.ownerRealmId())
                    {
                        if (auto* old = world.soldiers_.find(c.soldierId)) old->unit_ = {};
                        c.militaryUnitId = {}; c.militaryDeployed = false;
                        c.hasVisualSnapshot = false;
                        local->activities.finish(*local, c, minute);
                    }
                }
                const bool enlisted = people.militaryEligible(c) && w && w->operational &&
                                      w->objectTypeId == SettlementObjectTypes::Barracks;
                auto* soldier = world.soldiers_.find(c.soldierId);
                if (soldier && (soldier->home_ != city.id() || soldier->person_ != c.id ||
                                !enlisted || soldier->barracks_ != w->objectId))
                    soldier = nullptr;
                if (enlisted && !soldier)
                {
                    // A changed payroll seat invalidates the old deployment.
                    // Never leave the replacement reserve hidden in its city.
                    c.militaryUnitId = {}; c.militaryDeployed = false;
                    c.hasVisualSnapshot = false;
                    local->activities.finish(*local, c, minute);
                    c.soldierId = world.soldiers_.create(city.id(), c.id, w->objectId);
                    soldier = world.soldiers_.find(c.soldierId);
                }
                if (soldier)
                {
                    retained.insert(soldier->id());
                    if (soldier->unit_ && !world.army(soldier->unit_)) soldier->unit_ = {};
                    c.militaryUnitId = soldier->unit_;
                }
                else if (c.soldierId || c.militaryDeployed)
                {
                    // Removal/death never leaves a phantom roster entry or a
                    // person permanently hidden after their building is lost.
                    c.soldierId = {}; c.militaryUnitId = {};
                    c.militaryDeployed = false;
                    c.hasVisualSnapshot = false;
                    local->activities.finish(*local, c, minute);
                }
            }
        }
        std::vector<SoldierId> removed;
        for (const auto& soldier : world.soldiers())
            if (!retained.contains(soldier.id())) removed.push_back(soldier.id());
        for (auto id : removed) world.soldiers_.erase(id);
        for (auto& unit : world.armies())
        {
            std::erase_if(unit.soldiers_, [&](SoldierId id)
            {
                const auto* s = world.soldier(id);
                return !s || s->unitId() != unit.id();
            });
            if (unit.soldiers_.empty())
            {
                unit.route_.clear(); unit.routeIndex_ = 0; unit.stepMinutes_ = 0;
                // Personnel losses do not conjure replacement recruits. The
                // empty record can only be reinforced where friendly reserves exist.
            }
            unit.station_ = stationAt(world, unit);
        }
    }
    ArmyId MilitarySystem::maintainStrategicGarrison(World& world, RealmId actor, SettlementId id, int target, int changeLimit)
    {
        auto* city=world.settlement(id); const auto* realm=world.realm(actor);
        if(!city || !realm || !realm->aiControlled || city->ownerRealmId()!=actor || map(world,id)) return {};
        target=std::clamp(target,0,96); changeLimit=std::clamp(changeLimit,0,8);
        auto& state=city->simulationState(); auto& people=state.citizens_;
        people.configureCommunity(id,actor,city->primaryCultureId(),realm->laws,realm->citizenshipResearched);
        ArmyId unitId;
        for(const auto& unit:world.armies())
            if(unit.ownerRealmId()==actor && presentAt(world,unit,id) &&
                std::all_of(unit.soldiers().begin(),unit.soldiers().end(),[&](SoldierId soldier)
                { const auto* s=world.soldier(soldier); return s && s->homeSettlementId()==id; }))
            { unitId=unit.id(); break; }
        if(!unitId && target>0)
        {
            if(std::count_if(world.armies().begin(),world.armies().end(),[&](const auto& u){return u.ownerRealmId()==actor;})>=MaximumUnitsPerRealm) return {};
            unitId=world.createArmy(city->position()); world.assignArmyToRealm(unitId,actor);
            auto* unit=world.army(unitId); unit->station_=id; unit->name_=std::string(city->name())+" Guard";
        }
        auto* unit=world.army(unitId); if(!unit) return {};
        int changes=0;
        while(int(unit->soldierCount())>target && changes++<changeLimit)
        {
            const auto sid=unit->soldiers_.back(); const auto* soldier=world.soldier(sid);
            auto* c=soldier?person(world,*soldier):nullptr;
            if(!c || !state.population_.transferResidents(1)) break;
            c->militaryDeployed=false; c->militaryUnitId={}; c->soldierId={};
            c->activity=CitizenActivity::Idle; c->workplaceId={}; c->hasVisualSnapshot=false;
            unit->soldiers_.pop_back(); world.soldiers_.erase(sid); ++people.version_;
        }
        returnSurplus(world,*unit,double(world.time().totalGameMinutes()));
        changes=0;
        while(int(unit->soldierCount())<target && changes<changeLimit && city->population()>8)
        {
            // Protect two civilian days of food and provision the new person
            // with an actual field pack. No abstract food or phantom recruits.
            const double spare=state.stockpile().amount("rations")+
                std::max(0.,state.stockpile().amount("food")-2.*double(city->population()));
            if(spare<RationsPerSoldier) break;
            SettlementCitizen* candidate=nullptr;
            for(int attempts=0;attempts<32 && !candidate;++attempts)
            {
                for(auto& c:people.citizens_)
                    if(!c.soldierId && !c.militaryDeployed && people.militaryEligible(c)) { candidate=&c; break; }
                if(candidate || people.residentCount()>=city->population() || !people.appendCitizens(1,false)) break;
            }
            if(!candidate || !state.population_.transferResidents(-1)) break;
            candidate->soldierId=world.soldiers_.create(id,candidate->id,SettlementObjectId{});
            candidate->militaryUnitId=unitId; candidate->militaryDeployed=true;
            candidate->workplaceId={}; candidate->insideHome=false;
            world.soldiers_.find(candidate->soldierId)->unit_=unitId;
            unit->soldiers_.push_back(candidate->soldierId); ++people.version_; ++changes;
            resupply(world,*unit,double(world.time().totalGameMinutes()));
        }
        return unitId;
    }
    ArmyId MilitarySystem::createUnit(World& world, RealmId actor, SettlementId home)
    {
        auto* city = world.settlement(home);
        if (!actor || !city || city->ownerRealmId() != actor || !map(world, home)) return {};
        if (std::count_if(world.armies().begin(), world.armies().end(), [&](const auto& u)
            { return u.ownerRealmId() == actor; }) >= MaximumUnitsPerRealm) return {};
        synchronize(world, double(world.time().totalGameMinutes()));
        if (available(world, home) == 0) return {};
        const auto id = world.createArmy(city->position());
        auto* unit = world.army(id);
        unit->station_ = home;
        unit->name_ = "Unit " + std::to_string(id.value());
        world.assignArmyToRealm(id, actor);
        // A newly created unit already has a real soldier and is immediately
        // drawable/selectable. Never manufacture an independent headcount.
        if (resizeUnit(world, actor, id, 1) != MilitaryResult::Success)
        { world.armies_.erase(id); return {}; }
        return id;
    }
    void MilitarySystem::setDeployed(World& world, Army& unit, bool deployed, double minute, const PersonnelIndex* indexed)
    {
        for (auto id : unit.soldiers_)
        {
            const auto* soldier = world.soldier(id);
            if (!soldier) continue;
            SettlementCitizen* c = nullptr;
            if (indexed)
            {
                const auto found = indexed->find(id);
                if (found != indexed->end()) c = found->second;
            }
            else c = person(world, *soldier);
            auto* local = map(world, soldier->homeSettlementId());
            if (!c || !local || c->militaryDeployed == deployed) continue;
            local->activities.finish(*local, *c, minute);
            c->path.clear(); c->pathIndex = 0; c->stepProgress = 0;
            c->explicitMovement = false;
            if (deployed && c->workplaceId)
            {
                local->employment().citizenDeparted(c->workplaceId);
                c->workplaceId = {};
                if (auto* owner = world.settlement(soldier->homeSettlementId()))
                    ++owner->simulationState().citizens_.version_;
            }
            c->militaryDeployed = deployed;
            c->insideHome = false;
            c->hasVisualSnapshot = false;
            c->activity = CitizenActivity::Idle;
            if (auto* home = world.settlement(soldier->homeSettlementId()))
                home->simulationState().synchronizeCitizenPopulation();
        }
    }
    void MilitarySystem::returnSurplus(World& world, Army& unit, double minute)
    {
        const int capacity = int(unit.soldierCount()) * RationsPerSoldier;
        int excess = std::max(0, unit.rations_ - capacity);
        auto* local = map(world, stationAt(world, unit));
        if(!local && excess>0)
        {
            if(auto* city=world.settlement(stationAt(world,unit)); city && city->simulationState().stockpile().addAmount("rations",excess)) unit.rations_-=excess;
            return;
        }
        if (!local || !canOrganize(world, unit) || excess == 0) return;
        for (const auto& w : local->employment().workplaces())
        {
            if (!w.operational || w.objectTypeId != SettlementObjectTypes::Barracks) continue;
            const auto inventory = local->logistics.forObject(w.objectId);
            const int amount = std::min(excess, local->logistics.receivable(inventory, "rations"));
            if (amount > 0 && local->logistics.add(inventory, "rations", amount, minute))
            { excess -= amount; unit.rations_ -= amount; }
        }
        if (excess > 0)
        {
            // No full-storage deletion: return the remaining physical goods
            // as a haulable pile at the city keep.
            const auto objects = local->objectState().completedObjects();
            const auto keep = std::find_if(objects.begin(), objects.end(), [](const auto& object)
            { return object.objectTypeId == SettlementObjectTypes::CityKeep; });
            if (keep != objects.end())
            {
                local->logistics.drop(keep->footprint.topLeft, "rations", excess, minute);
                unit.rations_ -= excess;
            }
        }
    }
    bool MilitarySystem::restoreEmployment(World& world, const Soldier& soldier)
    {
        auto* c = person(world, soldier);
        auto* local = map(world, soldier.homeSettlementId());
        if (!c || !local) return false;
        const auto* current = local->employment().workplace(c->workplaceId);
        if (current && current->operational && current->objectTypeId == SettlementObjectTypes::Barracks)
            return true;
        auto& people = world.settlement(soldier.homeSettlementId())->simulationState().citizens_;
        // Assign this exact returning person, not whichever unemployed citizen
        // happens to come first in the generic hiring loop.
        for (auto& w : local->employment().workplaces_)
        {
            if (!w.operational || w.objectTypeId != SettlementObjectTypes::Barracks ||
                w.capacity >= w.maximumCapacity) continue;
            c->workplaceId = w.id;
            ++w.capacity;
            ++people.version_;
            if (auto* s = world.soldiers_.find(soldier.id())) s->barracks_ = w.objectId;
            return true;
        }
        return false;
    }
    MilitaryResult MilitarySystem::resizeUnit(World& world, RealmId actor, ArmyId id, int delta)
    {
        synchronize(world, double(world.time().totalGameMinutes()));
        auto* unit = world.army(id);
        if (!unit) return MilitaryResult::InvalidUnit;
        if (!actor || unit->ownerRealmId() != actor) return MilitaryResult::NotOwned;
        if (unit->engagedOpponent_) return MilitaryResult::InBattle;
        if (!canOrganize(world, *unit)) return MilitaryResult::ReturnHome;
        if (delta == 0) return MilitaryResult::Success;
        if (delta > 0)
        {
            int remaining = delta;
            const auto station = stationAt(world, *unit);
            for (auto& s : world.soldiers_.entities())
            {
                if (s.home_ != station || s.unit_) continue;
                s.unit_ = id;
                unit->soldiers_.push_back(s.id());
                if (auto* c = person(world, s)) c->militaryUnitId = id;
                if (--remaining == 0) break;
            }
            if (remaining == delta) return MilitaryResult::NoBarracksEmployee;
        }
        else
        {
            // Use a wide negation so INT_MIN cannot overflow on external input.
            const auto count = std::min<std::size_t>(unit->soldiers_.size(),
                std::size_t(-std::int64_t(delta)));
            std::size_t removed = 0;
            const auto station = stationAt(world, *unit);
            for (std::size_t i = unit->soldiers_.size(); i > 0 && removed < count; --i)
            {
                auto* s = world.soldiers_.find(unit->soldiers_[i-1]);
                // Origin is only a personnel-record key. Cross-city civilian
                // migration is not implemented; never teleport a discharged
                // person to a distant city or create a duplicate reserve.
                if (!s || s->homeSettlementId() != station || !restoreEmployment(world, *s)) continue;
                s->unit_ = {};
                if (auto* c = person(world, *s))
                {
                    c->militaryUnitId = {};
                    c->militaryDeployed = false;
                    c->hasVisualSnapshot = false;
                    c->nextWorkCheckMinutes = 0;
                    world.settlement(s->homeSettlementId())->simulationState().synchronizeCitizenPopulation();
                }
                unit->soldiers_.erase(unit->soldiers_.begin() + std::ptrdiff_t(i-1));
                ++removed;
            }
            if (count > 0 && removed == 0) return MilitaryResult::PersonnelOrigin;
            returnSurplus(world, *unit, double(world.time().totalGameMinutes()));
        }
        return MilitaryResult::Success;
    }
    MilitaryResult MilitarySystem::disbandUnit(World& world, RealmId actor, ArmyId id)
    {
        synchronize(world, double(world.time().totalGameMinutes()));
        const auto* existing = world.army(id);
        if (!existing) return MilitaryResult::InvalidUnit;
        if (!actor || existing->ownerRealmId() != actor) return MilitaryResult::NotOwned;
        if (existing->engagedOpponent_) return MilitaryResult::InBattle;
        const double minute = double(world.time().totalGameMinutes());
        // Demobilization is not hiring back into a barracks. Return each SAME
        // canonical person to their source settlement as an unemployed civilian.
        // This is an immediate administrative return, not a simulated return march.
        struct Return { SoldierId soldier; SettlementId home; SettlementTilePosition tile; };
        std::vector<Return> returns;
        for (const auto soldierId : existing->soldiers())
        {
            const auto* soldier = world.soldier(soldierId);
            auto* c = soldier ? person(world, *soldier) : nullptr;
            auto* local = soldier ? map(world, soldier->homeSettlementId()) : nullptr;
            if (!c) return MilitaryResult::PersonnelOrigin;
            if (!local)
            {
                const auto* home=world.settlement(soldier->homeSettlementId());
                if(!home || !home->simulationState().isInitialized() || std::uint64_t(std::count_if(returns.begin(),returns.end(),[&](const auto& ret){return ret.home==home->id();}))+1 >
                    std::numeric_limits<std::uint64_t>::max()-home->population()) return MilitaryResult::PersonnelOrigin;
                returns.push_back({soldierId,soldier->homeSettlementId(),{0,0}});
                continue;
            }
            SettlementTilePosition anchor = c->tilePosition;
            for (const auto& object : local->objectState().completedObjects())
                if (object.objectTypeId == SettlementObjectTypes::CityKeep)
                { anchor = object.footprint.topLeft; break; }
            SettlementNavigation navigation;
            navigation.synchronize(*local);
            bool found = false;
            SettlementTilePosition destination = anchor;
            for (int radius = 0; radius <= 16 && !found; ++radius)
                for (int y = -radius; y <= radius && !found; ++y)
                    for (int x = -radius; x <= radius; ++x)
                    {
                        if (std::max(std::abs(x), std::abs(y)) != radius) continue;
                        const SettlementTilePosition candidate{anchor.x+x,anchor.y+y};
                        if (navigation.walkable(*local,candidate))
                        { destination=candidate; found=true; break; }
                    }
            if (!found) return MilitaryResult::PersonnelOrigin;
            returns.push_back({soldierId,soldier->homeSettlementId(),destination});
        }
        auto* unit = world.army(id);
        if (returns.empty() && unit->rations_ > 0)
        {
            returnSurplus(world,*unit,minute);
            if (unit->rations_ > 0) return MilitaryResult::ReturnHome;
        }
        // Preflight above is all-or-nothing: never partially discharge a mixed
        // roster because one home cannot currently receive its person.
        for (const auto& ret : returns)
        {
            auto& home = world.settlement(ret.home)->simulationState();
            auto* local = map(world,ret.home);
            auto& c = *person(world,*world.soldier(ret.soldier));
            if(local)
            {
                local->activities.finish(*local,c,minute);
                if (c.workplaceId) local->employment().citizenDeparted(c.workplaceId);
            }
            c.workplaceId={}; c.soldierId={}; c.militaryUnitId={};
            c.militaryDeployed=false; c.insideHome=false;
            c.path.clear(); c.pathIndex=0; c.stepProgress=0;
            c.explicitMovement=false; c.tilePosition=ret.tile;
            c.hasVisualSnapshot=false; c.activity=CitizenActivity::Idle;
            c.nextWorkCheckMinutes=0;
            ++home.citizens_.version_;
            if(local) home.synchronizeCitizenPopulation();
            else static_cast<void>(home.population_.transferResidents(1));
            world.soldiers_.erase(ret.soldier);
        }
        if (!returns.empty() && unit->rations_ > 0)
        {
            // The returning personnel carry the remaining physical pack home.
            auto* local=map(world,returns.front().home);
            if(local) local->logistics.drop(returns.front().tile,"rations",unit->rations_,minute);
            else static_cast<void>(world.settlement(returns.front().home)->simulationState().stockpile().addAmount("rations",unit->rations_));
            unit->rations_=0;
        }
        world.armies_.erase(id);
        return MilitaryResult::Success;
    }

    MilitaryResult MilitarySystem::recruit(World& world, RealmId actor, SettlementId cityId, int delta)
    {
        auto* city = world.settlement(cityId);
        auto* local = map(world, cityId);
        if (!actor || !city || city->ownerRealmId() != actor) return MilitaryResult::NotOwned;
        if (!local || delta == 0) return MilitaryResult::NoBarracksEmployee;
        auto& people = city->simulationState().citizens_;
        local->employment().synchronize(local->objectState(), people);
        bool changed = false;
        // adjustType can select a fully assigned barracks while another has
        // reserves. Try the actual seats; adjust protects every assigned person.
        for (const auto& workplace : local->employment().workplaces())
        {
            if (!workplace.operational || workplace.objectTypeId != SettlementObjectTypes::Barracks)
                continue;
            if (local->employment().adjust(workplace.id, delta > 0 ? 1 : -1, people))
            { changed = true; break; }
        }
        synchronize(world, double(world.time().totalGameMinutes()));
        return changed ? MilitaryResult::Success : MilitaryResult::NoBarracksEmployee;
    }
    MilitaryResult MilitarySystem::orderMove(World& world, RealmId actor, ArmyId id, WorldTilePosition target)
    {
        synchronize(world, double(world.time().totalGameMinutes()));
        auto* unit = world.army(id);
        if (!unit) return MilitaryResult::InvalidUnit;
        if (!actor || unit->ownerRealmId() != actor) return MilitaryResult::NotOwned;
        if (unit->engagedOpponent_) return MilitaryResult::InBattle;
        if (unit->soldiers_.empty()) return MilitaryResult::EmptyUnit;
        const auto& grid = world.grid();
        const auto* tile = grid.tile(target);
        if (!tile || tile->terrain != TerrainType::Land) return MilitaryResult::InvalidDestination;
        // Re-orders begin at the last reached tile. Do not teleport a moving
        // unit to the end of its old route or send it through sea/mountains.
        const bool continuingStep = unit->moving() && unit->stepMinutes_ > 0;
        const auto start = continuingStep ? unit->route_[unit->routeIndex_] : unit->position();
        const double continuedMinutes = continuingStep ? unit->stepMinutes_ : 0;
        const int width = grid.width();
        auto planned = worldLandRoute(grid, start, target, WorldLandMovement::EightWay);
        if (!planned) return MilitaryResult::NoLandRoute;
        std::vector<WorldTilePosition> route(planned->begin() + 1, planned->end());
        if (continuingStep) route.insert(route.begin(), start);
        // Buy/load only already produced rations before leaving supply range.
        resupply(world, *unit, double(world.time().totalGameMinutes()));
        unit->attackTarget_ = {};
        unit->route_ = std::move(route); unit->routeIndex_ = 0;
        unit->stepMinutes_ = continuedMinutes; unit->wrapWidth_ = width;
        unit->station_ = stationAt(world, *unit);
        if (unit->moving())
            setDeployed(world, *unit, true, double(world.time().totalGameMinutes()));
        return MilitaryResult::Success;
    }
    void MilitarySystem::resupply(World& world, Army& unit, double minute)
    {
        if (unit.moving() || unit.soldiers_.empty()) return;
        int needed = std::max(0, int(unit.soldierCount()) * RationsPerSoldier - unit.rations_);
        for (const auto& city : world.settlements())
        {
            if (needed <= 0) break;
            if (city.ownerRealmId() != unit.ownerRealmId() || !presentAt(world, unit, city.id())) continue;
            auto* local = map(world, city.id());
            if (!local)
            {
                auto& supply=world.settlement(city.id())->simulationState().stockpile();
                for(const auto* resource:{"rations","food"})
                {
                    const double reserve=std::string_view(resource)=="food"?2.*double(city.population()):0.;
                    const int load=int(std::clamp(std::floor(supply.amount(resource)-reserve),0.,double(needed)));
                    if(load>0 && supply.addAmount(resource,-load)) { unit.rations_+=load; needed-=load; }
                }
                continue;
            }
            for (const auto& w : local->employment().workplaces())
            {
                if (!w.operational || w.objectTypeId != SettlementObjectTypes::Barracks) continue;
                const auto source = local->logistics.forObject(w.objectId);
                const int amount = std::min(needed, local->logistics.available(source, "rations"));
                if (amount > 0 && local->logistics.consumeAvailable(source, "rations", amount))
                { unit.rations_ += amount; needed -= amount; }
            }
        }
        (void)minute;
    }
    std::size_t MilitarySystem::applyBattleCasualties(World& world, ArmyId id, std::size_t amount)
    {
        auto* unit=world.army(id);
        if (!unit) return 0;
        amount=std::min(amount,unit->soldiers_.size());
        const double minute=double(world.time().totalGameMinutes());
        std::size_t removed=0;
        // Soldier IDs are independent of vector addresses. Reacquire each person
        // after erasure; ancestry and surviving relatives retain the original ID.
        while (removed<amount && !unit->soldiers_.empty())
        {
            const auto sid=unit->soldiers_.back();
            const auto* soldier=world.soldier(sid);
            auto* city=soldier?world.settlement(soldier->homeSettlementId()):nullptr;
            auto* c=soldier?person(world,*soldier):nullptr;
            if (city && c)
            {
                auto& state=city->simulationState();
                auto& citizens=state.citizens_;
                const auto personId=c->id;
                const bool resident=!c->militaryDeployed;
                c->health=0;
                if (auto* local=map(world,city->id()))
                    local->activities.retireCitizen(*local,citizens,*c,minute);
                else
                {
                    citizens.rememberAncestry(*c);
                    for (auto& survivor:citizens.citizens_) survivor.familiarities.erase(personId);
                }
                std::erase_if(citizens.citizens_,[&](const auto& person){return person.id==personId;});
                ++citizens.familyVersion_; ++citizens.version_;
                if (state.localMap()) state.synchronizeCitizenPopulation();
                else if (resident) static_cast<void>(state.population_.transferResidents(-1));
            }
            unit->soldiers_.pop_back();
            world.soldiers_.erase(sid);
            ++removed;
        }
        return removed;
    }
    void MilitarySystem::tick(World& world, double minute, double elapsed)
    {
        if (!std::isfinite(elapsed) || elapsed <= 0) return;
        synchronize(world, minute);
        BattleSystem::updatePursuit(world);
        BattleSystem::detectContacts(world);
        // Indexed once per tick, never a citizen-array scan per marching soldier.
        PersonnelIndex people;
        for (auto& city : world.settlements())
        {
            auto* local = map(world, city.id());
            std::unordered_map<SettlementObjectId, int, StrongIdHash> barracksStaff;
            for (auto& c : city.simulationState().citizens_.citizens_)
                if (c.soldierId)
                {
                    people.emplace(c.soldierId, &c);
                    const auto* s = world.soldier(c.soldierId);
                    if (s && !c.militaryDeployed && c.workplaceId) ++barracksStaff[s->barracksId()];
                }
            // Only soldiers actually employed here procure for this barracks.
            // Field units draw stocked supplies when physically at a friendly city.
            for (const auto& [object, count] : barracksStaff)
                if (local) produceIndustry(*local, object, count, minute, elapsed);
        }
        for (auto& unit : world.armies())
        {
            if (unit.soldiers_.empty()) continue;
            double remaining = elapsed;
            while (remaining > 1e-9)
            {
                const double dt = std::min(remaining, 15.0);
                if (unit.moving())
                {
                    double movementMinutes = dt;
                    while (unit.moving() && movementMinutes > 1e-9)
                    {
                        const auto next = unit.route_[unit.routeIndex_];
                        // Validate before interpolating, not after reaching a newly
                        // blocked tile. Planning and movement share the same rules.
                        if (!worldLandStepAllowed(world.grid(), unit.position_, next))
                        { unit.route_.clear(); unit.routeIndex_ = 0; unit.stepMinutes_ = 0; break; }
                        const double duration = unit.currentStepMinutes();
                        const double used = std::min(movementMinutes, std::max(0.0, duration - unit.stepMinutes_));
                        unit.stepMinutes_ += used;
                        movementMinutes -= used;
                        if (unit.stepMinutes_ + 1e-9 < duration) break;
                        unit.position_ = next;
                        ++unit.routeIndex_;
                        unit.stepMinutes_ = 0;
                        BattleSystem::detectContacts(world);
                    }
                    if (!unit.moving()) unit.stepMinutes_ = 0;
                }
                unit.station_ = stationAt(world, unit);
                if (unit.moving()) setDeployed(world, unit, true, minute + elapsed - remaining, &people);
                resupply(world, unit, minute + elapsed - remaining);
                {
                    // A realm field force does not depend on its recruitment
                    // city's existence, ownership, payroll or needs policy.
                    const CitizenSimulationPolicy policy;
                    for (auto id : unit.soldiers_)
                    {
                        const auto it = people.find(id);
                        if (it == people.end() || it->second->health <= 0 || !it->second->militaryDeployed) continue;
                        auto& c = *it->second;
                        const auto* soldier = world.soldier(id);
                        auto* source = soldier ? map(world, soldier->homeSettlementId()) : nullptr;
                        auto* realm = world.realm(unit.ownerRealmId());
                        if (source && realm && realm->treasury)
                            source->commerce.payFieldSoldier(c.id, *realm->treasury, dt);
                        c.hunger = std::min(100.0, c.hunger + policy.hungerPerDay * dt / 1440);
                        if (c.hunger >= policy.foodSeekThreshold && unit.rations_ > 0)
                        {
                            --unit.rations_;
                            c.modifyAttributes({{AttributeEffect::Meals, -policy.mealRestoration}});
                            // Field meals consume the unit's real pack only;
                            // they are not consumption by a remote city.
                        }
                        // A hungry army is not an infinite-food loophole. This
                        // replaces (never supplements) its absent city needs.
                        if (c.hunger > policy.starvationThreshold)
                            c.modifyAttributes({{AttributeEffect::Starvation,
                                -(c.hunger - policy.starvationThreshold) * dt / 1440}});
                        else c.health = std::min(100.0, c.health + policy.healthRecoveryPerDay * dt / 1440);
                    }
                }
                remaining -= dt;
            }
        }
        synchronize(world, minute + elapsed);
        BattleSystem::detectContacts(world);
    }
}

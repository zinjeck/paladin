#include "simulation/MilitarySystem.h"
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
        case MilitaryResult::Success: return "Order accepted.";
        case MilitaryResult::NotOwned: return "This is not your realm's unit or city.";
        case MilitaryResult::NoBarracksEmployee: return "Hire an unassigned adult at a completed barracks first.";
        case MilitaryResult::ReturnHome: return "Return to the home city before transferring or dismissing soldiers.";
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
    bool MilitarySystem::canOrganize(const World& world, const Army& unit) noexcept
    {
        const auto* city = world.settlement(unit.homeSettlementId());
        return city && city->ownerRealmId() == unit.ownerRealmId() &&
               presentAt(world, unit, city->id());
    }
    std::size_t MilitarySystem::available(const World& world, SettlementId city) noexcept
    {
        return std::count_if(world.soldiers().begin(), world.soldiers().end(), [&](const auto& s)
        { return s.homeSettlementId() == city && !s.unitId(); });
    }
    void MilitarySystem::synchronize(World& world, double minute)
    {
        std::unordered_set<SoldierId, StrongIdHash> retained;
        for (auto& city : world.settlements())
        {
            auto* local = map(world, city.id());
            if (!local) continue;
            auto& people = city.simulationState().citizens_;
            local->employment().synchronize(local->objectState(), people);
            for (auto& c : people.citizens_)
            {
                const auto* w = local->employment().workplace(c.workplaceId);
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
                const bool enlisted = !c.child && c.health > 0 && w && w->operational &&
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
                return !s || s->unitId() != unit.id() || s->homeSettlementId() != unit.home_;
            });
            if (unit.soldiers_.empty())
            {
                unit.route_.clear(); unit.routeIndex_ = 0; unit.stepMinutes_ = 0;
                // Personnel losses do not conjure replacement recruits. The
                // empty record can be reorganized at home, not remotely.
            }
        }
    }
    ArmyId MilitarySystem::createUnit(World& world, RealmId actor, SettlementId home)
    {
        auto* city = world.settlement(home);
        if (!actor || !city || city->ownerRealmId() != actor || !map(world, home)) return {};
        if (std::count_if(world.armies().begin(), world.armies().end(), [&](const auto& u)
            { return u.ownerRealmId() == actor; }) >= MaximumUnitsPerRealm) return {};
        synchronize(world, double(world.time().totalGameMinutes()));
        const auto id = world.createArmy(city->position());
        auto* unit = world.army(id);
        unit->home_ = home;
        unit->name_ = "Unit " + std::to_string(id.value());
        world.assignArmyToRealm(id, actor);
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
            c->militaryDeployed = deployed;
            c->insideHome = false;
            c->hasVisualSnapshot = false;
            c->activity = CitizenActivity::Idle;
        }
    }
    void MilitarySystem::returnSurplus(World& world, Army& unit, double minute)
    {
        const int capacity = int(unit.soldierCount()) * RationsPerSoldier;
        int excess = std::max(0, unit.rations_ - capacity);
        auto* local = map(world, unit.home_);
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
    MilitaryResult MilitarySystem::resizeUnit(World& world, RealmId actor, ArmyId id, int delta)
    {
        synchronize(world, double(world.time().totalGameMinutes()));
        auto* unit = world.army(id);
        if (!unit) return MilitaryResult::InvalidUnit;
        if (!actor || unit->ownerRealmId() != actor) return MilitaryResult::NotOwned;
        if (!canOrganize(world, *unit)) return MilitaryResult::ReturnHome;
        if (delta == 0) return MilitaryResult::Success;
        if (delta > 0)
        {
            int remaining = delta;
            for (auto& s : world.soldiers_.entities())
            {
                if (s.home_ != unit->home_ || s.unit_) continue;
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
            for (std::size_t n = 0; n < count; ++n)
            {
                auto* s = world.soldiers_.find(unit->soldiers_.back());
                if (s)
                {
                    s->unit_ = {};
                    if (auto* c = person(world, *s)) c->militaryUnitId = {};
                }
                unit->soldiers_.pop_back();
            }
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
        // An empty, exhausted record has no people or goods to teleport home.
        if (existing->soldierCount() == 0 && existing->rations() == 0)
        {
            world.armies_.erase(id);
            return MilitaryResult::Success;
        }
        const auto result = resizeUnit(world, actor, id, std::numeric_limits<int>::min());
        if (result != MilitaryResult::Success) return result;
        auto* unit = world.army(id);
        // A city without a keep may have nowhere to return its physical pack.
        // Keep that empty record instead of deleting goods.
        if (unit->rations_ > 0) return MilitaryResult::ReturnHome;
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
        const auto index = [width](WorldTilePosition p) { return std::size_t(p.y) * width + p.x; };
        const auto startIndex = index(start), targetIndex = index(target);
        if (!grid.isValidPosition(start)) return MilitaryResult::InvalidDestination;
        std::vector<int> parents(grid.tileCount(), -1);
        std::queue<WorldTilePosition> open;
        open.push(start); parents[startIndex] = int(startIndex);
        std::size_t expanded = 0;
        constexpr std::size_t MaximumExpanded = 262144;
        while (!open.empty() && parents[targetIndex] < 0 && expanded++ < MaximumExpanded)
        {
            const auto p = open.front(); open.pop();
            for (const auto d : {WorldTilePosition{1,0}, {-1,0}, {0,1}, {0,-1}})
            {
                const WorldTilePosition next{(p.x + d.x + width) % width, p.y + d.y};
                const auto* t = grid.tile(next);
                if (!t || t->terrain != TerrainType::Land || parents[index(next)] >= 0) continue;
                parents[index(next)] = int(index(p)); open.push(next);
            }
        }
        if (parents[targetIndex] < 0) return MilitaryResult::NoLandRoute;
        std::vector<WorldTilePosition> route;
        for (auto at = targetIndex; at != startIndex; at = std::size_t(parents[at]))
            route.push_back({int(at % width), int(at / width)});
        std::reverse(route.begin(), route.end());
        if (continuingStep) route.insert(route.begin(), start);
        // Buy/load only already produced rations before leaving supply range.
        resupply(world, *unit, double(world.time().totalGameMinutes()));
        unit->route_ = std::move(route); unit->routeIndex_ = 0;
        unit->stepMinutes_ = continuedMinutes; unit->wrapWidth_ = width;
        setDeployed(world, *unit, !canOrganize(world, *unit), double(world.time().totalGameMinutes()));
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
            if (!local) continue;
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
    void MilitarySystem::tick(World& world, double minute, double elapsed)
    {
        if (!std::isfinite(elapsed) || elapsed <= 0) return;
        synchronize(world, minute);
        // Indexed once per tick, never a citizen-array scan per marching soldier.
        PersonnelIndex people;
        for (auto& city : world.settlements())
        {
            auto* local = map(world, city.id());
            if (!local) continue;
            std::unordered_map<SettlementObjectId, int, StrongIdHash> barracksStaff;
            for (auto& c : city.simulationState().citizens_.citizens_)
                if (c.soldierId)
                {
                    people.emplace(c.soldierId, &c);
                    const auto* s = world.soldier(c.soldierId);
                    if (s) ++barracksStaff[s->barracksId()];
                }
            // Procurement remains available while a barracks' actual paid
            // soldiers are deployed. It does not produce rations or new people.
            for (const auto& [object, count] : barracksStaff)
                produceIndustry(*local, object, count, minute, elapsed);
        }
        for (auto& unit : world.armies())
        {
            auto* home = map(world, unit.home_);
            if (!home || unit.soldiers_.empty()) continue;
            double remaining = elapsed;
            while (remaining > 1e-9)
            {
                const double dt = std::min(remaining, 15.0);
                if (unit.moving())
                {
                    unit.stepMinutes_ += dt;
                    while (unit.moving() && unit.stepMinutes_ >= unit.minutesPerTile_)
                    {
                        const auto next = unit.route_[unit.routeIndex_];
                        const auto* tile = world.grid().tile(next);
                        if (!tile || tile->terrain != TerrainType::Land)
                        { unit.route_.clear(); unit.routeIndex_ = 0; unit.stepMinutes_ = 0; break; }
                        unit.position_ = next; ++unit.routeIndex_;
                        unit.stepMinutes_ -= unit.minutesPerTile_;
                    }
                    if (!unit.moving()) unit.stepMinutes_ = 0;
                }
                const bool deployed = !canOrganize(world, unit);
                setDeployed(world, unit, deployed, minute + elapsed - remaining, &people);
                resupply(world, unit, minute + elapsed - remaining);
                if (deployed)
                {
                    const auto& policy = home->activities.policy;
                    for (auto id : unit.soldiers_)
                    {
                        const auto it = people.find(id);
                        if (it == people.end() || it->second->health <= 0) continue;
                        auto& c = *it->second;
                        c.hunger = std::min(100.0, c.hunger + policy.hungerPerDay * dt / 1440);
                        if (c.hunger >= policy.foodSeekThreshold && unit.rations_ > 0)
                        {
                            --unit.rations_;
                            c.modifyAttributes({{AttributeEffect::Meals, -policy.mealRestoration}});
                            home->commerce.recordNonMealConsumption("rations", 1);
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
    }
}

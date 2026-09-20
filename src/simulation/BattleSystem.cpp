#include "simulation/BattleSystem.h"
#include "simulation/WorldLandNavigation.h"
#include "simulation/DiplomacySystem.h"
#include "simulation/RealmRulerSystem.h"
#include "world/World.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace Paladin
{
    void BattleSystem::stop(Army& unit)
    {
        unit.route_.clear(); unit.routeIndex_ = 0; unit.stepMinutes_ = 0;
    }
    MilitaryResult BattleSystem::orderAttack(World& world, RealmId actor, ArmyId id, ArmyId target)
    {
        MilitarySystem::synchronize(world, double(world.time().totalGameMinutes()));
        auto* unit=world.army(id); const auto* enemy=world.army(target);
        if (!unit || !enemy || enemy->garrisoned() || id == target)
        {
            return MilitaryResult::InvalidUnit;
        }
        if (!actor || unit->ownerRealmId()!=actor || enemy->ownerRealmId()==actor || !enemy->ownerRealmId()) return MilitaryResult::NotOwned;
        if (!unit->soldierCount() || !enemy->soldierCount()) return MilitaryResult::EmptyUnit;
        if (unit->engagedOpponent_ || enemy->engagedOpponent_) return MilitaryResult::InBattle;
        const auto result=MilitarySystem::orderMove(world,actor,id,enemy->position());
        if (result!=MilitaryResult::Success) return result;
        // Explicitly attacking a physically reachable foreign army is a hostile
        // act, even when the two realm capitals are outside diplomatic range.
        DiplomacySystem::hostileContact(world,actor,enemy->ownerRealmId());
        unit=world.army(id); unit->attackTarget_=target;
        detectContacts(world);
        return MilitaryResult::Success;
    }
    void BattleSystem::updatePursuit(World& world)
    {
        for (auto& unit:world.armies())
        {
            if (unit.engagedOpponent_)
            {
                const auto* enemy=world.army(unit.engagedOpponent_);
                if (!enemy || !enemy->soldierCount() || !unit.soldierCount() || enemy->engagedOpponent_!=unit.id())
                    release(world,unit.id(),unit.engagedOpponent_);
                continue;
            }
            if (!unit.attackTarget_) continue;
            const auto* target=world.army(unit.attackTarget_);
            const auto* relation=target?world.diplomacy().between(unit.ownerRealmId(),target->ownerRealmId()):nullptr;
            if (!target || target->garrisoned() || !target->soldierCount() ||
                !unit.soldierCount() ||
                target->ownerRealmId() == unit.ownerRealmId() || !relation ||
                !relation->atWar)
            { stop(unit); unit.attackTarget_={}; continue; }
            if (target->engagedOpponent_ && target->engagedOpponent_!=unit.id())
            { stop(unit); unit.attackTarget_={}; continue; }
            if (unit.destination()==target->position()) continue;
            // Do not throw away partial edge progress or re-run a global search
            // on every tick when the destination has not actually changed.
            const bool inStep=unit.moving() && unit.stepMinutes_>0;
            const auto start=inStep?unit.route_[unit.routeIndex_]:unit.position();
            auto path=worldLandRoute(world.grid(),start,target->position(),WorldLandMovement::EightWay);
            if (!path) { stop(unit); unit.attackTarget_={}; continue; }
            unit.route_.assign(path->begin()+1,path->end());
            if (inStep) unit.route_.insert(unit.route_.begin(),start);
            unit.routeIndex_=0;
        }
    }
    void BattleSystem::detectContacts(World& world)
    {
        for (auto& unit:world.armies())
        {
            if (!unit.attackTarget_ || unit.engagedOpponent_ || !unit.soldierCount()) continue;
            auto* enemy=world.army(unit.attackTarget_);
            if (!enemy || enemy->garrisoned() || enemy->engagedOpponent_ ||
                !enemy->soldierCount() ||
                enemy->ownerRealmId() == unit.ownerRealmId() ||
                enemy->position() != unit.position())
            {
                continue;
            }
            const auto* tile=world.grid().tile(unit.position());
            if (!tile || tile->terrain!=TerrainType::Land) continue;
            // Contact is at a reached world tile, never an interpolated screen
            // overlap. Freeze both routes before they can step through each other.
            stop(unit); stop(*enemy);
            unit.engagedOpponent_=enemy->id(); enemy->engagedOpponent_=unit.id();
        }
    }
    bool BattleSystem::valid(const World& world,const BattleEncounter& battle) noexcept
    {
        const auto* a=world.army(battle.player); const auto* b=world.army(battle.enemy);
        return a && b && a->soldierCount() && b->soldierCount() && a->ownerRealmId()!=b->ownerRealmId() &&
            a->engagedOpponent_==b->id() && b->engagedOpponent_==a->id() &&
            a->position()==battle.tile && b->position()==battle.tile;
    }
    std::optional<BattleEncounter> BattleSystem::pendingFor(const World& world,RealmId actor)
    {
        if (!actor) return {};
        for (const auto& unit:world.armies()) if (unit.ownerRealmId()==actor && unit.engagedOpponent_)
        {
            BattleEncounter battle{unit.id(),unit.engagedOpponent_,unit.position()};
            if (valid(world,battle)) return battle;
        }
        return {};
    }
    void BattleSystem::release(World& world,ArmyId first,ArmyId second)
    {
        for (auto id:{first,second}) if (auto* unit=world.army(id))
        {
            const auto other=id==first?second:first;
            if (unit->engagedOpponent_==other) unit->engagedOpponent_={};
            if (unit->attackTarget_==other) unit->attackTarget_={};
            if (!unit->engagedOpponent_) stop(*unit);
        }
    }
    bool BattleSystem::retreat(World& world,RealmId actor,const BattleEncounter& battle)
    {
        auto* unit=world.army(battle.player);
        if (!unit || unit->ownerRealmId()!=actor || !valid(world,battle)) return false;
        release(world,battle.player,battle.enemy);
        // Move the retreating army one reachable tile away if possible. When
        // surrounded, cancel the encounter in place without teleporting to sea.
        constexpr WorldTilePosition offsets[]{{0,1},{1,1},{-1,1},{1,0},{-1,0},{0,-1},{1,-1},{-1,-1}};
        for (const auto d:offsets)
        {
            const WorldTilePosition next{(battle.tile.x+d.x+world.grid().width())%world.grid().width(),battle.tile.y+d.y};
            if (!worldLandStepAllowed(world.grid(),battle.tile,next)) continue;
            if (MilitarySystem::orderMove(world,actor,battle.player,next)==MilitaryResult::Success) break;
        }
        return true;
    }
    double BattleSystem::strength(const World& world,const Army& army)
    {
        double value=0;
        for (auto id:army.soldiers())
        {
            const auto* soldier=world.soldier(id);
            const auto* home=soldier?world.settlement(soldier->homeSettlementId()):nullptr;
            const auto* person=home?home->simulationState().citizens().citizen(soldier->sourceCitizenId()):nullptr;
            if (!person || person->health<=0) continue;
            value+=std::clamp(person->health/100.,0.,1.)*(.5+.5*std::clamp(person->energy/100.,0.,1.))*(1.-.4*std::clamp(person->hunger/100.,0.,1.));
        }
        return value;
    }
    BattleResult BattleSystem::simulate(World& world,RealmId actor,const BattleEncounter& battle)
    {
        const auto* a=world.army(battle.player); const auto* b=world.army(battle.enemy);
        if (!valid(world,battle) || !a || a->ownerRealmId()!=actor) return {};
        const double first=strength(world,*a),second=strength(world,*b);
        const bool firstWins=first>second || (first==second && a->id().value()<b->id().value());
        const ArmyId winner=firstWins?a->id():b->id(), loser=firstWins?b->id():a->id();
        const auto winCount=firstWins?a->soldierCount():b->soldierCount(),loseCount=firstWins?b->soldierCount():a->soldierCount();
        const double ratio=std::min(first,second)/std::max(.000001,std::max(first,second));
        // Explicit first-pass autoresolve: the stronger force wins, the losing
        // roster is killed, and the winner loses up to half its force. No random
        // rerolls, abstract manpower, duplicated citizens, or free replacement troops.
        const std::size_t loss=std::min(winCount-1,std::size_t(std::ceil(double(winCount)*ratio*.5)));
        release(world,battle.player,battle.enemy);
        const auto winnerLoss=MilitarySystem::applyBattleCasualties(world,winner,loss);
        const auto loserLoss=MilitarySystem::applyBattleCasualties(world,loser,loseCount);
        // Physical supplies survive the defeated unit as captured rations.
        auto* win=world.army(winner); auto* lost=world.army(loser);
        if (win && lost && lost->soldierCount()==0)
        {
            const int transfer=std::min(lost->rations_,std::numeric_limits<int>::max()-win->rations_);
            win->rations_+=transfer; lost->rations_-=transfer;
            if (lost->rations_==0) world.armies_.erase(loser);
        }
        RealmRulerSystem::updatePlayer(world,actor);
        return {true,winner,firstWins?winnerLoss:loserLoss,firstWins?loserLoss:winnerLoss};
    }
}

#include "simulation/CitizenshipSystem.h"
#include "world/World.h"
#include "world/WorldGeography.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include <algorithm>
#include <numbers>

namespace Paladin
{
    namespace
    {
        void naturalize(SettlementCitizen& person, RealmId realm, CultureId culture)
        {
            if (!realm || !culture || person.health <= 0) return;
            if (person.primaryCultureId != culture)
            {
                // Preserve the previous primary, never duplicate the new one.
                person.secondaryCultureId=person.primaryCultureId;
                person.primaryCultureId=culture;
            }
            if (person.secondaryCultureId==culture) person.secondaryCultureId={};
            person.citizenshipRealmId=realm;
        }
    }
    void SettlementCitizenState::configureCommunity(SettlementId settlement, RealmId realm,
                                                    CultureId culture, const RealmLaws& laws,
                                                    bool citizenshipResearched)
    {
        const bool changed=community_!=settlement || communityRealm_!=realm ||
            dominantCulture_!=culture || laws_!=laws || naturalization_!=citizenshipResearched;
        community_=settlement; communityRealm_=realm; dominantCulture_=culture;
        laws_=laws; naturalization_=citizenshipResearched;
        if (!changed) return;
        for (auto& c : citizens_)
        {
            if (!c.primaryCultureId)
            { c.primaryCultureId=culture; c.birthSettlementId=settlement; }
            if (citizenshipResearched && !c.militaryDeployed) naturalize(c,realm,culture);
        }
        ++version_;
    }
    void CitizenshipSystem::synchronize(World& world)
    {
        for (auto& city : world.settlements())
            if (const auto* realm=world.realm(city.ownerRealmId()))
                city.simulationState().citizens().configureCommunity(city.id(),realm->id(),
                    city.primaryCultureId(),realm->laws,realm->citizenshipResearched);
    }
    bool CitizenshipSystem::research(World& world, RealmId actor)
    {
        auto* realm=world.realm(actor);
        if (!realm || !realm->capitalSettlementId() || realm->citizenshipResearched) return false;
        realm->citizenshipResearched=true;
        synchronize(world);
        // The technology enfranchises the realm's current personnel too,
        // without moving field soldiers back into city population.
        for (auto& city : world.settlements())
            if (city.ownerRealmId()==actor)
                for (auto& c : city.simulationState().citizens().citizens_)
                    naturalize(c,actor,city.primaryCultureId());
        return true;
    }
    std::vector<ImmigrantOrigin> CitizenshipSystem::nearbyOrigins(const World& world, SettlementId id)
    {
        const auto* target=world.settlement(id);
        if (!target) return {};
        struct Candidate { double distance; ImmigrantOrigin origin; };
        std::vector<Candidate> candidates;
        for (const auto& city : world.settlements())
        {
            const auto* realm=world.realm(city.ownerRealmId());
            if (!realm || !realm->aiControlled || city.ownerRealmId()==target->ownerRealmId() ||
                !city.primaryCultureId() || city.population()==0) continue;
            const double distance=geographicDistance(target->position(),city.position(),
                world.grid().width(),world.grid().height());
            if (distance<=std::numbers::pi/6)
                candidates.push_back({distance,{city.id(),city.primaryCultureId()}});
        }
        std::sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b)
        { return a.distance!=b.distance ? a.distance<b.distance : a.origin.settlement.value()<b.origin.settlement.value(); });
        std::vector<ImmigrantOrigin> result;
        for (const auto& c : candidates)
        { result.push_back(c.origin); if (result.size()==8) break; }
        return result;
    }
    void CitizenshipSystem::assignImmigrants(World& world, SettlementId id, std::size_t first,
                                              const std::vector<ImmigrantOrigin>& origins)
    {
        auto* city=world.settlement(id);
        if (!city || origins.empty()) return;
        const auto* realm=world.realm(city->ownerRealmId());
        auto& people=city->simulationState().citizens();
        for (std::size_t i=first;i<people.citizens_.size();++i)
        {
            auto& c=people.citizens_[i];
            const auto& origin=origins[(c.id.value()-1)%origins.size()];
            c.primaryCultureId=origin.culture; c.secondaryCultureId={};
            c.birthSettlementId=origin.settlement; c.citizenshipRealmId={};
            if (realm && realm->citizenshipResearched)
                naturalize(c,realm->id(),city->primaryCultureId());
        }
        ++people.version_;
    }
    void CitizenshipSystem::inherit(SettlementCitizen& baby,const SettlementCitizen& mother,
                                    const SettlementCitizen& father,CultureId dominant,
                                    RealmId realm,SettlementId bornIn,CitizenshipRights rights)
    {
        baby.birthSettlementId=bornIn;
        baby.primaryCultureId=mother.primaryCultureId;
        baby.secondaryCultureId={};
        if (dominant && mother.hasCulture(dominant) && father.hasCulture(dominant))
            baby.primaryCultureId=dominant;
        else if (father.primaryCultureId && father.primaryCultureId!=baby.primaryCultureId)
            baby.secondaryCultureId=father.primaryCultureId;
        else if (mother.secondaryCultureId && mother.secondaryCultureId!=baby.primaryCultureId)
            baby.secondaryCultureId=mother.secondaryCultureId;
        baby.citizenshipRealmId={};
        const bool qualifies=rights==CitizenshipRights::FullCitizenship ||
            (rights==CitizenshipRights::Blood && (mother.citizenshipRealmId==realm || father.citizenshipRealmId==realm)) ||
            (rights==CitizenshipRights::AcceptedCulture && baby.hasCulture(dominant));
        if (realm && bornIn && qualifies) baby.citizenshipRealmId=realm;
    }
}

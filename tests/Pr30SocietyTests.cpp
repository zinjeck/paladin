#include "TestFramework.h"
#include "simulation/CitizenshipSystem.h"
#include "world/World.h"
#include "world/WorldGeography.h"
#include <cmath>
#include <iostream>
#include <set>

void runPr30SocietyTests()
{
    using namespace Paladin;
    WorldGenerationSettings settings;
    settings.width=128; settings.height=64; settings.seed=3030;
    World world(settings);
    for (int y=0;y<64;++y) for (int x=0;x<128;++x)
    { auto& tile=*world.grid().tile({x,y}); tile.terrain=TerrainType::Land; tile.biome=BiomeType::Plain; }
    const auto player=world.createRealm(), nearby=world.createRealm(), remote=world.createRealm();
    const auto home=world.foundCapitalSettlement({40,32},player,{"Home","Home Folk","Capital",{},"tribal"});
    const auto origin=world.foundCapitalSettlement({50,32},nearby,{"Near","Near Folk","Near City",{},"tribal"});
    const auto distant=world.foundCapitalSettlement({105,32},remote,{"Far","Far Folk","Far City",{},"tribal"});
    PALADIN_CHECK(home && origin && distant);
    world.realm(nearby)->aiControlled=true; world.realm(remote)->aiControlled=true;
    const auto homeCulture=world.realm(player)->primaryCultureId();
    const auto originCulture=world.realm(nearby)->primaryCultureId();
    auto& people=world.settlement(home)->simulationState().citizens();
    const RealmLaws defaults;
    PALADIN_CHECK(defaults.governance==GovernanceType::AbsoluteMonarchy);
    PALADIN_CHECK(defaults.citizenship==CitizenshipRights::Blood && defaults.gender==GenderRights::MaleDominated);
    for (const auto& c : people.citizens())
    {
        PALADIN_CHECK(c.primaryCultureId==homeCulture && !c.secondaryCultureId && !c.citizenshipRealmId);
        PALADIN_CHECK(people.militaryEligible(c)==(c.sex==CitizenSex::Male));
    }
    auto origins=CitizenshipSystem::nearbyOrigins(world,home);
    PALADIN_CHECK(origins.size()==1 && origins.front().settlement==origin && origins.front().culture==originCulture);
    const auto first=people.citizens().size();
    PALADIN_CHECK(people.spawnImmigrants(2));
    CitizenshipSystem::assignImmigrants(world,home,first,origins);
    PALADIN_CHECK(people.citizens()[first].primaryCultureId==originCulture);
    PALADIN_CHECK(!people.citizens()[first].citizenshipRealmId);
    PALADIN_CHECK(CitizenshipSystem::research(world,player));
    PALADIN_CHECK(!CitizenshipSystem::research(world,player));
    for (const auto& c : people.citizens())
        PALADIN_CHECK(c.citizenshipRealmId==player && c.primaryCultureId==homeCulture);
    PALADIN_CHECK(people.citizens()[first].secondaryCultureId==originCulture);
    PALADIN_CHECK(people.citizens()[first].birthSettlementId==origin);
    PALADIN_CHECK(!people.citizens()[0].secondaryCultureId);
    const auto subsequent=people.citizens().size();
    PALADIN_CHECK(people.spawnImmigrants(1));
    CitizenshipSystem::assignImmigrants(world,home,subsequent,origins);
    PALADIN_CHECK(people.citizens().back().primaryCultureId==homeCulture &&
                  people.citizens().back().secondaryCultureId==originCulture &&
                  people.citizens().back().citizenshipRealmId==player);
    SettlementCitizen child, mother=people.citizens()[first],father=people.citizens()[first+1];
    CitizenshipSystem::inherit(child,mother,father,homeCulture,player,home,CitizenshipRights::Blood);
    PALADIN_CHECK(child.primaryCultureId==homeCulture && !child.secondaryCultureId && child.citizenshipRealmId==player);
    mother.citizenshipRealmId={}; father.citizenshipRealmId={};
    CitizenshipSystem::inherit(child,mother,father,homeCulture,player,home,CitizenshipRights::Blood);
    PALADIN_CHECK(!child.citizenshipRealmId);
    // One citizen parent is sufficient, but birth must be in a settlement.
    father.citizenshipRealmId=player;
    CitizenshipSystem::inherit(child,mother,father,homeCulture,player,{},CitizenshipRights::Blood);
    PALADIN_CHECK(!child.citizenshipRealmId);
    RealmLaws changed=defaults; changed.gender=GenderRights::FemaleDominated;
    people.configureCommunity(home,player,homeCulture,changed,true);
    for(const auto& c:people.citizens()) PALADIN_CHECK(people.militaryEligible(c)==(c.sex==CitizenSex::Female));
    changed.gender=GenderRights::Equal;
    people.configureCommunity(home,player,homeCulture,changed,true);
    for(const auto& c:people.citizens()) PALADIN_CHECK(people.militaryEligible(c));
    PALADIN_CHECK(geographicDistance({127,32},{0,32},128,64)<.05);
    PALADIN_CHECK(std::abs(geographicDistance({127,32},{0,32},128,64)-
                          geographicDistance({0,32},{1,32},128,64))<1e-10);
    std::cout<<"[pr30/society] laws, gender eligibility, nearby origins, dual culture, citizenship and inheritance passed\n";
}

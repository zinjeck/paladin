#include "TestFramework.h"
#include "simulation/DiplomacySystem.h"
#include "rendering/WorldThematicPalette.h"
#include "ui/RealmStatistics.h"
#include "world/World.h"
#include <iostream>
#include <limits>
void runDiplomacyTests()
{
    using namespace Paladin;
    WorldGenerationSettings settings; settings.width=256; settings.height=64; settings.seed=891; settings.populateAiRealms=false;
    World world(settings);
    for(int y=0;y<64;++y) for(int x=0;x<settings.width;++x) world.grid().tile({x,y})->terrain=TerrainType::Land;
    world.grid().terrainChanged();
    const auto a=world.createRealm(), b=world.createRealm(), c=world.createRealm();
    const auto city=world.foundCapitalSettlement({16,32},a,{"Amber","A","Amber city",{},"civic"},playerSettlementFoundationProfile(settings.seed));
    const auto tribal=world.foundCapitalSettlement({45,32},b,{"Blue","B","Blue city",{},"tribal"},playerSettlementFoundationProfile(settings.seed+1));
    auto largeProfile=playerSettlementFoundationProfile(settings.seed+2); largeProfile.initialPopulation=200;
    const auto third=world.foundCapitalSettlement({30,44},c,{"Cedar","C","Cedar city",{},"tribal"},largeProfile);
    PALADIN_CHECK(city && tribal && third);
    const auto civicTiles=world.territory().controlledTileCount();
    const auto tribalRevision=world.tribalInfluence().revision();
    world.realm(a)->treasury->balance=2500; world.realm(b)->treasury->balance=400;
    using A=DiplomaticAction; using R=DiplomaticResult;
    PALADIN_CHECK(DiplomacySystem::apply(world,a,a,A::War)==R::Self);
    PALADIN_CHECK(DiplomacySystem::apply(world,a,{},A::War)==R::InvalidRealm);
    PALADIN_CHECK(DiplomacySystem::apply(world,a,b,A::Gift,-1)==R::InvalidGift);
    PALADIN_CHECK(DiplomacySystem::apply(world,a,b,A::Gift,2501)==R::InsufficientGold);
    PALADIN_CHECK(world.realm(a)->treasury->balance==2500 && world.realm(b)->treasury->balance==400);
    const int beforeGift=DiplomacySystem::opinion(world,b,a);
    const int senderView=DiplomacySystem::opinion(world,a,b);
    PALADIN_CHECK(DiplomacySystem::suggestedGift(world,a,b)<=125);
    PALADIN_CHECK(DiplomacySystem::apply(world,a,b,A::Gift,1000)==R::Success);
    PALADIN_CHECK(DiplomacySystem::opinion(world,b,a)>beforeGift);
    PALADIN_CHECK(DiplomacySystem::opinion(world,a,b)==senderView);
    PALADIN_CHECK(world.realm(a)->treasury->balance==1500 && world.realm(b)->treasury->balance==1400);
    world.realm(b)->treasury->balance=std::numeric_limits<Money>::max();
    PALADIN_CHECK(DiplomacySystem::apply(world,a,b,A::Gift,100)==R::TreasuryOverflow);
    PALADIN_CHECK(world.realm(a)->treasury->balance==1500);
    world.realm(b)->treasury->balance=1400;
    PALADIN_CHECK(DiplomacySystem::apply(world,a,b,A::Alliance)==R::Success);
    PALADIN_CHECK(world.diplomacy().between(b,a)->allied);
    PALADIN_CHECK(DiplomacySystem::apply(world,b,a,A::Alliance)==R::Already);
    PALADIN_CHECK(DiplomacySystem::apply(world,a,b,A::Trade)==R::Success);
    PALADIN_CHECK(DiplomacySystem::apply(world,a,b,A::Tribute)==R::Success);
    PALADIN_CHECK(world.diplomacy().overlordOf(b)==a);
    PALADIN_CHECK(DiplomacySystem::apply(world,c,b,A::Tribute)==R::SubjectConflict);
    PALADIN_CHECK(DiplomacySystem::apply(world,b,c,A::Tribute)==R::Success);
    PALADIN_CHECK(DiplomacySystem::apply(world,c,a,A::Tribute)==R::SubjectConflict); // indirect cycle
    PALADIN_CHECK(DiplomacySystem::apply(world,a,b,A::Tribute)==R::Success); // release
    PALADIN_CHECK(!world.diplomacy().overlordOf(b));
    PALADIN_CHECK(DiplomacySystem::apply(world,a,b,A::War)==R::Success);
    PALADIN_CHECK(world.diplomacy().between(b,a)->atWar);
    PALADIN_CHECK(!world.diplomacy().between(a,b)->allied && !world.diplomacy().between(a,b)->trading);
    PALADIN_CHECK(DiplomacySystem::apply(world,a,b,A::Alliance)==R::AtWar);
    PALADIN_CHECK(DiplomacySystem::apply(world,a,b,A::Trade)==R::AtWar);
    PALADIN_CHECK(DiplomacySystem::apply(world,a,b,A::Peace)==R::Success);
    PALADIN_CHECK(!world.diplomacy().between(b,a)->atWar);
    PALADIN_CHECK(world.territory().controlledTileCount()==civicTiles);
    PALADIN_CHECK(world.territory().controlledTileCount(b)==0);
    PALADIN_CHECK(world.tribalInfluence().revision()==tribalRevision);
    PALADIN_CHECK(worldRealmAt(world,16.5,32.5)==a && worldRealmAt(world,45.5,32.5)==b);
    const auto stats=collectRealmStatistics(world);
    PALADIN_CHECK(stats.size()==3);
    for(const auto& r:stats)
        if(r.realm==a) { PALADIN_CHECK(r.population==8 && r.cities==1 && r.fortresses==0 && r.size>0 && r.gold==1500); }
    const auto far=world.createRealm();
    PALADIN_CHECK(world.foundCapitalSettlement({150,32},far,{"Far","F","Far city",{},"tribal"}));
    const auto before=world.realm(a)->treasury->balance;
    for (auto action:{A::Alliance,A::Gift,A::Tribute,A::Trade,A::War,A::Peace})
        PALADIN_CHECK(DiplomacySystem::apply(world,a,far,action,100)==R::OutOfRange);
    PALADIN_CHECK(!world.diplomacy().between(a,far) && world.realm(a)->treasury->balance==before);
    PALADIN_CHECK(DiplomacySystem::inRange(world,a,b));
    PALADIN_CHECK(!DiplomacySystem::inRange(world,a,far));
    world.realm(a)->treasury->balance=100000000;
    PALADIN_CHECK(DiplomacySystem::suggestedGift(world,a,c)>DiplomacySystem::suggestedGift(world,a,b));
    world.realm(a)->treasury->balance=before;
    PALADIN_CHECK(populationMapColor(0)==PopulationColors[0]);
    PALADIN_CHECK(populationMapColor(31)==PopulationColors[1]);
    PALADIN_CHECK(populationMapColor(32)==PopulationColors[2]);
    PALADIN_CHECK(populationMapColor(8192)==PopulationColors.back());
    PALADIN_CHECK(worldArmyVisibility(10)==0 && worldArmyVisibility(16)==1);
    PALADIN_CHECK(Army::MarchMinutesPerTile==5);
    std::cout<<"[diplomacy] symmetric pacts, conserved gifts, invalid-action preflight, acyclic subjects, realm statistics and choropleth classes passed\n";
}

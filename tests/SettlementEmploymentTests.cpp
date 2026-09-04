#include "TestFramework.h"
#include "interaction/SettlementObjectPlacementController.h"
#include "world/settlements/SettlementFoundationProfile.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include <cmath>
using namespace Paladin;
void runSettlementEmploymentTests()
{
    const auto profile = playerSettlementFoundationProfile(555);
    PALADIN_CHECK(profile.initialPopulation == 8);
    PALADIN_CHECK(profile.initialDetailedCitizenCount == 8);
    PALADIN_CHECK(profile.resourceFlowRates.empty());
    SettlementCitizenState citizens;
    PALADIN_CHECK(citizens.initialize(8, 555));
    for (const auto& c : citizens.citizens())
    {
        PALADIN_CHECK(c.ageYears == 20);
        PALADIN_CHECK(!c.workplaceId);
    }
    SettlementCitizenState sample;
    PALADIN_CHECK(sample.initialize(10000, 711));
    std::size_t males = 0;
    for (const auto& c : sample.citizens())
        males += c.sex == CitizenSex::Male;
    PALADIN_CHECK(males > 4700 && males < 5300);

    SettlementGrid grid(40, 40);
    for (int y = 0; y < 40; ++y)
        for (int x = 0; x < 40; ++x)
            grid.tile({x, y})->terrain = TerrainType::Land;
    SettlementMap map(std::move(grid), {0, 0}, 1, 1, 40, 711);
    SettlementObjectPlacementController placement;
    PALADIN_CHECK(placement.beginPlacement(SettlementObjectTypes::House));
    placement.pointerMoved(SettlementTilePosition{5, 5});
    PALADIN_CHECK(!placement.visibleFootprintIsValid(map));
    PALADIN_CHECK(
        placement.pointerPressed({{5, 5}}, map) ==
        SettlementPlacementCommitResult::None
    );
    PALADIN_CHECK(map.objectState().constructionSites().empty());
    PALADIN_CHECK(placement.beginPlacement(SettlementObjectTypes::CityKeep));
    PALADIN_CHECK(
        placement.pointerPressed({{5, 5}}, map) ==
        SettlementPlacementCommitResult::CompletedObject
    );
    PALADIN_CHECK(map.objectState().hasCityKeep());
    PALADIN_CHECK(map.objectState().constructionSites().empty());
    citizens.placeUnpositionedCitizens(map);
    auto fishing = *SettlementObjectCatalog::definition(
        SettlementObjectTypes::FishingGrounds
    );
    PALADIN_CHECK(map.objectState().createConstructionSites(
        map.grid(),
        fishing,
        {{10, 10}, 3, 3}
    ));
    auto stockpile =
        *SettlementObjectCatalog::definition(SettlementObjectTypes::Stockpile);
    stockpile.bypassesConstruction = true;
    PALADIN_CHECK(map.objectState().placeCompletedObject(
        map.grid(),
        stockpile,
        {{20, 20}, 4, 4}
    ));
    auto& jobs = map.employment();
    jobs.synchronize(map.objectState(), citizens);
    PALADIN_CHECK(jobs.workplaces().size() == 2);
    const auto pendingFishId =
        jobs.forConstruction(map.objectState().constructionSites().front().id);
    const auto storeId =
        jobs.forObject(map.objectState().completedObjects().back().id);
    PALADIN_CHECK(pendingFishId && storeId && pendingFishId != storeId);
    PALADIN_CHECK(jobs.workplace(pendingFishId)->capacity == 0);
    PALADIN_CHECK(jobs.workplace(pendingFishId)->maximumCapacity == 4);
    PALADIN_CHECK(jobs.workplace(storeId)->capacity == 0);
    PALADIN_CHECK(jobs.workplace(storeId)->maximumCapacity == 8);
    PALADIN_CHECK(!jobs.workplace(pendingFishId)->operational);
    PALADIN_CHECK(!jobs.adjust(pendingFishId, 1, citizens));
    PALADIN_CHECK(
        !jobs.adjustType(SettlementObjectTypes::FishingGrounds, 1, citizens)
    );
    const auto& delivery =
        map.objectState().constructionSites().front().resourceDeliveries;
    PALADIN_CHECK(delivery.size() == 1);
    PALADIN_CHECK(delivery.front().resourceId == "lumber");
    PALADIN_CHECK(delivery.front().requiredAmount == 4);
    PALADIN_CHECK(
        !map.objectState().canPlace(map.grid(), fishing, {{10, 10}, 3, 3})
    );
    PALADIN_CHECK(
        map.commandState()
            .cancelIntersecting(map, {{11, 11}, 1, 1}, citizens) == 1
    );
    PALADIN_CHECK(map.objectState().constructionSites().empty());
    PALADIN_CHECK(!jobs.workplace(pendingFishId));
    PALADIN_CHECK(
        map.objectState().canPlace(map.grid(), fishing, {{10, 10}, 3, 3})
    );
    PALADIN_CHECK(!map.objectState().blocksMovement({11, 11}));
    PALADIN_CHECK(map.objectState().blocksMovement({20, 20}));
    fishing.bypassesConstruction = true;
    PALADIN_CHECK(map.objectState().placeCompletedObject(
        map.grid(),
        fishing,
        {{10, 10}, 3, 3}
    ));
    jobs.synchronize(map.objectState(), citizens);
    const auto fishId =
        jobs.forObject(map.objectState().completedObjects().back().id);
    PALADIN_CHECK(jobs.workplace(fishId)->capacity == 0);
    PALADIN_CHECK(jobs.workplace(fishId)->maximumCapacity == 4);
    PALADIN_CHECK(jobs.workplace(storeId)->operational);
    jobs.record(360, citizens);
    for (int i = 0; i < 4; ++i)
        PALADIN_CHECK(jobs.adjust(fishId, 1, citizens));
    PALADIN_CHECK(!jobs.adjust(fishId, 1, citizens));
    PALADIN_CHECK(jobs.employed(fishId, citizens) == 4);
    PALADIN_CHECK(jobs.workplace(fishId)->capacity == 4);
    PALADIN_CHECK(jobs.unemployed(citizens) == 4);
    PALADIN_CHECK(jobs.rename(fishId, "  Lakehaven  "));
    PALADIN_CHECK(
        jobs.workplace(citizens.citizens().front().workplaceId)->name ==
        "Lakehaven"
    );
    PALADIN_CHECK(!jobs.rename(fishId, "   "));
    for (int i = 0; i < 4; ++i)
        PALADIN_CHECK(
            jobs.adjustType(SettlementObjectTypes::Stockpile, 1, citizens)
        );
    PALADIN_CHECK(jobs.unemployed(citizens) == 0);
    PALADIN_CHECK(!jobs.adjust(storeId, 1, citizens));
    PALADIN_CHECK(
        jobs.employed(fishId, citizens) + jobs.employed(storeId, citizens) ==
        citizens.citizens().size()
    );
    PALADIN_CHECK(jobs.adjust(fishId, -1, citizens));
    PALADIN_CHECK(jobs.unemployed(citizens) == 1);
    PALADIN_CHECK(jobs.workplace(fishId)->capacity == 3);
    PALADIN_CHECK(!citizens.citizens().front().workplaceId);
    jobs.record(1440, citizens);
    PALADIN_CHECK(
        std::abs(jobs.history().back().unemployedPercent - 12.5) < 1e-9
    );
    jobs.record(40 * 1440, citizens);
    PALADIN_CHECK(jobs.history().size() <= 2);
    jobs.tickAttendance(map, citizens, 9 * 60);
    bool arrived = false;
    for (int tick = 0; tick < 1500; ++tick)
    {
        const double minute = 9 * 60 + tick * .1;
        jobs.tickAttendance(map, citizens, minute);
        citizens.tickMovement(map, .1);
        for (const auto& c : citizens.citizens())
            arrived = arrived || (c.workplaceId == storeId &&
                                  c.activity == CitizenActivity::AtWork);
    }
    PALADIN_CHECK(arrived);
    jobs.tickAttendance(map, citizens, 18 * 60);
    for (const auto& c : citizens.citizens())
        PALADIN_CHECK(c.activity != CitizenActivity::AtWork);
    // Larger footprints raise the ceiling without opening staffing slots.
    PALADIN_CHECK(map.objectState().placeCompletedObject(
        map.grid(),
        fishing,
        {{25, 5}, 6, 6}
    ));
    jobs.synchronize(map.objectState(), citizens);
    const auto largeId =
        jobs.forObject(map.objectState().completedObjects().back().id);
    PALADIN_CHECK(jobs.workplace(largeId)->capacity == 0);
    PALADIN_CHECK(jobs.workplace(largeId)->maximumCapacity == 16);
    PALADIN_CHECK(jobs.workplace(fishId)->maximumCapacity == 4);
    const auto* road =
        SettlementObjectCatalog::definition(SettlementObjectTypes::Road);
    PALADIN_CHECK(map.objectState().createConstructionSites(
        map.grid(),
        *road,
        {{5, 30}, 5, 2}
    ));
    PALADIN_CHECK(
        map.commandState().cancelIntersecting(map, {{7, 30}, 1, 1}, citizens) ==
        1
    );
    PALADIN_CHECK(!map.objectState().constructionSiteAt({7, 30}));
    PALADIN_CHECK(map.objectState().constructionSiteAt({6, 30}));
    PALADIN_CHECK(map.objectState().constructionSiteAt({8, 30}));
    PALADIN_CHECK(map.objectState().constructionSiteAt({7, 31}));
    PALADIN_CHECK(
        map.objectState().canPlace(map.grid(), *road, {{7, 30}, 1, 1})
    );
    const SettlementObjectState emptyObjects(40, 40);
    jobs.synchronize(emptyObjects, citizens);
    PALADIN_CHECK(jobs.workplaces().empty());
    PALADIN_CHECK(jobs.unemployed(citizens) == 8);
    for (const auto& c : citizens.citizens())
        PALADIN_CHECK(c.ageYears == 20);
}

#pragma once
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include "world/settlements/objects/SettlementObjectState.h"
#include "world/settlements/objects/jobs/WorkplaceDefinition.h"

namespace Paladin
{
    inline constexpr WorkplaceDefinition
        FisheryWorkplace{SettlementObjectTypes::FishingGrounds, 4, 4, 9, 50};
    struct FisheryJobPolicy
    {
        double minutesPerFish = 240;
        int waterTilesPerWorker = 12;
        int baseReach = 4;
        int referenceArea = 4;
        double reachAreaExponent = .25;
    };
    int fisheryReach(
        const SettlementObjectFootprint&,
        const FisheryJobPolicy& = {}
    );
    double fisheryProductionPerMinute(
        std::size_t waterTiles,
        int attendingWorkers,
        const FisheryJobPolicy& policy
    );
    struct FishingSpot
    {
        SettlementTilePosition land;
        SettlementTilePosition water;
    };
    std::vector<FishingSpot> fisheryShoreline(
        const SettlementGrid&,
        const CompletedSettlementObject&
    );
    struct FisheryZonePreview
    {
        std::vector<SettlementTilePosition> availableWater;
        std::vector<SettlementTilePosition> excludedWater;
        SettlementObjectFootprint bounds;
    };
    FisheryZonePreview fisheryZonePreview(
        const SettlementGrid&,
        const SettlementObjectState&,
        const SettlementObjectFootprint&
    );
} // namespace Paladin

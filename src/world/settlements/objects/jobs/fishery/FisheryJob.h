#pragma once
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include "world/settlements/objects/SettlementObjectState.h"
#include "world/settlements/objects/jobs/WorkplaceDefinition.h"
#include <span>
#include <unordered_map>

namespace Paladin
{
    inline constexpr WorkplaceDefinition
        FisheryWorkplace{SettlementObjectTypes::FishingGrounds, 4, 4, 40, 50};
    struct FisheryJobPolicy
    {
        // Nine fish per uninterrupted twelve-hour shift. Travel and needs
        // reduce this further: one fisher supports roughly two to three adults.
        double minutesPerFish = 80;
        int waterTilesPerWorker = 12;
        int baseReach = 8;
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

    class SettlementMap;
    struct FishingBoatPlan
    {
        FishingSpot launch{{-1, -1}, {-1, -1}};
        std::vector<SettlementTilePosition> route;
    };
    class FisheryBoatNavigation
    {
    public:
        FishingBoatPlan plan(
            const SettlementMap&,
            const CompletedSettlementObject&,
            std::uint64_t sequence,
            std::span<const SettlementTilePosition> occupiedTargets
        );
        std::vector<SettlementTilePosition> returnRoute(
            const SettlementMap&,
            SettlementTilePosition from,
            SettlementTilePosition preferredLanding
        ) const;
        std::size_t productiveWater(
            const SettlementMap&,
            const CompletedSettlementObject&
        );
        void synchronize(const SettlementObjectState&);
        static constexpr std::size_t MaximumWaterNodes = 4096;

    private:
        struct Node
        {
            SettlementTilePosition tile;
            std::size_t parent = 0;
            int depth = 0;
        };
        struct Entry
        {
            FishingSpot launch{{-1, -1}, {-1, -1}};
            std::vector<Node> nodes;
            std::size_t productiveWater = 0;
        };
        Entry& prepare(const SettlementMap&, const CompletedSettlementObject&);
        std::unordered_map<SettlementObjectId, Entry, StrongIdHash> entries_;
        std::uint64_t objectVersion_ = UINT64_MAX;
    };
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

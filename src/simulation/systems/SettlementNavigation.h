#pragma once
#include "core/StrongId.h"
#include "debug/TimingSamples.h"
#include "world/SettlementTilePosition.h"
#include <cstddef>
#include <cstdint>
#include <vector>
#include <deque>
namespace Paladin
{
    class SettlementMap;
    struct CitizenMovementPolicy
    {
        double tilesPerGameMinute = .75;
        double roadSpeedMultiplier = 2;
        double diagonalCost = 1.4142135623730951;
        std::size_t maximumExpandedNodes = 2048;
        std::size_t pathRequestsPerTick = 2;
        bool avoidBuildingFootprints = false;
        ConstructionSiteId escapeConstructionSite;
    };
    class SettlementNavigation
    {
    public:
        mutable TimingSamples timing;
        mutable std::uint64_t requests = 0, failures = 0;
        mutable std::size_t expandedNodes = 0, candidates = 0;
        mutable double lastCost = 0;
        void synchronize(const SettlementMap&);
        bool walkable(
            const SettlementMap&,
            SettlementTilePosition,
            bool avoidBuildingFootprints = false,
            ConstructionSiteId escapeConstructionSite = {}
        ) const;
        bool canStep(
            const SettlementMap&,
            SettlementTilePosition,
            SettlementTilePosition,
            bool avoidBuildingFootprints = false,
            ConstructionSiteId escapeConstructionSite = {}
        ) const;
        double stepCost(
            const SettlementMap&,
            SettlementTilePosition,
            SettlementTilePosition,
            const CitizenMovementPolicy&
        ) const;
        std::vector<SettlementTilePosition> findPath(
            const SettlementMap&,
            SettlementTilePosition,
            SettlementTilePosition,
            const CitizenMovementPolicy&
        ) const;

    private:
        // Value identity, never a retained map pointer.
        std::uint64_t sourceInstance_ = 0;
        std::uint64_t version_ = ~std::uint64_t(0);
        std::vector<std::uint8_t> roads_;
        bool hasRoads_ = false;
        struct SearchRecord { double cost=0; std::size_t parent=0; std::uint64_t generation=0; };
        // Per-map scratch survives requests. Stamps avoid clearing the entire
        // city for each A* search and eliminate one heap allocation per node.
        mutable std::vector<SearchRecord> searchRecords_;
        mutable std::uint64_t searchGeneration_=0;
        struct CachedRoute
        {
            SettlementTilePosition start,goal;
            CitizenMovementPolicy policy;
            std::vector<SettlementTilePosition> path;
            double cost=0;
        };
        mutable std::deque<CachedRoute> routeCache_;
        mutable std::uint64_t routeSource_=0, routeVersion_=~std::uint64_t(0);
    };
} // namespace Paladin

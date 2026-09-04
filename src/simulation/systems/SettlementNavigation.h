#pragma once
#include "world/SettlementTilePosition.h"
#include <vector>
#include <cstddef>
#include <cstdint>
namespace Paladin
{
    class SettlementMap;
    struct CitizenMovementPolicy
    {
        double tilesPerGameMinute = .5;
        double roadSpeedMultiplier = 2;
        double diagonalCost = 1.4142135623730951;
        std::size_t maximumExpandedNodes = 2048;
        std::size_t pathRequestsPerTick = 2;
    };
    class SettlementNavigation
    {
    public:
        void synchronize(const SettlementMap&);
        bool walkable(const SettlementMap&, SettlementTilePosition) const;
        bool canStep(const SettlementMap&, SettlementTilePosition, SettlementTilePosition) const;
        double stepCost(const SettlementMap&, SettlementTilePosition,
            SettlementTilePosition, const CitizenMovementPolicy&) const;
        std::vector<SettlementTilePosition> findPath(const SettlementMap&,
            SettlementTilePosition, SettlementTilePosition,
            const CitizenMovementPolicy&) const;
    private:
        const SettlementMap* source_ = nullptr;
        std::uint64_t version_ = ~std::uint64_t(0);
        std::vector<std::uint8_t> roads_;
    };
}

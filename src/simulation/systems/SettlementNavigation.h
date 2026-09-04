#pragma once
#include "debug/TimingSamples.h"
#include "world/SettlementTilePosition.h"
#include <cstddef>
#include <cstdint>
#include <vector>
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
    mutable TimingSamples timing;
    mutable std::uint64_t requests = 0, failures = 0;
    mutable std::size_t expandedNodes = 0, candidates = 0;
    mutable double lastCost = 0;
    void synchronize(const SettlementMap&);
    bool walkable(const SettlementMap&, SettlementTilePosition) const;
    bool canStep(
        const SettlementMap&,
        SettlementTilePosition,
        SettlementTilePosition
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
};
} // namespace Paladin

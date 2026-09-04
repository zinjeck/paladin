#include "world/settlements/SettlementMap.h"

#include <utility>

namespace Paladin
{
    SettlementMap::SettlementMap(
        WorldGrid grid,
        WorldPosition sourceRegionCenter,
        std::int32_t sourceRegionWidth,
        std::int32_t sourceRegionHeight,
        std::int32_t localTilesPerWorldTile,
        std::uint64_t generationSeed
    ) noexcept
        : grid_(std::move(grid)),
          objectState_(grid_.width(), grid_.height()),
          sourceRegionCenter_(sourceRegionCenter),
          sourceRegionWidth_(sourceRegionWidth),
          sourceRegionHeight_(sourceRegionHeight),
          localTilesPerWorldTile_(localTilesPerWorldTile),
          generationSeed_(generationSeed)
    {
    }


    WorldGrid& SettlementMap::grid() noexcept
    {
        return grid_;
    }


    const WorldGrid& SettlementMap::grid() const noexcept
    {
        return grid_;
    }


    WorldPosition SettlementMap::sourceRegionCenter() const noexcept
    {
        return sourceRegionCenter_;
    }


    std::int32_t SettlementMap::sourceRegionWidth() const noexcept
    {
        return sourceRegionWidth_;
    }


    std::int32_t SettlementMap::sourceRegionHeight() const noexcept
    {
        return sourceRegionHeight_;
    }


    std::int32_t SettlementMap::localTilesPerWorldTile() const noexcept
    {
        return localTilesPerWorldTile_;
    }


    std::uint64_t SettlementMap::generationSeed() const noexcept
    {
        return generationSeed_;
    }


    SettlementObjectState& SettlementMap::objectState() noexcept
    {
        return objectState_;
    }


    const SettlementObjectState&
    SettlementMap::objectState() const noexcept
    {
        return objectState_;
    }
}

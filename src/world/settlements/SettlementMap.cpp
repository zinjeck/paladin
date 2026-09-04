#include "world/settlements/SettlementMap.h"
#include <atomic>
#include <exception>

#include <utility>

namespace Paladin
{
namespace
{
std::uint64_t nextMapInstance() noexcept
{
    static std::atomic<std::uint64_t> next{1};
    const auto id = next.fetch_add(1, std::memory_order_relaxed);
    if (id == 0)
    {
        std::terminate();
    }
    return id;
}
} // namespace

SettlementMap::SettlementMap(
    SettlementGrid grid,
    WorldTilePosition sourceRegionCenter,
    std::int32_t sourceRegionWidth,
    std::int32_t sourceRegionHeight,
    std::int32_t localTilesPerWorldTile,
    std::uint64_t generationSeed
) noexcept :
    instanceId_(nextMapInstance()), grid_(std::move(grid)),
    naturalFeatures_(grid_.width(), grid_.height()),
    objectState_(grid_.width(), grid_.height()),
    sourceRegionCenter_(sourceRegionCenter),
    sourceRegionWidth_(sourceRegionWidth),
    sourceRegionHeight_(sourceRegionHeight),
    localTilesPerWorldTile_(localTilesPerWorldTile),
    generationSeed_(generationSeed)
{
}

    SettlementGrid& SettlementMap::grid() noexcept
    {
        return grid_;
    }


    const SettlementGrid& SettlementMap::grid() const noexcept
    {
        return grid_;
    }


    WorldTilePosition SettlementMap::sourceRegionCenter() const noexcept
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


    SettlementCommandState& SettlementMap::commandState() noexcept
    {
        return commandState_;
    }


    const SettlementCommandState&
    SettlementMap::commandState() const noexcept
    {
        return commandState_;
    }
}

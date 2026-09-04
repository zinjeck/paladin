#pragma once

#include "world/WorldGrid.h"
#include "world/WorldPosition.h"
#include "world/settlements/objects/SettlementObjectState.h"

#include <cstdint>

namespace Paladin
{
    class SettlementMap
    {
    public:
        SettlementMap(
            WorldGrid grid,
            WorldPosition sourceRegionCenter,
            std::int32_t sourceRegionWidth,
            std::int32_t sourceRegionHeight,
            std::int32_t localTilesPerWorldTile,
            std::uint64_t generationSeed
        ) noexcept;

        [[nodiscard]]
        WorldGrid& grid() noexcept;

        [[nodiscard]]
        const WorldGrid& grid() const noexcept;

        [[nodiscard]]
        WorldPosition sourceRegionCenter() const noexcept;

        [[nodiscard]]
        std::int32_t sourceRegionWidth() const noexcept;

        [[nodiscard]]
        std::int32_t sourceRegionHeight() const noexcept;

        [[nodiscard]]
        std::int32_t localTilesPerWorldTile() const noexcept;

        [[nodiscard]]
        std::uint64_t generationSeed() const noexcept;

        [[nodiscard]]
        SettlementObjectState& objectState() noexcept;

        [[nodiscard]]
        const SettlementObjectState& objectState() const noexcept;

    private:
        WorldGrid grid_;
        SettlementObjectState objectState_;
        WorldPosition sourceRegionCenter_;
        std::int32_t sourceRegionWidth_ = 0;
        std::int32_t sourceRegionHeight_ = 0;
        std::int32_t localTilesPerWorldTile_ = 0;
        std::uint64_t generationSeed_ = 0;
    };
}

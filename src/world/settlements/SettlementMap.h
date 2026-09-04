#pragma once

#include "world/SettlementGrid.h"
#include "world/WorldTilePosition.h"
#include "world/settlements/objects/SettlementObjectState.h"
#include "world/settlements/commands/SettlementCommandState.h"

#include <cstdint>

namespace Paladin
{
    class SettlementMap
    {
    public:
        SettlementMap(
            SettlementGrid grid,
            WorldTilePosition sourceRegionCenter,
            std::int32_t sourceRegionWidth,
            std::int32_t sourceRegionHeight,
            std::int32_t localTilesPerWorldTile,
            std::uint64_t generationSeed
        ) noexcept;

        [[nodiscard]]
        SettlementGrid& grid() noexcept;

        [[nodiscard]]
        const SettlementGrid& grid() const noexcept;

        [[nodiscard]]
        WorldTilePosition sourceRegionCenter() const noexcept;

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

        [[nodiscard]]
        SettlementCommandState& commandState() noexcept;

        [[nodiscard]]
        const SettlementCommandState& commandState() const noexcept;

    private:
        SettlementGrid grid_;
        SettlementObjectState objectState_;
        SettlementCommandState commandState_;
        WorldTilePosition sourceRegionCenter_;
        std::int32_t sourceRegionWidth_ = 0;
        std::int32_t sourceRegionHeight_ = 0;
        std::int32_t localTilesPerWorldTile_ = 0;
        std::uint64_t generationSeed_ = 0;
    };
}

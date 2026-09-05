#pragma once

#include "simulation/systems/SettlementActivitySystem.h"
#include "world/SettlementGrid.h"
#include "world/WorldTilePosition.h"
#include "world/settlements/SettlementEmploymentState.h"
#include "world/settlements/SettlementLogistics.h"
#include "world/settlements/SettlementNaturalFeatures.h"
#include "world/settlements/commands/SettlementCommandState.h"
#include "world/settlements/objects/SettlementObjectState.h"

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

        // Simulation owns maps through unique_ptr. Maps never move or copy;
        // identity also invalidates caches if allocator storage is reused.
        SettlementMap(const SettlementMap&) = delete;
        SettlementMap& operator=(const SettlementMap&) = delete;
        SettlementMap(SettlementMap&&) = delete;
        SettlementMap& operator=(SettlementMap&&) = delete;

        [[nodiscard]]
        std::uint64_t instanceId() const noexcept
        {
            return instanceId_;
        }

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

        SettlementNaturalFeatures& naturalFeatures() noexcept { return naturalFeatures_; }
        const SettlementNaturalFeatures& naturalFeatures() const noexcept { return naturalFeatures_; }

        SettlementEmploymentState& employment() noexcept { return employment_; }
        const SettlementEmploymentState& employment() const noexcept { return employment_; }

        SettlementLogistics logistics;
        SettlementActivitySystem activities;

      private:
        const std::uint64_t instanceId_;
        SettlementEmploymentState employment_;
        SettlementGrid grid_;
        SettlementNaturalFeatures naturalFeatures_;
        SettlementObjectState objectState_;
        SettlementCommandState commandState_;
        WorldTilePosition sourceRegionCenter_;
        std::int32_t sourceRegionWidth_ = 0;
        std::int32_t sourceRegionHeight_ = 0;
        std::int32_t localTilesPerWorldTile_ = 0;
        std::uint64_t generationSeed_ = 0;
    };
}

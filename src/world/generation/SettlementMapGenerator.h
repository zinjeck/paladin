#pragma once

#include "world/WorldPosition.h"

#include <cstdint>
#include <memory>

namespace Paladin
{
    class SettlementMap;
    class WorldGrid;

    struct SettlementMapGenerationSettings
    {
        std::int32_t localTilesPerWorldTile = 64;
        double coordinateWarpStrength = 0.62;
        double coastlineNoiseStrength = 0.18;
        double biomeBoundaryNoiseStrength = 0.075;
        double localElevationNoiseStrength = 0.030;
    };

    [[nodiscard]]
    SettlementMapGenerationSettings
    defaultSettlementMapGenerationSettings() noexcept;

    class SettlementMapGenerator
    {
    public:
        [[nodiscard]]
        std::unique_ptr<SettlementMap> generate(
            const WorldGrid& sourceGrid,
            WorldPosition sourceRegionCenter,
            std::int32_t sourceRegionWidth,
            std::int32_t sourceRegionHeight,
            std::uint64_t worldSeed,
            const SettlementMapGenerationSettings& settings =
                defaultSettlementMapGenerationSettings()
        ) const;
    };
}

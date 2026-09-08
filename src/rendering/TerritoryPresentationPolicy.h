#pragma once

#include <cstdint>

namespace Paladin
{
    struct TerritoryPresentationPolicy
    {
        double enterPoliticalViewTilePixels = 2.0;
        double exitPoliticalViewTilePixels = 2.5;

        std::uint8_t politicalFillAlpha = 150;
        bool cartographicBase = false;
        float closeViewBorderWidth = 2.0F;
        float politicalViewBorderWidth = 1.25F;
        float borderBackdropExtraWidth = 1.5F;

        float minimumLabelPixelSize = 0.75F;
        float maximumLabelPixelSize = 3.0F;
        float labelTerritoryWidthFraction = 0.85F;
    };

    [[nodiscard]]
    const TerritoryPresentationPolicy&
    defaultTerritoryPresentationPolicy() noexcept;
} // namespace Paladin

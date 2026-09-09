#pragma once

#include "rendering/SceneDetail.h"

#include <algorithm>
#include <cmath>

namespace Paladin
{
    // One authoritative zoom policy for the world screen.  The values are
    // expressed in effective screen pixels per logical world tile, so the same
    // presentation bands survive projection switches, window-size changes and
    // different world dimensions.
    struct WorldPresentationPolicy
    {
        double realmToRegionalBeginPixels = 5.5;
        double realmToRegionalEndPixels = 10.0;
        double regionalToLocalBeginPixels = 28.0;
        double regionalToLocalEndPixels = 40.0;

        // Realm borders remain legible in the closest world view without
        // becoming the dominant visual layer.
        float closeRealmBorderOpacity = 0.22F;

        float minimumRealmLabelPixelSize = 0.75F;
        float maximumRealmLabelPixelSize = 3.0F;
        float realmLabelTerritoryWidthFraction = 0.85F;
    };

    struct WorldPresentationState
    {
        float realmFillWeight = 1.0F;
        float realmLabelWeight = 1.0F;
        float realmBorderWeight = 1.0F;
        float regionalWeight = 0.0F;
        float settlementMarkerWeight = 0.0F;
        float localWorldWeight = 0.0F;
    };

    [[nodiscard]]
    inline WorldPresentationState worldPresentationState(
        double effectiveTilePixels,
        const WorldPresentationPolicy& policy = {}
    ) noexcept
    {
        if (!std::isfinite(effectiveTilePixels) || effectiveTilePixels <= 0.0)
        {
            return {};
        }

        const double realmBegin =
            std::max(0.0, policy.realmToRegionalBeginPixels);
        const double realmEnd = std::max(
            realmBegin + 0.001,
            policy.realmToRegionalEndPixels
        );
        const double localBegin = std::max(
            realmEnd,
            policy.regionalToLocalBeginPixels
        );
        const double localEnd = std::max(
            localBegin + 0.001,
            policy.regionalToLocalEndPixels
        );

        const float regionalArrival = static_cast<float>(
            detailBlend(effectiveTilePixels, realmBegin, realmEnd)
        );
        const float localArrival = static_cast<float>(
            detailBlend(effectiveTilePixels, localBegin, localEnd)
        );
        const float realmWeight = 1.0F - regionalArrival;
        const float regionalWeight = std::clamp(
            regionalArrival * (1.0F - localArrival),
            0.0F,
            1.0F
        );
        const float closeBorderOpacity = std::clamp(
            policy.closeRealmBorderOpacity,
            0.0F,
            1.0F
        );

        return {
            realmWeight,
            realmWeight,
            1.0F - localArrival * (1.0F - closeBorderOpacity),
            regionalWeight,
            regionalWeight,
            localArrival
        };
    }
} // namespace Paladin

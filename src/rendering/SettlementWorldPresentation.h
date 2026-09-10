#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Paladin
{
    // Settlements are one domain concept. World-map presentation scales from
    // settlement data instead of introducing village/town/city type branches.
    struct SettlementWorldPresentationPolicy
    {
        float minimumMarkerDiameterPixels = 10.0F;
        float maximumMarkerDiameterPixels = 22.0F;
        std::uint64_t populationAtMaximumMarker = 4096;
        float minimumLabelPixelSize = 1.45F;
        float maximumLabelPixelSize = 1.90F;
    };

    struct SettlementWorldPresentation
    {
        float markerDiameterPixels = 10.0F;
        float borderPixels = 1.5F;
        float labelPixelSize = 1.45F;
    };

    [[nodiscard]]
    inline SettlementWorldPresentation settlementWorldPresentation(
        std::uint64_t population,
        const SettlementWorldPresentationPolicy& policy = {}
    ) noexcept
    {
        const float minimumDiameter =
            std::max(1.0F, policy.minimumMarkerDiameterPixels);
        const float maximumDiameter =
            std::max(minimumDiameter, policy.maximumMarkerDiameterPixels);
        const float minimumLabel = std::max(0.5F, policy.minimumLabelPixelSize);
        const float maximumLabel =
            std::max(minimumLabel, policy.maximumLabelPixelSize);
        const std::uint64_t maximumPopulation =
            std::max<std::uint64_t>(1, policy.populationAtMaximumMarker);
        const std::uint64_t clampedPopulation =
            std::min(population, maximumPopulation);

        const double denominator = std::log1p(double(maximumPopulation));
        const double scale = denominator > 0.0
                                 ? std::log1p(double(clampedPopulation)) /
                                       denominator
                                 : 0.0;
        const float progress =
            static_cast<float>(std::clamp(scale, 0.0, 1.0));
        const float diameter =
            minimumDiameter + (maximumDiameter - minimumDiameter) * progress;

        return {
            diameter,
            std::clamp(diameter * 0.12F, 1.5F, 3.0F),
            minimumLabel + (maximumLabel - minimumLabel) * progress
        };
    }
} // namespace Paladin

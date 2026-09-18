#pragma once
#include <algorithm>
#include <cstdint>
namespace Paladin
{
    inline constexpr float SettlementMapIconDiameterPixels = 18.F;
    // Cartographic symbols are universal, not a population tier or a miniature
    // city. Their footprint and label scale remain unchanged at every zoom.
    struct SettlementWorldPresentationPolicy
    {
        float labelPixelSize = 1.45F;
    };
    struct SettlementWorldPresentation
    {
        float markerDiameterPixels = SettlementMapIconDiameterPixels;
        float borderPixels = 1.F;
        float labelPixelSize = 1.45F;
    };
    inline SettlementWorldPresentation settlementWorldPresentation(
        std::uint64_t /*population*/, const SettlementWorldPresentationPolicy& policy = {}) noexcept
    {
        return {SettlementMapIconDiameterPixels,1.F,std::clamp(policy.labelPixelSize,.5F,3.F)};
    }
}

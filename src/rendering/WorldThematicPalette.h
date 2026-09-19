#pragma once
#include "rendering/Renderer.h"
#include "rendering/WorldPresentation.h"
#include <array>
#include <cstdint>
namespace Paladin
{
    inline constexpr RenderColor GovernmentTribal{183,106,54,255};
    inline constexpr RenderColor GovernmentCivic{84,138,196,255};
    inline constexpr RenderColor UnclaimedLand{113,109,112,255};
    inline constexpr RenderColor ThematicWater{8,15,27,255};
    inline constexpr std::array<RenderColor,7> PopulationColors{{
        {32,44,67,255},{57,70,88,255},{70,98,125,255},{84,138,196,255},
        {126,156,170,255},{175,201,214,255},{244,243,232,255}}};
    inline constexpr std::array<std::uint64_t,6> PopulationThresholds{1,32,128,512,2048,8192};
    // Estimated residents per logical world tile. Zero population is a quiet
    // wash, not an opaque administrative polygon; shaded terrain reads through.
    inline RenderColor populationDensityColor(double people) noexcept
    {
        constexpr std::array<double,6> stops{1,4,16,64,256,1024};
        std::size_t band=0;
        while(band<stops.size() && people>=stops[band]) ++band;
        auto color=PopulationColors[band];
        color.alpha=static_cast<std::uint8_t>(band==0?26:112+band*15);
        return color;
    }
    inline RenderColor populationMapColor(std::uint64_t people) noexcept
    {
        std::size_t index=0;
        while (index<PopulationThresholds.size() && people>=PopulationThresholds[index]) ++index;
        return PopulationColors[index];
    }
}

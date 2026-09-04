#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Paladin
{
    inline constexpr std::string_view defaultLandmassTemplateId =
        "godot_base";

    struct NormalizedMapPoint
    {
        double x = 0.5;
        double y = 0.5;
    };

    enum class ShapeNoiseType
    {
        Value,
        Simplex
    };

    struct FractalShapeNoise
    {
        std::uint64_t seedOffset = 0;
        ShapeNoiseType type = ShapeNoiseType::Simplex;
        double frequency = 0.01;
        std::int32_t octaveCount = 1;
        double gain = 0.5;
        double lacunarity = 2.0;
        double amplitude = 0.0;
    };

    struct ContinentLobeTemplate
    {
        std::int32_t minimumCount = 1;
        std::int32_t maximumCount = 1;
        double maximumOffsetWidthFraction = 0.0;
        double maximumOffsetHeightFraction = 0.0;
        double minimumRadiusWidthFraction = 0.1;
        double maximumRadiusWidthFraction = 0.1;
        double minimumRadiusHeightFraction = 0.1;
        double maximumRadiusHeightFraction = 0.1;
        double minimumStrength = 1.0;
        double maximumStrength = 1.0;
        double distanceExponent = 2.0;
        double falloffExponent = 1.0;
    };

    struct CoastlineBreakupTemplate
    {
        FractalShapeNoise noise;
        double interiorCoreThreshold = 0.62;
        double coastlineCoreThreshold = 0.18;
        double interiorAmplitude = 0.025;
        double coastlineAmplitude = 0.18;
        double offshoreAmplitude = 0.045;
    };

    struct IslandTemplate
    {
        FractalShapeNoise noise;
        double maximumContinentCore = 0.14;
        double noiseThreshold = 0.50;
        double amplitude = 1.35;
    };

    struct EdgeFalloffTemplate
    {
        double exponent = 7.0;
        double strength = 0.95;
    };

    struct LandmassGenerationTemplate
    {
        std::string id;
        std::string displayName;
        std::int32_t defaultMinimumContinentCount = 1;
        std::int32_t defaultMaximumContinentCount = 1;
        std::vector<NormalizedMapPoint> continentSlots;
        double centerJitterX = 0.0;
        double centerJitterY = 0.0;
        double minimumCenterX = 0.0;
        double maximumCenterX = 1.0;
        double minimumCenterY = 0.0;
        double maximumCenterY = 1.0;
        ContinentLobeTemplate continentLobes;
        FractalShapeNoise continentNoise;
        FractalShapeNoise regionalNoise;
        CoastlineBreakupTemplate coastline;
        IslandTemplate islands;
        EdgeFalloffTemplate edgeFalloff;
        double elevationBias = 0.0;
        double maximumContinentCore = 1.0;
    };

    struct LandmassContinentCountRange
    {
        std::int32_t minimum = 0;
        std::int32_t maximum = 0;
    };

    [[nodiscard]]
    std::span<const LandmassGenerationTemplate>
    landmassGenerationTemplates();

    [[nodiscard]]
    const LandmassGenerationTemplate* findLandmassGenerationTemplate(
        std::string_view id
    );

    [[nodiscard]]
    LandmassContinentCountRange resolveLandmassContinentCountRange(
        const LandmassGenerationTemplate& definition,
        std::int32_t requestedMinimum,
        std::int32_t requestedMaximum
    ) noexcept;

    [[nodiscard]]
    bool isValidLandmassGenerationTemplate(
        const LandmassGenerationTemplate& definition
    ) noexcept;
}

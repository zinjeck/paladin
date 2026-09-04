#include "world/generation/LandmassGenerationTemplate.h"

#include <cmath>
#include <vector>

namespace Paladin
{
    namespace
    {
        LandmassGenerationTemplate createGodotBaseTemplate()
        {
            LandmassGenerationTemplate definition;
            definition.id = defaultLandmassTemplateId;
            definition.displayName = "Godot Base";
            definition.defaultMinimumContinentCount = 3;
            definition.defaultMaximumContinentCount = 5;

            definition.continentSlots = {
                {0.22, 0.24},
                {0.50, 0.22},
                {0.78, 0.25},
                {0.28, 0.70},
                {0.58, 0.66},
                {0.82, 0.70}
            };

            definition.centerJitterX = 0.08;
            definition.centerJitterY = 0.08;
            definition.minimumCenterX = 0.14;
            definition.maximumCenterX = 0.86;
            definition.minimumCenterY = 0.14;
            definition.maximumCenterY = 0.86;

            definition.continentLobes = {
                6,
                9,
                0.17,
                0.15,
                0.075,
                0.18,
                0.075,
                0.19,
                0.50,
                0.86,
                1.35,
                1.18
            };

            definition.continentNoise = {
                0,
                ShapeNoiseType::Simplex,
                0.004,
                3,
                0.45,
                2.0,
                0.14
            };

            definition.regionalNoise = {
                9'917,
                ShapeNoiseType::Simplex,
                0.026,
                3,
                0.46,
                2.15,
                0.11
            };

            definition.coastline = {
                {
                    17'771,
                    ShapeNoiseType::Simplex,
                    0.085,
                    2,
                    0.55,
                    2.4,
                    1.0
                },
                0.62,
                0.18,
                0.025,
                0.18,
                0.045
            };

            definition.islands = {
                {
                    28'891,
                    ShapeNoiseType::Simplex,
                    0.021,
                    4,
                    0.52,
                    2.1,
                    1.0
                },
                0.14,
                0.50,
                1.35
            };

            definition.edgeFalloff = {
                7.0,
                0.95
            };

            definition.elevationBias = -0.14;
            definition.maximumContinentCore = 1.05;
            return definition;
        }

        const std::vector<LandmassGenerationTemplate>& templates()
        {
            static const std::vector<LandmassGenerationTemplate>
                definitions{
                    createGodotBaseTemplate()
                };

            return definitions;
        }

        bool isFiniteNonnegative(double value) noexcept
        {
            return std::isfinite(value) && value >= 0.0;
        }

        bool isValidNoise(
            const FractalShapeNoise& noise
        ) noexcept
        {
            const bool hasKnownType =
                noise.type == ShapeNoiseType::Value ||
                noise.type == ShapeNoiseType::Simplex;

            return
                hasKnownType &&
                std::isfinite(noise.frequency) &&
                noise.frequency > 0.0 &&
                noise.octaveCount > 0 &&
                isFiniteNonnegative(noise.gain) &&
                std::isfinite(noise.lacunarity) &&
                noise.lacunarity > 0.0 &&
                isFiniteNonnegative(noise.amplitude);
        }
    }

    std::span<const LandmassGenerationTemplate>
    landmassGenerationTemplates()
    {
        return templates();
    }

    const LandmassGenerationTemplate* findLandmassGenerationTemplate(
        std::string_view id
    )
    {
        for (const LandmassGenerationTemplate& definition
            : templates())
        {
            if (definition.id == id)
            {
                return &definition;
            }
        }

        return nullptr;
    }

    LandmassContinentCountRange resolveLandmassContinentCountRange(
        const LandmassGenerationTemplate& definition,
        std::int32_t requestedMinimum,
        std::int32_t requestedMaximum
    ) noexcept
    {
        return {
            requestedMinimum == 0
                ? definition.defaultMinimumContinentCount
                : requestedMinimum,
            requestedMaximum == 0
                ? definition.defaultMaximumContinentCount
                : requestedMaximum
        };
    }

    bool isValidLandmassGenerationTemplate(
        const LandmassGenerationTemplate& definition
    ) noexcept
    {
        if (
            definition.id.empty() ||
            definition.displayName.empty() ||
            definition.defaultMinimumContinentCount <= 0 ||
            definition.defaultMaximumContinentCount <
                definition.defaultMinimumContinentCount ||
            definition.continentSlots.empty() ||
            !isFiniteNonnegative(definition.centerJitterX) ||
            !isFiniteNonnegative(definition.centerJitterY) ||
            !std::isfinite(definition.minimumCenterX) ||
            !std::isfinite(definition.maximumCenterX) ||
            !std::isfinite(definition.minimumCenterY) ||
            !std::isfinite(definition.maximumCenterY) ||
            definition.minimumCenterX > definition.maximumCenterX ||
            definition.minimumCenterY > definition.maximumCenterY
        )
        {
            return false;
        }

        if (
            static_cast<std::size_t>(
                definition.defaultMaximumContinentCount
            ) > definition.continentSlots.size()
        )
        {
            return false;
        }

        for (const NormalizedMapPoint& slot
            : definition.continentSlots)
        {
            if (!std::isfinite(slot.x) || !std::isfinite(slot.y))
            {
                return false;
            }
        }

        const ContinentLobeTemplate& lobes =
            definition.continentLobes;

        if (
            lobes.minimumCount <= 0 ||
            lobes.maximumCount < lobes.minimumCount ||
            !isFiniteNonnegative(
                lobes.maximumOffsetWidthFraction
            ) ||
            !isFiniteNonnegative(
                lobes.maximumOffsetHeightFraction
            ) ||
            !std::isfinite(lobes.minimumRadiusWidthFraction) ||
            lobes.minimumRadiusWidthFraction <= 0.0 ||
            !std::isfinite(lobes.maximumRadiusWidthFraction) ||
            lobes.maximumRadiusWidthFraction <
                lobes.minimumRadiusWidthFraction ||
            !std::isfinite(lobes.minimumRadiusHeightFraction) ||
            lobes.minimumRadiusHeightFraction <= 0.0 ||
            !std::isfinite(lobes.maximumRadiusHeightFraction) ||
            lobes.maximumRadiusHeightFraction <
                lobes.minimumRadiusHeightFraction ||
            !isFiniteNonnegative(lobes.minimumStrength) ||
            !std::isfinite(lobes.maximumStrength) ||
            lobes.maximumStrength < lobes.minimumStrength ||
            !std::isfinite(lobes.distanceExponent) ||
            lobes.distanceExponent <= 0.0 ||
            !std::isfinite(lobes.falloffExponent) ||
            lobes.falloffExponent <= 0.0
        )
        {
            return false;
        }

        const CoastlineBreakupTemplate& coastline =
            definition.coastline;

        const IslandTemplate& islands = definition.islands;

        return
            isValidNoise(definition.continentNoise) &&
            isValidNoise(definition.regionalNoise) &&
            isValidNoise(coastline.noise) &&
            std::isfinite(coastline.interiorCoreThreshold) &&
            std::isfinite(coastline.coastlineCoreThreshold) &&
            coastline.interiorCoreThreshold >=
                coastline.coastlineCoreThreshold &&
            isFiniteNonnegative(coastline.interiorAmplitude) &&
            isFiniteNonnegative(coastline.coastlineAmplitude) &&
            isFiniteNonnegative(coastline.offshoreAmplitude) &&
            isValidNoise(islands.noise) &&
            isFiniteNonnegative(islands.maximumContinentCore) &&
            std::isfinite(islands.noiseThreshold) &&
            isFiniteNonnegative(islands.amplitude) &&
            std::isfinite(definition.edgeFalloff.exponent) &&
            definition.edgeFalloff.exponent > 0.0 &&
            isFiniteNonnegative(definition.edgeFalloff.strength) &&
            std::isfinite(definition.elevationBias) &&
            std::isfinite(definition.maximumContinentCore) &&
            definition.maximumContinentCore > 0.0;
    }
}

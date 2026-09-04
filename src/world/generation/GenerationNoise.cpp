#include "world/generation/GenerationNoise.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Paladin
{
    namespace
    {
        double smooth(double value) noexcept
        {
            return value * value * (3.0 - 2.0 * value);
        }

        double interpolate(
            double first,
            double second,
            double amount
        ) noexcept
        {
            return first + (second - first) * amount;
        }

        double latticeValue(
            std::int64_t x,
            std::int64_t y,
            std::uint64_t seed
        ) noexcept
        {
            const std::uint64_t xBits =
                static_cast<std::uint64_t>(x);

            const std::uint64_t yBits =
                static_cast<std::uint64_t>(y);

            const std::uint64_t hash =
                GenerationNoise::mix(
                    seed
                    ^ GenerationNoise::mix(
                        xBits + 0x9E37'79B9'7F4A'7C15ULL
                    )
                    ^ GenerationNoise::mix(
                        yBits + 0xC2B2'AE3D'27D4'EB4FULL
                    )
                );

            constexpr double inverse53Bits =
                1.0 / 9'007'199'254'740'992.0;

            return
                static_cast<double>(hash >> 11)
                * inverse53Bits
                * 2.0
                - 1.0;
        }

        double simplexGradient(
            std::int64_t x,
            std::int64_t y,
            std::uint64_t seed,
            double offsetX,
            double offsetY
        ) noexcept
        {
            constexpr std::array<std::array<double, 2>, 12>
                gradients{{
                    {{1.0, 1.0}},
                    {{-1.0, 1.0}},
                    {{1.0, -1.0}},
                    {{-1.0, -1.0}},
                    {{1.0, 0.0}},
                    {{-1.0, 0.0}},
                    {{1.0, 0.0}},
                    {{-1.0, 0.0}},
                    {{0.0, 1.0}},
                    {{0.0, -1.0}},
                    {{0.0, 1.0}},
                    {{0.0, -1.0}}
                }};

            const std::uint64_t hash =
                GenerationNoise::mix(
                    seed
                    ^ GenerationNoise::mix(
                        static_cast<std::uint64_t>(x)
                        + 0x9E37'79B9'7F4A'7C15ULL
                    )
                    ^ GenerationNoise::mix(
                        static_cast<std::uint64_t>(y)
                        + 0xC2B2'AE3D'27D4'EB4FULL
                    )
                );

            const auto& gradient = gradients[
                static_cast<std::size_t>(hash % gradients.size())
            ];

            return
                gradient[0] * offsetX
                + gradient[1] * offsetY;
        }
    }

    double GenerationNoise::fractal(
        double x,
        double y,
        std::uint64_t seed,
        int octaveCount,
        double persistence,
        double lacunarity
    ) noexcept
    {
        double total = 0.0;
        double amplitude = 1.0;
        double amplitudeTotal = 0.0;
        double frequency = 1.0;

        for (int octave = 0; octave < octaveCount; ++octave)
        {
            total +=
                valueNoise(
                    x * frequency,
                    y * frequency,
                    seed
                        + static_cast<std::uint64_t>(octave)
                            * 0x9E37'79B9'7F4A'7C15ULL
                )
                * amplitude;

            amplitudeTotal += amplitude;
            amplitude *= persistence;
            frequency *= lacunarity;
        }

        return amplitudeTotal > 0.0
            ? total / amplitudeTotal
            : 0.0;
    }

    double GenerationNoise::simplexFractal(
        double x,
        double y,
        std::uint64_t seed,
        int octaveCount,
        double persistence,
        double lacunarity
    ) noexcept
    {
        double total = 0.0;
        double amplitude = 1.0;
        double amplitudeTotal = 0.0;
        double frequency = 1.0;

        for (int octave = 0; octave < octaveCount; ++octave)
        {
            total +=
                simplexNoise(
                    x * frequency,
                    y * frequency,
                    seed
                        + static_cast<std::uint64_t>(octave)
                            * 0x9E37'79B9'7F4A'7C15ULL
                )
                * amplitude;

            amplitudeTotal += amplitude;
            amplitude *= persistence;
            frequency *= lacunarity;
        }

        return amplitudeTotal > 0.0
            ? total / amplitudeTotal
            : 0.0;
    }

    std::uint64_t GenerationNoise::mix(
        std::uint64_t value
    ) noexcept
    {
        value += 0x9E37'79B9'7F4A'7C15ULL;
        value = (value ^ (value >> 30))
            * 0xBF58'476D'1CE4'E5B9ULL;
        value = (value ^ (value >> 27))
            * 0x94D0'49BB'1331'11EBULL;

        return value ^ (value >> 31);
    }

    double GenerationNoise::valueNoise(
        double x,
        double y,
        std::uint64_t seed
    ) noexcept
    {
        const auto x0 =
            static_cast<std::int64_t>(std::floor(x));

        const auto y0 =
            static_cast<std::int64_t>(std::floor(y));

        const std::int64_t x1 = x0 + 1;
        const std::int64_t y1 = y0 + 1;

        const double blendX = smooth(x - static_cast<double>(x0));
        const double blendY = smooth(y - static_cast<double>(y0));

        const double top = interpolate(
            latticeValue(x0, y0, seed),
            latticeValue(x1, y0, seed),
            blendX
        );

        const double bottom = interpolate(
            latticeValue(x0, y1, seed),
            latticeValue(x1, y1, seed),
            blendX
        );

        return interpolate(top, bottom, blendY);
    }

    double GenerationNoise::simplexNoise(
        double x,
        double y,
        std::uint64_t seed
    ) noexcept
    {
        constexpr double skewFactor =
            0.36602540378443864676;

        constexpr double unskewFactor =
            0.21132486540518711775;

        const double skew = (x + y) * skewFactor;

        const auto cellX =
            static_cast<std::int64_t>(std::floor(x + skew));

        const auto cellY =
            static_cast<std::int64_t>(std::floor(y + skew));

        const double unskew =
            static_cast<double>(cellX + cellY) * unskewFactor;

        const double originX =
            static_cast<double>(cellX) - unskew;

        const double originY =
            static_cast<double>(cellY) - unskew;

        const double offsetX0 = x - originX;
        const double offsetY0 = y - originY;

        const std::int64_t stepX = offsetX0 > offsetY0 ? 1 : 0;
        const std::int64_t stepY = offsetX0 > offsetY0 ? 0 : 1;

        const double offsetX1 =
            offsetX0 - static_cast<double>(stepX) + unskewFactor;

        const double offsetY1 =
            offsetY0 - static_cast<double>(stepY) + unskewFactor;

        const double offsetX2 =
            offsetX0 - 1.0 + 2.0 * unskewFactor;

        const double offsetY2 =
            offsetY0 - 1.0 + 2.0 * unskewFactor;

        const auto cornerContribution = [seed](
            std::int64_t cornerX,
            std::int64_t cornerY,
            double cornerOffsetX,
            double cornerOffsetY
        ) noexcept
        {
            double attenuation =
                0.5
                - cornerOffsetX * cornerOffsetX
                - cornerOffsetY * cornerOffsetY;

            if (attenuation <= 0.0)
            {
                return 0.0;
            }

            attenuation *= attenuation;

            return
                attenuation * attenuation
                * simplexGradient(
                    cornerX,
                    cornerY,
                    seed,
                    cornerOffsetX,
                    cornerOffsetY
                );
        };

        const double value = 70.0 * (
            cornerContribution(
                cellX,
                cellY,
                offsetX0,
                offsetY0
            )
            + cornerContribution(
                cellX + stepX,
                cellY + stepY,
                offsetX1,
                offsetY1
            )
            + cornerContribution(
                cellX + 1,
                cellY + 1,
                offsetX2,
                offsetY2
            )
        );

        return std::clamp(value, -1.0, 1.0);
    }
}

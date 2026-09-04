#include "world/generation/GenerationNoise.h"

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

    double GenerationNoise::unit(
        std::uint64_t seed,
        std::uint64_t stream
    ) noexcept
    {
        const std::uint64_t hash =
            mix(
                seed
                + stream * 0x9E37'79B9'7F4A'7C15ULL
            );

        constexpr double inverse53Bits =
            1.0 / 9'007'199'254'740'992.0;

        return
            static_cast<double>(hash >> 11)
            * inverse53Bits;
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
}

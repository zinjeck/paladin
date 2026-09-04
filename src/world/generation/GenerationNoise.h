#pragma once

#include <cstdint>

namespace Paladin
{
    class GenerationNoise
    {
    public:
        [[nodiscard]]
        static double fractal(
            double x,
            double y,
            std::uint64_t seed,
            int octaveCount,
            double persistence = 0.5,
            double lacunarity = 2.0
        ) noexcept;

        [[nodiscard]]
        static double simplexFractal(
            double x,
            double y,
            std::uint64_t seed,
            int octaveCount,
            double persistence = 0.5,
            double lacunarity = 2.0
        ) noexcept;

        [[nodiscard]]
        static std::uint64_t mix(
            std::uint64_t value
        ) noexcept;

    private:
        [[nodiscard]]
        static double valueNoise(
            double x,
            double y,
            std::uint64_t seed
        ) noexcept;

        [[nodiscard]]
        static double simplexNoise(
            double x,
            double y,
            std::uint64_t seed
        ) noexcept;
    };
}

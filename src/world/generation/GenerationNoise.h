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
        static std::uint64_t mix(
            std::uint64_t value
        ) noexcept;

        [[nodiscard]]
        static double unit(
            std::uint64_t seed,
            std::uint64_t stream
        ) noexcept;

    private:
        [[nodiscard]]
        static double valueNoise(
            double x,
            double y,
            std::uint64_t seed
        ) noexcept;
    };
}

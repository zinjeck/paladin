#include "world/generation/WorldGenerationSeed.h"

#include <atomic>
#include <chrono>
#include <random>

namespace Paladin
{
    namespace
    {
        constexpr std::uint64_t goldenRatioIncrement =
            0x9E37'79B9'7F4A'7C15ULL;

        std::uint64_t mixSeed(std::uint64_t value) noexcept
        {
            value = (value ^ (value >> 30U))
                * 0xBF58'476D'1CE4'E5B9ULL;
            value = (value ^ (value >> 27U))
                * 0x94D0'49BB'1331'11EBULL;
            return value ^ (value >> 31U);
        }

        std::uint64_t initialSeedMaterial()
        {
            std::uint64_t material = static_cast<std::uint64_t>(
                std::chrono::high_resolution_clock::now()
                    .time_since_epoch()
                    .count()
            );

            try
            {
                std::random_device entropy;
                material ^= static_cast<std::uint64_t>(entropy()) << 32U;
                material ^= static_cast<std::uint64_t>(entropy());
            }
            catch (...)
            {
                // The clock and monotonically advanced state still provide
                // distinct session seeds if system entropy is unavailable.
            }

            return mixSeed(material);
        }
    }

    std::uint64_t nextRandomWorldSeed()
    {
        static std::atomic<std::uint64_t> state{
            initialSeedMaterial()
        };

        return mixSeed(
            state.fetch_add(
                goldenRatioIncrement,
                std::memory_order_relaxed
            )
        );
    }

    WorldGenerationSettings withRandomWorldSeed(
        WorldGenerationSettings settings
    )
    {
        settings.seed = nextRandomWorldSeed();
        return settings;
    }
}

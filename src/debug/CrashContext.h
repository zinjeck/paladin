#pragma once

#include <atomic>
#include <cstdint>

namespace Paladin
{
    // Fixed-size, allocation-free telemetry. Crash handlers never traverse the
    // damaged World, SDL renderer, strings, or entity registries.
    struct CrashContext
    {
        static inline std::atomic<const char*> screen{"Starting"};
        static inline std::atomic<const char*> phase{"Process startup"};
        static inline std::atomic<std::uint64_t> seed{0};
        static inline std::atomic<std::uint64_t> minute{0};
        static inline std::atomic<std::uint64_t> tick{0};
        static inline std::atomic<std::uint64_t> settlements{0};
        static inline std::atomic<std::uint64_t> soldiers{0};
        static inline std::atomic<std::uint64_t> armies{0};
        static inline std::atomic<std::uint64_t> shipments{0};
        static inline std::atomic<std::uint64_t> activeSettlement{0};
        static inline std::atomic<double> elapsedMinutes{0};
        static inline std::atomic<double> tickMilliseconds{0};

        // Only pass string literals with process lifetime.
        static void setScreen(const char* value) noexcept
        {
            screen.store(value, std::memory_order_relaxed);
        }
        static void setPhase(const char* value) noexcept
        {
            phase.store(value, std::memory_order_relaxed);
        }
    };
} // namespace Paladin

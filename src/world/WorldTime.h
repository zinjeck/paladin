#pragma once

#include <cstdint>

namespace Paladin
{
    class WorldTime
    {
    public:
        WorldTime() noexcept;

        void advanceMinutes(std::uint64_t gameMinutes) noexcept;

        [[nodiscard]]
        std::uint64_t day() const noexcept;

        [[nodiscard]]
        int hour() const noexcept;

        [[nodiscard]]
        int minute() const noexcept;

        [[nodiscard]]
        int second() const noexcept;

        [[nodiscard]]
        double secondsIntoDay() const noexcept;

        [[nodiscard]]
        double totalGameSeconds() const noexcept;

        [[nodiscard]]
        std::uint64_t totalGameMinutes() const noexcept;

    private:
        static constexpr std::uint64_t MinutesPerHour = 60;
        static constexpr std::uint64_t HoursPerDay = 24;

        static constexpr std::uint64_t MinutesPerDay =
            MinutesPerHour * HoursPerDay;

        // Paladin begins at 06:00 on Day 1. Authoritative world time is
        // integral; rendering may interpolate independently later.
        std::uint64_t totalGameMinutes_ = 6 * MinutesPerHour;
    };
}

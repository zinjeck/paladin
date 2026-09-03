#pragma once

#include <cstdint>

namespace Paladin
{
    class WorldTime
    {
    public:
        WorldTime() noexcept;

        void advance(double gameSeconds) noexcept;

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

    private:
        static constexpr double SecondsPerMinute = 60.0;
        static constexpr double MinutesPerHour = 60.0;
        static constexpr double HoursPerDay = 24.0;

        static constexpr double SecondsPerHour =
            SecondsPerMinute * MinutesPerHour;

        static constexpr double SecondsPerDay =
            SecondsPerHour * HoursPerDay;

        // Paladin begins at 06:00 on Day 1.
        double totalGameSeconds_ =
            6.0 * SecondsPerHour;
    };
}
#include "world/WorldTime.h"

#include <cmath>

namespace Paladin
{
    WorldTime::WorldTime() noexcept = default;


    void WorldTime::advance(
        double gameSeconds
    ) noexcept
    {
        if (gameSeconds <= 0.0)
        {
            return;
        }

        totalGameSeconds_ += gameSeconds;
    }


    std::uint64_t WorldTime::day() const noexcept
    {
        return
            static_cast<std::uint64_t>(
                totalGameSeconds_ / SecondsPerDay
            ) + 1;
    }


    int WorldTime::hour() const noexcept
    {
        const double secondsToday =
            secondsIntoDay();

        return static_cast<int>(
            secondsToday / SecondsPerHour
        );
    }


    int WorldTime::minute() const noexcept
    {
        const double secondsToday =
            secondsIntoDay();

        return static_cast<int>(
            std::fmod(
                secondsToday,
                SecondsPerHour
            ) / SecondsPerMinute
        );
    }


    int WorldTime::second() const noexcept
    {
        return static_cast<int>(
            std::fmod(
                secondsIntoDay(),
                SecondsPerMinute
            )
        );
    }


    double WorldTime::secondsIntoDay() const noexcept
    {
        return std::fmod(
            totalGameSeconds_,
            SecondsPerDay
        );
    }


    double WorldTime::totalGameSeconds() const noexcept
    {
        return totalGameSeconds_;
    }
}
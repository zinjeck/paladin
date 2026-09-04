#include "world/WorldTime.h"

#include <limits>

namespace Paladin
{
    WorldTime::WorldTime() noexcept = default;


    void WorldTime::advanceMinutes(
        std::uint64_t gameMinutes
    ) noexcept
    {
        if (gameMinutes == 0)
        {
            return;
        }

        constexpr std::uint64_t maximumMinutes =
            std::numeric_limits<std::uint64_t>::max();

        if (gameMinutes > maximumMinutes - totalGameMinutes_)
        {
            totalGameMinutes_ = maximumMinutes;
            return;
        }

        totalGameMinutes_ += gameMinutes;
    }


    std::uint64_t WorldTime::day() const noexcept
    {
        return
            totalGameMinutes_ / MinutesPerDay + 1;
    }


    int WorldTime::hour() const noexcept
    {
        return static_cast<int>(
            totalGameMinutes_ % MinutesPerDay / MinutesPerHour
        );
    }


    int WorldTime::minute() const noexcept
    {
        return static_cast<int>(
            totalGameMinutes_ % MinutesPerHour
        );
    }


    int WorldTime::second() const noexcept
    {
        return 0;
    }


    double WorldTime::secondsIntoDay() const noexcept
    {
        return static_cast<double>(
            totalGameMinutes_ % MinutesPerDay * 60
        );
    }


    double WorldTime::totalGameSeconds() const noexcept
    {
        return static_cast<double>(totalGameMinutes_) * 60.0;
    }


    std::uint64_t WorldTime::totalGameMinutes() const noexcept
    {
        return totalGameMinutes_;
    }
}

#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace Paladin
{
    enum class Season : std::uint8_t
    {
        Summer,
        Spring,
        Autumn,
        Winter
    };
    struct SeasonDefinition
    {
        std::string_view name;
        int sunriseMinute;
        int sunsetMinute;
    };
    inline constexpr std::array<SeasonDefinition, 4> seasons{
        {{"Summer", 5 * 60, 21 * 60},
         {"Spring", 6 * 60, 19 * 60},
         {"Autumn", 7 * 60, 18 * 60},
         {"Winter", 8 * 60, 17 * 60}}
    };
    inline Season seasonAtMinute(double minute) noexcept
    {
        return Season(std::uint64_t(std::max(0.0, minute) / (3 * 1440)) % 4);
    }
    inline const SeasonDefinition& seasonDefinition(Season season) noexcept
    {
        return seasons[std::size_t(season)];
    }
    inline bool isNight(double minute) noexcept
    {
        const auto& season = seasonDefinition(seasonAtMinute(minute));
        const double time = std::fmod(minute, 1440.0);
        return time < season.sunriseMinute || time >= season.sunsetMinute;
    }
} // namespace Paladin

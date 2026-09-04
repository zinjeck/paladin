#pragma once

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace Paladin
{
    inline constexpr std::size_t maximumFoundingNameLength = 40;

    struct MapColor
    {
        std::uint8_t red = 210;
        std::uint8_t green = 54;
        std::uint8_t blue = 54;

        friend constexpr bool operator==(
            const MapColor&,
            const MapColor&
        ) noexcept = default;
    };

    inline std::string trimFoundingName(
        std::string_view name
    )
    {
        std::size_t first = 0;

        while (
            first < name.size() &&
            std::isspace(
                static_cast<unsigned char>(name[first])
            )
        )
        {
            ++first;
        }

        std::size_t last = name.size();

        while (
            last > first &&
            std::isspace(
                static_cast<unsigned char>(name[last - 1])
            )
        )
        {
            --last;
        }

        return std::string(name.substr(first, last - first));
    }

    inline bool isValidFoundingName(
        std::string_view name
    ) noexcept
    {
        std::size_t first = 0;

        while (
            first < name.size() &&
            std::isspace(
                static_cast<unsigned char>(name[first])
            )
        )
        {
            ++first;
        }

        std::size_t last = name.size();

        while (
            last > first &&
            std::isspace(
                static_cast<unsigned char>(name[last - 1])
            )
        )
        {
            --last;
        }

        return
            last > first &&
            last - first <= maximumFoundingNameLength;
    }

    struct FoundingIdentity
    {
        std::string polityName;
        std::string cultureName;
        std::string capitalName;
        MapColor mapColor;
        std::string polityOriginId;
    };
}

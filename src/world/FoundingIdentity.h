#pragma once

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Paladin
{
    inline constexpr std::size_t maximumFoundingNameLength = 40;

    namespace Detail
    {
        struct FoundingNameBounds
        {
            std::size_t first = 0;
            std::size_t last = 0;
        };

        inline FoundingNameBounds foundingNameBounds(
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

            return {first, last};
        }
    }

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

    struct FlagCell
    {
        bool painted = false;
        MapColor color{};

        friend constexpr bool operator==(
            const FlagCell&,
            const FlagCell&
        ) noexcept = default;
    };

    struct PolityFlag
    {
        static constexpr std::size_t defaultWidth = 7;
        static constexpr std::size_t defaultHeight = 9;

        std::size_t width = defaultWidth;
        std::size_t height = defaultHeight;
        MapColor primaryColor{210, 54, 54};
        std::vector<FlagCell> cells = std::vector<FlagCell>(
            defaultWidth * defaultHeight,
            FlagCell{}
        );

        [[nodiscard]]
        bool isValid() const noexcept
        {
            return
                width > 0 &&
                height > 0 &&
                cells.size() == width * height;
        }

        friend bool operator==(
            const PolityFlag&,
            const PolityFlag&
        ) = default;
    };

    inline std::string trimFoundingName(
        std::string_view name
    )
    {
        const Detail::FoundingNameBounds bounds =
            Detail::foundingNameBounds(name);

        return std::string(
            name.substr(
                bounds.first,
                bounds.last - bounds.first
            )
        );
    }

    inline bool isValidFoundingName(
        std::string_view name
    ) noexcept
    {
        const Detail::FoundingNameBounds bounds =
            Detail::foundingNameBounds(name);

        return
            bounds.last > bounds.first &&
            bounds.last - bounds.first <=
                maximumFoundingNameLength;
    }

    struct FoundingIdentity
    {
        std::string polityName;
        std::string cultureName;
        std::string capitalName;
        MapColor mapColor;
        std::string polityOriginId;
        PolityFlag flag;
    };
}

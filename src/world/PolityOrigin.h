#pragma once

#include <array>
#include <string_view>

namespace Paladin
{
    struct PolityOriginDefinition
    {
        std::string_view id;
        std::string_view displayName;
    };

    inline constexpr std::array startingPolityOrigins{
        PolityOriginDefinition{"tribal", "Tribal"},
        PolityOriginDefinition{"civic", "Civic"}
    };

    inline constexpr bool isKnownPolityOrigin(
        std::string_view originId
    ) noexcept
    {
        for (const PolityOriginDefinition& definition
            : startingPolityOrigins)
        {
            if (definition.id == originId)
            {
                return true;
            }
        }

        return false;
    }
}

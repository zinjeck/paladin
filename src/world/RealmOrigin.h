#pragma once

#include <array>
#include <string_view>

namespace Paladin
{
    struct RealmOriginDefinition
    {
        std::string_view id;
        std::string_view displayName;
    };

    inline constexpr std::array startingRealmOrigins{
        RealmOriginDefinition{"tribal", "Tribal"},
        RealmOriginDefinition{"civic", "Civic"}
    };

    inline constexpr bool isKnownRealmOrigin(std::string_view originId) noexcept
    {
        for (const RealmOriginDefinition& definition : startingRealmOrigins)
        {
            if (definition.id == originId)
            {
                return true;
            }
        }

        return false;
    }
} // namespace Paladin

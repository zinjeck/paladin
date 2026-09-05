#pragma once

#include <span>
#include <string_view>

namespace Paladin
{
    struct SettlementResourceDefinition
    {
        std::string_view id;
        std::string_view displayName;
        bool edible = false;
    };

    namespace SettlementResourceTypes
    {
    inline constexpr std::string_view Fish = "fish";
    inline constexpr std::string_view Food = "food";
    inline constexpr std::string_view Materials = "materials";
    inline constexpr std::string_view Stone = "stone";
    inline constexpr std::string_view Lumber = "lumber";
    }

    class SettlementResourceCatalog
    {
    public:
        [[nodiscard]]
        static std::span<const SettlementResourceDefinition>
        definitions() noexcept;

        [[nodiscard]]
        static const SettlementResourceDefinition* definition(
            std::string_view resourceId
        ) noexcept;
    };
}

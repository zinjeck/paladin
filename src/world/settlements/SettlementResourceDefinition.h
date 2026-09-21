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
        bool emergencyOnly = false;
    };

    namespace SettlementResourceTypes
    {
        inline constexpr std::string_view Fish = "fish";
        inline constexpr std::string_view Meat = "meat";
        inline constexpr std::string_view Stone = "stone";
        inline constexpr std::string_view Lumber = "lumber";
        inline constexpr std::string_view Wheat = "wheat";
        inline constexpr std::string_view Bread = "bread";
        inline constexpr std::string_view Coal = "coal";
        inline constexpr std::string_view Iron = "iron";
        inline constexpr std::string_view Gold = "gold";
        inline constexpr std::string_view Rations = "rations";
    } // namespace SettlementResourceTypes

    class SettlementResourceCatalog
    {
    public:
        [[nodiscard]]
        static std::span<
            const SettlementResourceDefinition> definitions() noexcept;

        [[nodiscard]]
        static const SettlementResourceDefinition* definition(
            std::string_view resourceId
        ) noexcept;
    };
} // namespace Paladin

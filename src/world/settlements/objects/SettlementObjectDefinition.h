#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace Paladin
{
    enum class SettlementObjectCategory : std::uint8_t
    {
        Rule,
        Roads,
        Housing,
        Logistics,
        Food
    };

    enum class SettlementFootprintSelectionMode : std::uint8_t
    {
        Fixed,
        DragRectangle
    };

    struct SettlementObjectVisualStyle
    {
        std::array<std::uint8_t, 3> frameColor{};
        std::array<std::uint8_t, 3> fillColor{};
        float iconWidth = 1.0F;
        float iconHeight = 1.0F;
    };

    struct SettlementObjectDefinition
    {
        std::string_view id;
        std::string_view displayName;
        SettlementObjectCategory category =
            SettlementObjectCategory::Housing;
        std::int32_t menuOrder = 0;
        SettlementFootprintSelectionMode selectionMode =
            SettlementFootprintSelectionMode::Fixed;
        std::int32_t previewWidth = 1;
        std::int32_t previewHeight = 1;
        std::int32_t minimumWidth = 1;
        std::int32_t minimumHeight = 1;
        std::int32_t maximumWidth = 1;
        std::int32_t maximumHeight = 1;
        bool bypassesConstruction = false;
        bool uniquePerSettlement = false;
        bool separateConstructionSitePerTile = false;
        SettlementObjectVisualStyle visual;
    };

    namespace SettlementObjectTypes
    {
        inline constexpr std::string_view CityKeep = "city_keep";
        inline constexpr std::string_view Road = "road";
        inline constexpr std::string_view House = "house";
        inline constexpr std::string_view Stockpile = "stockpile";
        inline constexpr std::string_view FishingGrounds =
            "fishing_grounds";
        inline constexpr std::string_view WheatFarm = "wheat_farm";
        inline constexpr std::string_view Pastureland = "pastureland";
        inline constexpr std::string_view Bakery = "bakery";
    }

    class SettlementObjectCatalog
    {
    public:
        [[nodiscard]]
        static std::span<const SettlementObjectDefinition>
        definitions() noexcept;

        [[nodiscard]]
        static const SettlementObjectDefinition* definition(
            std::string_view objectTypeId
        ) noexcept;
    };
}

#include "world/settlements/objects/SettlementObjectDefinition.h"
#include "world/settlements/SettlementResourceDefinition.h"

#include <array>

namespace Paladin
{
    namespace
    {
        constexpr std::array<SettlementConstructionResourceCost, 2>
            initialConstructionResourceCosts{{
                {SettlementResourceTypes::Lumber, 0},
                {SettlementResourceTypes::Stone, 0}
            }};

        constexpr std::array<SettlementObjectDefinition, 8>
            objectDefinitions{{
                {
                    SettlementObjectTypes::CityKeep,
                    "City Keep",
                    SettlementObjectCategory::Rule,
                    0,
                    SettlementFootprintSelectionMode::Fixed,
                    3, 7, 3, 7,
                    true,
                    true,
                    false,
                    false,
                    SettlementObjectPlacementLayer::Structure,
                    {{82, 77, 61}, {219, 214, 194}, 3.0F, 7.0F}
                },
                {
                    SettlementObjectTypes::Road,
                    "Road",
                    SettlementObjectCategory::Roads,
                    0,
                    SettlementFootprintSelectionMode::DragRectangle,
                    1, 1, 1, 1,
                    false,
                    false,
                    true,
                    true,
                    SettlementObjectPlacementLayer::Infrastructure,
                    {{74, 28, 11}, {143, 64, 26}, 1.0F, 1.0F},
                    initialConstructionResourceCosts
                },
                {
                    SettlementObjectTypes::House,
                    "House",
                    SettlementObjectCategory::Housing,
                    0,
                    SettlementFootprintSelectionMode::Fixed,
                    3, 3, 3, 3,
                    false,
                    false,
                    false,
                    false,
                    SettlementObjectPlacementLayer::Structure,
                    {{82, 77, 61}, {219, 214, 194}, 3.0F, 3.0F},
                    initialConstructionResourceCosts
                },
                {
                    SettlementObjectTypes::Stockpile,
                    "Stockpile",
                    SettlementObjectCategory::Logistics,
                    0,
                    SettlementFootprintSelectionMode::DragRectangle,
                    1, 1, 2, 2,
                    false,
                    false,
                    false,
                    false,
                    SettlementObjectPlacementLayer::Structure,
                    {{117, 77, 31}, {209, 163, 82}, 2.0F, 2.0F},
                    initialConstructionResourceCosts
                },
                {
                    SettlementObjectTypes::FishingGrounds,
                    "Fishing Grounds",
                    SettlementObjectCategory::Food,
                    0,
                    SettlementFootprintSelectionMode::DragRectangle,
                    1, 1, 2, 2,
                    false,
                    false,
                    false,
                    false,
                    SettlementObjectPlacementLayer::Structure,
                    {{15, 87, 102}, {46, 158, 179}, 3.0F, 3.0F},
                    initialConstructionResourceCosts
                },
                {
                    SettlementObjectTypes::WheatFarm,
                    "Wheat Farm",
                    SettlementObjectCategory::Food,
                    1,
                    SettlementFootprintSelectionMode::DragRectangle,
                    1, 1, 2, 2,
                    false,
                    false,
                    false,
                    false,
                    SettlementObjectPlacementLayer::Structure,
                    {{115, 87, 15}, {214, 176, 46}, 2.0F, 2.0F},
                    initialConstructionResourceCosts
                },
                {
                    SettlementObjectTypes::Pastureland,
                    "Pastureland",
                    SettlementObjectCategory::Food,
                    2,
                    SettlementFootprintSelectionMode::DragRectangle,
                    1, 1, 2, 2,
                    false,
                    false,
                    false,
                    false,
                    SettlementObjectPlacementLayer::Structure,
                    {{56, 97, 31}, {115, 176, 71}, 4.0F, 4.0F},
                    initialConstructionResourceCosts
                },
                {
                    SettlementObjectTypes::Bakery,
                    "Bakery",
                    SettlementObjectCategory::Food,
                    3,
                    SettlementFootprintSelectionMode::DragRectangle,
                    1, 1, 2, 2,
                    false,
                    false,
                    false,
                    false,
                    SettlementObjectPlacementLayer::Structure,
                    {{74, 77, 79}, {156, 158, 163}, 3.0F, 3.0F},
                    initialConstructionResourceCosts
                }
            }};
    }


    std::span<const SettlementObjectDefinition>
    SettlementObjectCatalog::definitions() noexcept
    {
        return objectDefinitions;
    }


    const SettlementObjectDefinition*
    SettlementObjectCatalog::definition(
        std::string_view objectTypeId
    ) noexcept
    {
        for (const SettlementObjectDefinition& definition : objectDefinitions)
        {
            if (definition.id == objectTypeId)
            {
                return &definition;
            }
        }

        return nullptr;
    }
}

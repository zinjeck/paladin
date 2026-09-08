#include "world/settlements/objects/SettlementObjectDefinition.h"
#include "world/settlements/SettlementResourceDefinition.h"

#include <algorithm>
#include <array>

namespace Paladin
{
    namespace
    {
        constexpr std::array<SettlementConstructionResourceCost, 1>
            initialConstructionResourceCosts{
                {{SettlementResourceTypes::Lumber, 4}}
            };

        constexpr std::array<SettlementConstructionResourceCost, 2>
            houseConstructionResourceCosts{
                {{SettlementResourceTypes::Lumber, 16},
                 {SettlementResourceTypes::Stone, 8}}
            };

        constexpr std::array<SettlementConstructionResourceCost, 1>
            stockpileTileCosts{{{SettlementResourceTypes::Lumber, 1}}};
        constexpr std::array<SettlementConstructionResourceCost, 2>
            marketTileCosts{
                {{SettlementResourceTypes::Lumber, 1},
                 {SettlementResourceTypes::Stone, 1, 12}}
            };

        constexpr SettlementObjectDefinition enclosed(
            SettlementObjectDefinition d
        )
        {
            d.wallThickness = 1;
            d.previewWidth = std::max(3, d.previewWidth) + 2;
            d.previewHeight = std::max(3, d.previewHeight) + 2;
            d.minimumWidth = std::max(3, d.minimumWidth) + 2;
            d.minimumHeight = std::max(3, d.minimumHeight) + 2;
            d.visual.iconWidth = float(d.previewWidth);
            d.visual.iconHeight = float(d.previewHeight);
            return d;
        }
        constexpr std::array<SettlementObjectDefinition, 10>
            rawObjectDefinitions{
                {enclosed(
                     {SettlementObjectTypes::CityKeep,
                      "City Keep",
                      SettlementObjectCategory::Rule,
                      0,
                      SettlementFootprintSelectionMode::Fixed,
                      3,
                      5,
                      3,
                      5,
                      true,
                      true,
                      false,
                      false,
                      SettlementObjectPlacementLayer::Structure,
                      {{82, 77, 61}, {219, 214, 194}, 3.0F, 7.0F},
                      {},
                      true,
                      false,
                      true}
                 ),
                 {SettlementObjectTypes::Road,
                  "Road",
                  SettlementObjectCategory::Roads,
                  0,
                  SettlementFootprintSelectionMode::DragRectangle,
                  1,
                  1,
                  1,
                  1,
                  false,
                  false,
                  true,
                  true,
                  SettlementObjectPlacementLayer::Infrastructure,
                  {{74, 28, 11}, {143, 64, 26}, 1.0F, 1.0F},
                  {},
                  false},
                 enclosed(
                     {SettlementObjectTypes::House,
                      "House",
                      SettlementObjectCategory::Housing,
                      0,
                      SettlementFootprintSelectionMode::Fixed,
                      3,
                      3,
                      3,
                      3,
                      false,
                      false,
                      false,
                      false,
                      SettlementObjectPlacementLayer::Structure,
                      {{82, 77, 61}, {219, 214, 194}, 3.0F, 3.0F},
                      houseConstructionResourceCosts}
                 ),
                 {SettlementObjectTypes::Market,
                  "Market",
                  SettlementObjectCategory::Logistics,
                  -1,
                  SettlementFootprintSelectionMode::DragRectangle,
                  1,
                  1,
                  2,
                  2,
                  false,
                  false,
                  false,
                  false,
                  SettlementObjectPlacementLayer::Structure,
                  {{104, 55, 36}, {202, 126, 72}, 4.0F, 3.0F},
                  marketTileCosts,
                  false,
                  true},
                 {SettlementObjectTypes::Stockpile,
                  "Stockpile",
                  SettlementObjectCategory::Logistics,
                  0,
                  SettlementFootprintSelectionMode::DragRectangle,
                  1,
                  1,
                  2,
                  2,
                  false,
                  false,
                  false,
                  false,
                  SettlementObjectPlacementLayer::Structure,
                  {{117, 77, 31}, {209, 163, 82}, 2.0F, 2.0F},
                  stockpileTileCosts,
                  true,
                  true},
                 {SettlementObjectTypes::FishingGrounds,
                  "Fishing Grounds",
                  SettlementObjectCategory::Food,
                  0,
                  SettlementFootprintSelectionMode::DragRectangle,
                  1,
                  1,
                  2,
                  2,
                  false,
                  false,
                  false,
                  false,
                  SettlementObjectPlacementLayer::Structure,
                  {{99, 62, 75}, {189, 134, 76}, 3.0F, 3.0F},
                  initialConstructionResourceCosts},
                 {SettlementObjectTypes::WheatFarm,
                  "Wheat Farm",
                  SettlementObjectCategory::Food,
                  1,
                  SettlementFootprintSelectionMode::DragRectangle,
                  1,
                  1,
                  2,
                  2,
                  false,
                  false,
                  false,
                  false,
                  SettlementObjectPlacementLayer::Structure,
                  {{115, 87, 15}, {214, 176, 46}, 2.0F, 2.0F},
                  initialConstructionResourceCosts,
                  false},
                 {SettlementObjectTypes::Pastureland,
                  "Pastureland",
                  SettlementObjectCategory::Food,
                  2,
                  SettlementFootprintSelectionMode::DragRectangle,
                  1,
                  1,
                  2,
                  2,
                  false,
                  false,
                  false,
                  false,
                  SettlementObjectPlacementLayer::Structure,
                  {{56, 97, 31}, {115, 176, 71}, 4.0F, 4.0F},
                  stockpileTileCosts,
                  false,
                  true},
                 enclosed(
                     {SettlementObjectTypes::Bakery,
                      "Bakery",
                      SettlementObjectCategory::Food,
                      3,
                      SettlementFootprintSelectionMode::DragRectangle,
                      1,
                      1,
                      2,
                      2,
                      false,
                      false,
                      false,
                      false,
                      SettlementObjectPlacementLayer::Structure,
                      {{74, 77, 79}, {156, 158, 163}, 3.0F, 3.0F},
                      initialConstructionResourceCosts}
                 ),
                 {SettlementObjectTypes::LoggingGrounds,
                  "Logging Grounds",
                  SettlementObjectCategory::Logistics,
                  1,
                  SettlementFootprintSelectionMode::DragRectangle,
                  1,
                  1,
                  3,
                  3,
                  false,
                  false,
                  false,
                  false,
                  SettlementObjectPlacementLayer::Structure,
                  {{99, 62, 75}, {189, 134, 76}, 4.0F, 4.0F},
                  initialConstructionResourceCosts,
                  false}}
            };
        constexpr auto objectDefinitions = []
        {
            auto definitions = rawObjectDefinitions;
            for (auto& d : definitions)
            {
                if (d.selectionMode ==
                        SettlementFootprintSelectionMode::DragRectangle &&
                    d.placementLayer ==
                        SettlementObjectPlacementLayer::Structure)
                {
                    const int minimum = 3 + 2 * d.wallThickness;
                    d.minimumWidth = std::max(minimum, d.minimumWidth);
                    d.minimumHeight = std::max(minimum, d.minimumHeight);
                }
            }
            return definitions;
        }();
    } // namespace

    std::span<const SettlementObjectDefinition> SettlementObjectCatalog::
        definitions() noexcept
    {
        return objectDefinitions;
    }

    const SettlementObjectDefinition* SettlementObjectCatalog::definition(
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
} // namespace Paladin

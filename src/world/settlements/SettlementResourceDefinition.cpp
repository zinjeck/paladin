#include "world/settlements/SettlementResourceDefinition.h"

#include <array>

namespace Paladin
{
    namespace
    {
        constexpr std::array<SettlementResourceDefinition, 10>
            resourceDefinitions{
                {{SettlementResourceTypes::Coal, "Coal"},
                 {SettlementResourceTypes::Iron, "Iron"},
                 {SettlementResourceTypes::Gold, "Gold"},
                 {SettlementResourceTypes::Fish, "Fish", true},
                 {SettlementResourceTypes::Meat, "Meat", true},
                 {SettlementResourceTypes::Stone, "Stone"},
                 {SettlementResourceTypes::Lumber, "Lumber"},
                 {SettlementResourceTypes::Wheat, "Wheat"},
                 {SettlementResourceTypes::Bread, "Bread", true},
                 {SettlementResourceTypes::Rations, "Rations", true, true}}
            };
    }


    std::span<const SettlementResourceDefinition> SettlementResourceCatalog::
        definitions() noexcept
    {
        return resourceDefinitions;
    }


    const SettlementResourceDefinition* SettlementResourceCatalog::definition(
        std::string_view resourceId
    ) noexcept
    {
        for (const SettlementResourceDefinition& definition :
             resourceDefinitions)
        {
            if (definition.id == resourceId)
            {
                return &definition;
            }
        }

        return nullptr;
    }
} // namespace Paladin

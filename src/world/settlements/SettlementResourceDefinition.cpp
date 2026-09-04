#include "world/settlements/SettlementResourceDefinition.h"

#include <array>

namespace Paladin
{
    namespace
    {
        constexpr std::array<SettlementResourceDefinition, 4>
            resourceDefinitions{{
                {SettlementResourceTypes::Food, "Food"},
                {SettlementResourceTypes::Materials, "Materials"},
                {SettlementResourceTypes::Stone, "Stone"},
                {SettlementResourceTypes::Lumber, "Lumber"}
            }};
    }


    std::span<const SettlementResourceDefinition>
    SettlementResourceCatalog::definitions() noexcept
    {
        return resourceDefinitions;
    }


    const SettlementResourceDefinition*
    SettlementResourceCatalog::definition(
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
}

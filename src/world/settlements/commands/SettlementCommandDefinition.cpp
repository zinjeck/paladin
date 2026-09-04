#include "world/settlements/commands/SettlementCommandDefinition.h"

#include <array>

namespace Paladin
{
    namespace
    {
        constexpr std::array<SettlementCommandDefinition, 5> commandDefinitions{{
            {SettlementCommandTypes::ChopTree, "Chop Trees", CommandTargetKind::Tree},
            {SettlementCommandTypes::CollectRock, "Collect Rocks", CommandTargetKind::Rock},
            {SettlementCommandTypes::Gather, "Gather", CommandTargetKind::Gatherable},
            {SettlementCommandTypes::Hunt, "Hunt", CommandTargetKind::Animal},
            {SettlementCommandTypes::Demolish, "Demolish", CommandTargetKind::Object}
        }};
    }


    std::span<const SettlementCommandDefinition>
    SettlementCommandCatalog::definitions() noexcept
    {
        return commandDefinitions;
    }


    const SettlementCommandDefinition*
    SettlementCommandCatalog::definition(
        std::string_view commandTypeId
    ) noexcept
    {
        for (const SettlementCommandDefinition& definition : commandDefinitions)
        {
            if (definition.id == commandTypeId)
            {
                return &definition;
            }
        }

        return nullptr;
    }
}

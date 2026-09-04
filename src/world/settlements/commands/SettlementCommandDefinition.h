#pragma once

#include <span>
#include <string_view>

namespace Paladin
{
    enum class CommandTargetKind { Tree, Rock, Gatherable, Animal, Object };

    struct SettlementCommandDefinition
    {
        std::string_view id;
        std::string_view displayName;
        CommandTargetKind targetKind = CommandTargetKind::Tree;
    };

    namespace SettlementCommandTypes
    {
        inline constexpr std::string_view ChopTree = "chop_tree";
        inline constexpr std::string_view CollectRock = "collect_rock";
        inline constexpr std::string_view Gather = "gather";
        inline constexpr std::string_view Hunt = "hunt";
        inline constexpr std::string_view Demolish = "demolish";
        inline constexpr std::string_view Cancel = "cancel_task";
    }

    class SettlementCommandCatalog
    {
    public:
        [[nodiscard]]
        static std::span<const SettlementCommandDefinition>
        definitions() noexcept;

        [[nodiscard]]
        static const SettlementCommandDefinition* definition(
            std::string_view commandTypeId
        ) noexcept;
    };
}

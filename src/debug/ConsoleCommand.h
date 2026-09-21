#pragma once
#include <array>
#include <charconv>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
namespace Paladin
{
    enum class ConsoleCommandKind
    {
        Empty,
        Help,
        Stats,
        SpawnCitizens,
        Money,
        Invalid
    };
    struct ConsoleCommand
    {
        ConsoleCommandKind kind = ConsoleCommandKind::Empty;
        std::uint64_t count = 1;
        std::string_view error;
        std::int64_t amount = 0;
    };
    struct ConsoleCommandDefinition
    {
        std::string_view name;
        std::string_view usage;
        std::string_view description;
        ConsoleCommandKind kind;
        ConsoleCommand (*parseArguments)(std::string_view);
    };

    inline ConsoleCommand parseMoneyArguments(std::string_view argument)
    {
        if (argument.empty())
        {
            return {
                ConsoleCommandKind::Invalid,
                0,
                "Specify an amount. Usage: money <whole amount>"
            };
        }
        std::int64_t amount = 0;
        const auto [end, error] = std::from_chars(
            argument.data(),
            argument.data() + argument.size(),
            amount
        );
        if (error != std::errc{} || end != argument.data() + argument.size() ||
            amount > std::numeric_limits<std::int64_t>::max() / 100 ||
            amount < std::numeric_limits<std::int64_t>::min() / 100)
        {
            return {
                ConsoleCommandKind::Invalid,
                0,
                "Invalid amount. Use money <whole amount> within the "
                "supported cash range."
            };
        }
        ConsoleCommand command{ConsoleCommandKind::Money};
        command.amount = amount * 100;
        return command;
    }

    inline ConsoleCommand parseSpawnArguments(std::string_view argument)
    {
        std::uint64_t count = 1;
        if (!argument.empty())
        {
            const auto [end, error] = std::from_chars(
                argument.data(),
                argument.data() + argument.size(),
                count
            );
            if (error != std::errc{} ||
                end != argument.data() + argument.size() || count == 0 ||
                count > 100000)
            {
                return {
                    ConsoleCommandKind::Invalid,
                    0,
                    "Usage: spawncitizens [positive whole number, maximum "
                    "100000]"
                };
            }
        }
        return {ConsoleCommandKind::SpawnCitizens, count};
    }

    // The parser and help use this one registry. Adding a command here also
    // adds its usage to help; no separately maintained command list exists.
    inline constexpr std::array ConsoleCommands{
        ConsoleCommandDefinition{
            "help",
            "help",
            "List every console command.",
            ConsoleCommandKind::Help,
            nullptr
        },
        ConsoleCommandDefinition{
            "money",
            "money <whole amount>",
            "Set the realm treasury in gold.",
            ConsoleCommandKind::Money,
            parseMoneyArguments
        },
        ConsoleCommandDefinition{
            "spawncitizens",
            "spawncitizens [number]",
            "Spawn citizens in the active settlement; "
            "defaults to 1, maximum 100000.",
            ConsoleCommandKind::SpawnCitizens,
            parseSpawnArguments
        },
        ConsoleCommandDefinition{
            "stats",
            "stats",
            "Open live debug statistics.",
            ConsoleCommandKind::Stats,
            nullptr
        }
    };

    inline std::string consoleCommandHelp()
    {
        std::string text;
        for (const auto& command : ConsoleCommands)
        {
            text += std::string(command.usage) + " - " +
                    std::string(command.description) + "\n";
        }
        return text;
    }

    inline ConsoleCommand parseConsoleCommand(std::string_view text)
    {
        const auto trim = [](std::string_view value)
        {
            const auto first = value.find_first_not_of(" \t\r\n");
            return first == value.npos
                       ? std::string_view{}
                       : value.substr(
                             first,
                             value.find_last_not_of(" \t\r\n") - first + 1
                         );
        };
        text = trim(text);
        if (text.empty())
        {
            return {};
        }
        const auto split = text.find_first_of(" \t");
        const auto name = text.substr(0, split);
        const auto argument =
            split == text.npos ? std::string_view{} : trim(text.substr(split));
        for (const auto& definition : ConsoleCommands)
        {
            if (definition.name != name)
            {
                continue;
            }
            if (definition.parseArguments)
            {
                return definition.parseArguments(argument);
            }
            return argument.empty()
                       ? ConsoleCommand{definition.kind}
                       : ConsoleCommand{
                             ConsoleCommandKind::Invalid,
                             0,
                             "This command takes no arguments. Type help."
                         };
        }
        return {ConsoleCommandKind::Invalid, 0, "Unknown command. Type help."};
    }
} // namespace Paladin

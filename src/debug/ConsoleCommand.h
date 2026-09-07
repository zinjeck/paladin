#pragma once
#include <charconv>
#include <cstdint>
#include <limits>
#include <string_view>
namespace Paladin
{
    enum class ConsoleCommandKind
    {
        Empty,
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
    inline ConsoleCommand parseConsoleCommand(std::string_view text)
    {
        auto trim = [](std::string_view s)
        {
            auto a = s.find_first_not_of(" \t\r\n");
            if (a == s.npos)
            {
                return std::string_view{};
            }
            return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
        };
        text = trim(text);
        if (text.empty())
        {
            return {};
        }
        auto split = text.find_first_of(" \t");
        auto name = text.substr(0, split);
        auto argument =
            split == text.npos ? std::string_view{} : trim(text.substr(split));
        if (name == "stats")
        {
            return argument.empty() ? ConsoleCommand{ConsoleCommandKind::Stats}
                                    : ConsoleCommand{
                                          ConsoleCommandKind::Invalid,
                                          0,
                                          "Usage: stats"
                                      };
        }
        if (name == "money")
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
            if (error != std::errc{} ||
                end != argument.data() + argument.size() ||
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
            command.amount =
                amount * 100; // Console units are gold, storage is hundredths.
            return command;
        }
        if (name != "spawncitizens")
        {
            return {ConsoleCommandKind::Invalid, 0, "Unknown command."};
        }
        std::uint64_t count = 1;
        if (!argument.empty())
        {
            auto [end, error] = std::from_chars(
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
} // namespace Paladin

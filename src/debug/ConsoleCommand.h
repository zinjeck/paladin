#pragma once
#include <charconv>
#include <cstdint>
#include <string_view>
namespace Paladin
{
enum class ConsoleCommandKind
{
    Empty,
    Stats,
    SpawnCitizens,
    Invalid
};
struct ConsoleCommand
{
    ConsoleCommandKind kind = ConsoleCommandKind::Empty;
    std::uint64_t count = 1;
    std::string_view error;
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
        if (error != std::errc{} || end != argument.data() + argument.size() ||
            count == 0 || count > 100000)
        {
            return {
                ConsoleCommandKind::Invalid,
                0,
                "Usage: spawncitizens [positive whole number, maximum 100000]"
            };
        }
    }
    return {ConsoleCommandKind::SpawnCitizens, count};
}
} // namespace Paladin

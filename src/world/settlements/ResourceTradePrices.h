#pragma once
#include <cstdint>
#include <string_view>
namespace Paladin
{
    inline std::int64_t resourceTradeBasePrice(std::string_view resource)
    {
        return resource == "gold"      ? 1800
               : resource == "iron"    ? 180
               : resource == "coal"    ? 80
               : resource == "stone"   ? 25
               : resource == "lumber"  ? 35
               : resource == "wheat"   ? 25
               : resource == "bread"   ? 80
               : resource == "rations" ? 90
                                       : 50;
    }
} // namespace Paladin

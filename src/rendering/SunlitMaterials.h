#pragma once
#include <cstdint>
namespace Paladin
{
    // Approved Sunlight & Shadow colors only. Clear jade shade, fresh sunlit
    // green and warm stone replace the dusty intermediate material colors.
    inline std::uint32_t sunlitMaterial(std::uint32_t rgb)
    {
        switch (rgb)
        {
        case 0x79B56D:
            return 0xA6CD59;
        case 0x337A58:
            return 0x49975B;
        case 0x235747:
            return 0x337A58;
        case 0xA99478:
            return 0xD9C79F;
        case 0x95655F:
            return 0xB78350;
        default:
            return rgb;
        }
    }
} // namespace Paladin

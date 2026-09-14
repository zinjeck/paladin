#pragma once
#include <array>
#include <cstdint>
#include <utility>

namespace Paladin::UiDetail
{
    // Original 9x14 optical-size glyphs, authored for native-screen UI.
    // Compact 5x7 labels and the world raster retain their existing glyphs.
    // Compact proportional advances preserve the existing 7*pixelSize line box.
    using SerifGlyph = std::array<std::uint16_t,14>;
    inline std::pair<int,int> serifSpan(const SerifGlyph& glyph) noexcept
    {
        std::uint16_t mask=0;
        for (const auto row : glyph) mask |= row;
        if (!mask) return {0,0};
        int left=0, right=8;
        while (!(mask & (1U << (8-left)))) ++left;
        while (!(mask & (1U << (8-right)))) --right;
        return {left,right-left+1};
    }
    inline const SerifGlyph* serifGlyph(char c) noexcept
    {
        switch(c)
        {
        case 'A': { static constexpr SerifGlyph g{16,16,40,40,40,68,68,254,130,130,257,455,0,0}; return &g; }
        case 'B': { static constexpr SerifGlyph g{504,196,194,194,196,248,198,195,195,195,194,508,0,0}; return &g; }
        case 'C': { static constexpr SerifGlyph g{63,71,131,259,384,384,384,384,128,129,66,60,0,0}; return &g; }
        case 'D': { static constexpr SerifGlyph g{504,196,194,193,195,195,195,195,194,194,196,504,0,0}; return &g; }
        case 'E': { static constexpr SerifGlyph g{511,195,195,192,198,254,198,192,192,195,195,511,0,0}; return &g; }
        case 'F': { static constexpr SerifGlyph g{511,195,195,192,198,254,198,192,192,192,192,448,0,0}; return &g; }
        case 'G': { static constexpr SerifGlyph g{63,71,131,259,384,384,399,387,131,131,67,60,0,0}; return &g; }
        case 'H': { static constexpr SerifGlyph g{455,195,195,195,195,255,195,195,195,195,195,455,0,0}; return &g; }
        case 'I': { static constexpr SerifGlyph g{254,24,24,24,24,24,24,24,24,24,24,254,0,0}; return &g; }
        case 'J': { static constexpr SerifGlyph g{63,6,6,6,6,6,6,6,262,262,136,112,0,0}; return &g; }
        case 'K': { static constexpr SerifGlyph g{455,196,200,208,224,208,208,200,200,196,196,455,0,0}; return &g; }
        case 'L': { static constexpr SerifGlyph g{448,192,192,192,192,192,192,192,192,195,195,511,0,0}; return &g; }
        case 'M': { static constexpr SerifGlyph g{455,195,199,199,235,235,211,211,195,195,195,455,0,0}; return &g; }
        case 'N': { static constexpr SerifGlyph g{455,195,195,227,227,211,211,203,203,199,199,455,0,0}; return &g; }
        case 'O': { static constexpr SerifGlyph g{56,68,130,129,387,387,387,387,258,130,68,56,0,0}; return &g; }
        case 'P': { static constexpr SerifGlyph g{504,196,195,195,195,196,248,192,192,192,192,448,0,0}; return &g; }
        case 'Q': { static constexpr SerifGlyph g{56,68,130,129,387,387,387,387,274,138,68,60,2,1}; return &g; }
        case 'R': { static constexpr SerifGlyph g{504,196,195,195,195,196,248,200,200,196,196,455,0,0}; return &g; }
        case 'S': { static constexpr SerifGlyph g{127,135,259,259,128,96,28,2,385,385,386,508,0,0}; return &g; }
        case 'T': { static constexpr SerifGlyph g{511,411,411,24,24,24,24,24,24,24,24,124,0,0}; return &g; }
        case 'U': { static constexpr SerifGlyph g{455,195,195,195,195,195,195,195,195,66,76,48,0,0}; return &g; }
        case 'V': { static constexpr SerifGlyph g{455,130,68,68,68,68,40,40,40,40,16,16,0,0}; return &g; }
        case 'W': { static constexpr SerifGlyph g{455,130,130,130,130,146,84,108,108,108,68,68,0,0}; return &g; }
        case 'X': { static constexpr SerifGlyph g{455,68,68,40,40,16,16,40,40,68,68,455,0,0}; return &g; }
        case 'Y': { static constexpr SerifGlyph g{455,68,68,40,40,24,24,24,24,24,24,124,0,0}; return &g; }
        case 'Z': { static constexpr SerifGlyph g{511,386,386,4,8,16,16,32,64,131,131,511,0,0}; return &g; }
        case 'a': { static constexpr SerifGlyph g{0,0,0,0,56,68,3,63,67,131,135,123,0,0}; return &g; }
        case 'b': { static constexpr SerifGlyph g{448,192,192,192,248,196,195,195,195,195,196,504,0,0}; return &g; }
        case 'c': { static constexpr SerifGlyph g{0,0,0,0,60,70,192,192,192,192,66,60,0,0}; return &g; }
        case 'd': { static constexpr SerifGlyph g{7,3,3,3,59,71,195,195,195,195,71,63,0,0}; return &g; }
        case 'e': { static constexpr SerifGlyph g{0,0,0,0,56,68,194,254,192,192,66,60,0,0}; return &g; }
        case 'f': { static constexpr SerifGlyph g{56,68,96,96,504,96,96,96,96,96,96,504,0,0}; return &g; }
        case 'g': { static constexpr SerifGlyph g{0,0,0,0,63,67,195,195,195,71,59,3,132,120}; return &g; }
        case 'h': { static constexpr SerifGlyph g{448,192,192,192,248,196,195,195,195,195,195,455,0,0}; return &g; }
        case 'i': { static constexpr SerifGlyph g{0,16,0,0,56,24,24,24,24,24,24,124,0,0}; return &g; }
        case 'j': { static constexpr SerifGlyph g{0,8,0,0,60,12,12,12,12,12,12,12,272,224}; return &g; }
        case 'k': { static constexpr SerifGlyph g{448,192,192,192,199,196,200,240,200,196,196,455,0,0}; return &g; }
        case 'l': { static constexpr SerifGlyph g{120,24,24,24,24,24,24,24,24,24,24,12,0,0}; return &g; }
        case 'm': { static constexpr SerifGlyph g{0,0,0,0,492,218,219,219,219,219,219,511,0,0}; return &g; }
        case 'n': { static constexpr SerifGlyph g{0,0,0,0,504,196,195,195,195,195,195,455,0,0}; return &g; }
        case 'o': { static constexpr SerifGlyph g{0,0,0,0,56,68,195,195,195,195,68,56,0,0}; return &g; }
        case 'p': { static constexpr SerifGlyph g{0,0,0,0,504,196,195,195,195,196,248,192,192,448}; return &g; }
        case 'q': { static constexpr SerifGlyph g{0,0,0,0,63,71,195,195,195,71,59,3,3,7}; return &g; }
        case 'r': { static constexpr SerifGlyph g{0,0,0,0,504,196,192,192,192,192,192,448,0,0}; return &g; }
        case 's': { static constexpr SerifGlyph g{0,0,0,0,124,130,128,96,24,6,130,124,0,0}; return &g; }
        case 't': { static constexpr SerifGlyph g{0,48,48,48,508,48,48,48,48,48,18,28,0,0}; return &g; }
        case 'u': { static constexpr SerifGlyph g{0,0,0,0,455,195,195,195,195,195,71,59,0,0}; return &g; }
        case 'v': { static constexpr SerifGlyph g{0,0,0,0,455,130,68,68,40,40,16,16,0,0}; return &g; }
        case 'w': { static constexpr SerifGlyph g{0,0,0,0,455,130,130,146,92,108,100,68,0,0}; return &g; }
        case 'x': { static constexpr SerifGlyph g{0,0,0,0,455,68,40,16,16,40,68,455,0,0}; return &g; }
        case 'y': { static constexpr SerifGlyph g{0,0,0,0,455,68,68,40,40,16,16,32,32,448}; return &g; }
        case 'z': { static constexpr SerifGlyph g{0,0,0,0,254,132,8,16,16,32,66,254,0,0}; return &g; }
        case '0': { static constexpr SerifGlyph g{56,68,195,195,195,211,211,195,195,195,68,56,0,0}; return &g; }
        case '1': { static constexpr SerifGlyph g{24,56,56,88,24,24,24,24,24,24,24,254,0,0}; return &g; }
        case '2': { static constexpr SerifGlyph g{56,68,131,3,3,4,8,16,16,35,67,255,0,0}; return &g; }
        case '3': { static constexpr SerifGlyph g{120,132,2,2,4,56,4,3,3,3,196,56,0,0}; return &g; }
        case '4': { static constexpr SerifGlyph g{6,14,22,22,38,70,134,134,511,6,6,31,0,0}; return &g; }
        case '5': { static constexpr SerifGlyph g{254,192,192,192,192,248,4,3,3,3,196,56,0,0}; return &g; }
        case '6': { static constexpr SerifGlyph g{60,66,64,192,192,248,196,195,195,195,68,56,0,0}; return &g; }
        case '7': { static constexpr SerifGlyph g{255,194,194,4,4,8,8,16,16,32,32,112,0,0}; return &g; }
        case '8': { static constexpr SerifGlyph g{56,68,130,130,68,56,68,195,195,195,68,56,0,0}; return &g; }
        case '9': { static constexpr SerifGlyph g{56,68,195,195,195,71,59,3,3,4,196,56,0,0}; return &g; }
        default: return nullptr;
        }
    }
}

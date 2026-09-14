#include "ui/BitmapFontRenderer.h"
#include "ui/UiSerifGlyphs.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Paladin
{
    namespace
    {
        using GlyphRows = std::array<std::uint8_t, 7>;

        GlyphRows glyphRows(char character) noexcept
        {
            switch (character)
            {
            // Open counters and modest terminals: everyday medieval lettering,
            // not blackletter. Lowercase remains distinct from capitals.
            case 'a': return {0,0,14,1,15,17,15};
            case 'b': return {24,8,14,9,9,9,30};
            case 'c': return {0,0,15,16,16,16,15};
            case 'd': return {3,2,14,18,18,18,15};
            case 'e': return {0,0,14,17,31,16,15};
            case 'f': return {6,9,8,28,8,8,28};
            case 'g': return {0,14,17,17,15,1,14};
            case 'h': return {24,8,14,9,9,9,27};
            case 'i': return {4,0,12,4,4,4,14};
            case 'j': return {2,0,6,2,2,18,12};
            case 'k': return {24,8,9,10,12,10,27};
            case 'l': return {12,4,4,4,4,4,14};
            case 'm': return {0,0,26,21,21,21,21};
            case 'n': return {0,0,30,9,9,9,27};
            case 'o': return {0,0,14,17,17,17,14};
            case 'p': return {0,30,9,9,14,8,28};
            case 'q': return {0,15,18,18,14,2,7};
            case 'r': return {0,0,22,9,8,8,28};
            case 's': return {0,0,15,16,14,1,30};
            case 't': return {8,8,28,8,8,9,6};
            case 'u': return {0,0,18,18,18,18,15};
            case 'v': return {0,0,17,17,17,10,4};
            case 'w': return {0,0,17,17,21,21,10};
            case 'x': return {0,0,17,10,4,10,17};
            case 'y': return {0,17,17,17,15,1,14};
            case 'z': return {0,0,31,2,4,8,31};
            case 'A':
                return {14, 17, 17, 31, 17, 17, 17};
            case 'B':
                return {30, 17, 17, 30, 17, 17, 30};
            case 'C':
                return {15, 16, 16, 16, 16, 16, 15};
            case 'D':
                return {30, 17, 17, 17, 17, 17, 30};
            case 'E':
                return {31, 16, 16, 30, 16, 16, 31};
            case 'F':
                return {31, 16, 16, 30, 16, 16, 16};
            case 'G':
                return {15, 16, 16, 19, 17, 17, 15};
            case 'H':
                return {17, 17, 17, 31, 17, 17, 17};
            case 'I':
                return {31, 4, 4, 4, 4, 4, 31};
            case 'J':
                return {7, 2, 2, 2, 18, 18, 12};
            case 'K':
                return {17, 18, 20, 24, 20, 18, 17};
            case 'L':
                return {16, 16, 16, 16, 16, 16, 31};
            case 'M':
                return {17, 27, 21, 21, 17, 17, 17};
            case 'N':
                return {17, 25, 21, 19, 17, 17, 17};
            case 'O':
                return {14, 17, 17, 17, 17, 17, 14};
            case 'P':
                return {30, 17, 17, 30, 16, 16, 16};
            case 'Q':
                return {14, 17, 17, 17, 21, 18, 13};
            case 'R':
                return {30, 17, 17, 30, 20, 18, 17};
            case 'S':
                return {15, 16, 16, 14, 1, 1, 30};
            case 'T':
                return {31, 21, 4, 4, 4, 4, 14};
            case 'U':
                return {17, 17, 17, 17, 17, 17, 14};
            case 'V':
                return {17, 17, 17, 17, 17, 10, 4};
            case 'W':
                return {17, 17, 17, 21, 21, 21, 10};
            case 'X':
                return {17, 17, 10, 4, 10, 17, 17};
            case 'Y':
                return {17, 17, 10, 4, 4, 4, 4};
            case 'Z':
                return {31, 1, 2, 4, 8, 16, 31};

            case '0':
                return {14, 17, 19, 21, 25, 17, 14};
            case '1':
                return {4, 12, 4, 4, 4, 4, 14};
            case '2':
                return {14, 17, 1, 2, 4, 8, 31};
            case '3':
                return {30, 1, 1, 14, 1, 1, 30};
            case '4':
                return {2, 6, 10, 18, 31, 2, 2};
            case '5':
                return {31, 16, 16, 30, 1, 1, 30};
            case '6':
                return {14, 16, 16, 30, 17, 17, 14};
            case '7':
                return {31, 1, 2, 4, 8, 8, 8};
            case '8':
                return {14, 17, 17, 14, 17, 17, 14};
            case '9':
                return {14, 17, 17, 15, 1, 1, 14};

            case '<':
                return {2, 4, 8, 16, 8, 4, 2};
            case '!':
                return {4, 4, 4, 4, 4, 0, 4};
            case '^':
                return {4, 10, 17, 0, 0, 0, 0};
            case ',':
                return {0, 0, 0, 0, 0, 4, 8};
            case ';':
                return {0, 4, 4, 0, 0, 4, 8};
            case '>':
                return {8, 4, 2, 1, 2, 4, 8};
            case '%':
                return {25, 25, 2, 4, 8, 19, 19};
            case '|':
                return {4, 4, 4, 4, 4, 4, 4};
            case '(':
                return {2, 4, 8, 8, 8, 4, 2};
            case ')':
                return {8, 4, 2, 2, 2, 4, 8};
            case ':':
                return {0, 4, 4, 0, 4, 4, 0};
            case '-':
                return {0, 0, 0, 31, 0, 0, 0};
            case '+':
                return {0, 4, 4, 31, 4, 4, 0};
            case '.':
                return {0, 0, 0, 0, 0, 12, 12};
            case '/':
                return {1, 1, 2, 4, 8, 16, 16};
            case '\'':
                return {4, 4, 2, 0, 0, 0, 0};
            case ' ':
                return {0, 0, 0, 0, 0, 0, 0};
            default:
                return {14, 17, 1, 2, 4, 0, 4};
            }
        }
    } // namespace

    float BitmapFontRenderer::measureWidth(
        std::string_view text,
        float pixelSize
    ) const noexcept
    {
        if (text.empty())
        {
            return 0.0F;
        }

        if (pixelSize < 2.F)
            return (static_cast<float>(text.size()) * 6.0F - 1.0F) * pixelSize;
        float width=0;
        for (const char c : text)
        {
            if (const auto* serif=UiDetail::serifGlyph(c))
                width += (UiDetail::serifSpan(*serif).second + 2) * pixelSize * .5F;
            else width += (c==' ' ? 3.F : 6.F) * pixelSize;
        }
        return width-pixelSize;
    }

    void BitmapFontRenderer::drawText(
        Renderer& renderer,
        std::string_view text,
        float x,
        float y,
        float pixelSize,
        RenderColor color
    ) const
    {
        float cursorX = x;
        std::vector<RenderRectangle> rectangles;
        rectangles.reserve(text.size() * 18U);

        for (const char character : text)
        {
            const GlyphRows rows = glyphRows(character);
            const auto* serif = pixelSize >= 2.F && renderer.currentPixelPitch() <= 1.0
                ? UiDetail::serifGlyph(character) : nullptr;
            const std::size_t rowCount = serif ? serif->size() : rows.size();
            const std::size_t columnCount = serif ? 9 : 5;
            const float stroke = serif ? pixelSize * .5F : pixelSize;
            const auto span = serif ? UiDetail::serifSpan(*serif) : std::pair<int,int>{0,5};

            for (std::size_t row = 0; row < rowCount; ++row)
            {
                for (std::size_t column = 0; column < columnCount; ++column)
                {
                    const std::uint16_t bit = std::uint16_t(1U << (columnCount - 1 - column));

                    if (((serif ? (*serif)[row] : rows[row]) & bit) == 0)
                    {
                        continue;
                    }

                    // Shared rounded edges avoid gaps between adjacent
                    // bitmap pixels when a label uses a fractional scale.
                    const float left = std::round(
                        cursorX + (static_cast<float>(column)-span.first) * stroke
                    );
                    const float top =
                        std::round(y + static_cast<float>(row) * stroke);
                    const float right = std::round(
                        cursorX + (static_cast<float>(column + 1)-span.first) * stroke
                    );
                    const float bottom =
                        std::round(y + static_cast<float>(row + 1) * stroke);
                    rectangles.push_back(
                        {left, top, right - left, bottom - top}
                    );
                }
            }

            cursorX += serif ? (span.second+2)*stroke
                : character==' ' && pixelSize>=2.F ? 3.F*pixelSize : 6.F*pixelSize;
        }

        renderer.fillRectangles(rectangles, color);
    }
} // namespace Paladin

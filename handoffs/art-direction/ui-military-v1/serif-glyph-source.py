"""Regenerate the original native UI optical-size glyph header. Python + Pillow."""
from pathlib import Path
from PIL import Image, ImageDraw
root=Path(__file__).resolve().parents[3]
glyphs={}
def glyph(ch, lines):
 im=Image.new('1',(9,14));d=ImageDraw.Draw(im)
 for line in lines:
  if len(line)==2:d.point(line,fill=1)
  else:
   d.line(line,fill=1)
   # Optical weight: broad stems, finer joins and restrained horizontal serifs.
   for i in range(0,len(line)-2,2):
    x,y,xx,yy=line[i:i+4]
    if x==xx and abs(yy-y)>1:
     d.line((x+1 if x<8 else x-1,y,xx+1 if xx<8 else xx-1,yy),fill=1)
 glyphs[ch]=[sum(im.getpixel((x,y))<<(8-x) for x in range(9)) for y in range(14)]
def stem(x,y=0,end=11): return [(x,y,x,end),(x-1,y,x+1,y),(x-1,end,x+1,end)]
glyph('A',[(0,11,4,0,8,11),(2,7,6,7),(0,11,2,11),(6,11,8,11)])
glyph('B',stem(1)+[(1,0,5,0,7,2,7,3,5,5,1,5),(5,5,8,7,8,9,6,11,1,11)])
glyph('C',[(7,2,6,0,3,0,1,2,0,4,0,7,1,9,3,11,6,11,8,9),(7,0,7,3)])
glyph('D',stem(1)+[(1,0,5,0,7,2,8,4,8,7,7,9,5,11,1,11)])
glyph('E',stem(1)+[(1,0,7,0,7,2),(1,5,6,5),(6,4,6,6),(1,11,7,11,7,9)])
glyph('F',stem(1)+[(1,0,7,0,7,2),(1,5,6,5),(6,4,6,6)])
glyph('G',[(7,2,6,0,3,0,1,2,0,4,0,7,1,9,3,11,6,11,7,10,7,6),(5,6,8,6),(7,0,7,3)])
glyph('H',stem(1)+stem(7)+[(1,5,7,5)])
glyph('I',[(4,0,4,11),(1,0,7,0),(1,11,7,11)])
glyph('J',[(3,0,8,0),(6,0,6,9,4,11,2,11,0,9),(0,8,0,9)])
glyph('K',stem(1)+[(6,0,8,0),(7,0,1,6),(3,4,7,11),(6,11,8,11)])
glyph('L',stem(1)+[(1,11,7,11,7,9)])
glyph('M',stem(1)+stem(7)+[(1,0,4,7,7,0)])
glyph('N',stem(1)+stem(7)+[(1,0,7,11)])
glyph('O',[(3,0,5,0,7,2,8,4,8,7,7,9,5,11,3,11,1,9,0,7,0,4,1,2,3,0)])
glyph('P',stem(1)+[(1,0,5,0,7,2,7,4,5,6,1,6)])
glyph('Q',[(3,0,5,0,7,2,8,4,8,7,7,9,5,11,3,11,1,9,0,7,0,4,1,2,3,0),(4,8,8,13)])
glyph('R',stem(1)+[(1,0,5,0,7,2,7,4,5,6,1,6),(4,6,7,11),(6,11,8,11)])
glyph('S',[(7,2,6,0,2,0,0,2,0,3,2,5,6,6,8,8,8,9,6,11,2,11,0,9),(7,0,7,3),(0,8,0,11)])
glyph('T',[(0,2,0,0,8,0,8,2),(4,0,4,11),(2,11,6,11)])
glyph('U',[(0,0,2,0),(6,0,8,0),(1,0,1,8,2,10,4,11,6,10,7,8,7,0)])
glyph('V',[(0,0,2,0),(6,0,8,0),(1,0,4,11,7,0)])
glyph('W',[(0,0,2,0),(6,0,8,0),(1,0,2,11,4,5,6,11,7,0)])
glyph('X',[(0,0,2,0),(6,0,8,0),(1,0,7,11),(7,0,1,11),(0,11,2,11),(6,11,8,11)])
glyph('Y',[(0,0,2,0),(6,0,8,0),(1,0,4,5,7,0),(4,5,4,11),(2,11,6,11)])
glyph('Z',[(0,2,0,0,8,0,0,11,8,11,8,9)])
glyph('a',[(2,5,3,4,5,4,7,6,7,11,8,11),(7,7,3,7,1,9,1,10,2,11,5,11,7,9)])
glyph('b',stem(1)+[(1,6,3,4,5,4,7,6,7,9,5,11,3,11,1,10)])
glyph('c',[(7,5,6,4,3,4,1,6,1,9,3,11,6,11,7,10),(6,4,6,5)])
glyph('d',stem(7)+[(7,6,5,4,3,4,1,6,1,9,3,11,5,11,7,9)])
glyph('e',[(1,7,7,7,7,6,5,4,3,4,1,6,1,9,3,11,6,11,7,10)])
glyph('f',[(6,1,5,0,3,0,2,2,2,11),(0,4,5,4),(0,11,5,11)])
glyph('g',[(7,4,3,4,1,6,1,8,3,10,5,10,7,8),(7,4,7,11,5,13,2,13,1,12)])
glyph('h',stem(1)+[(1,6,3,4,5,4,7,6,7,11),(6,11,8,11)])
glyph('i',[(4,1),(3,4,4,4,4,11),(2,11,6,11)])
glyph('j',[(5,1),(3,4,5,4,5,11,3,13,1,13,0,12)])
glyph('k',stem(1)+[(6,4,8,4),(7,4,1,9),(4,7,7,11),(6,11,8,11)])
glyph('l',[(2,0,4,0,4,10,5,11,6,11)])
glyph('m',[(0,4,1,4,1,11),(1,6,2,4,3,4,4,6,4,11),(4,6,5,4,6,4,7,6,7,11),(0,11,2,11),(3,11,5,11),(6,11,8,11)])
glyph('n',stem(1,4)+[(1,6,3,4,5,4,7,6,7,11),(6,11,8,11)])
glyph('o',[(3,4,5,4,7,6,7,9,5,11,3,11,1,9,1,6,3,4)])
glyph('p',stem(1,4,13)+[(1,6,3,4,5,4,7,6,7,8,5,10,3,10,1,9)])
glyph('q',stem(7,4,13)+[(7,6,5,4,3,4,1,6,1,8,3,10,5,10,7,9)])
glyph('r',stem(1,4)+[(1,6,3,4,5,4,6,5)])
glyph('s',[(7,5,6,4,2,4,1,5,1,6,3,7,5,8,7,9,7,10,6,11,2,11,1,10)])
glyph('t',[(3,1,3,9,4,11,6,11,7,10),(0,4,6,4)])
glyph('u',[(0,4,1,4,1,9,3,11,5,11,7,9),(6,4,7,4,7,11,8,11)])
glyph('v',[(0,4,2,4),(6,4,8,4),(1,4,4,11,7,4)])
glyph('w',[(0,4,2,4),(6,4,8,4),(1,4,2,11,4,7,6,11,7,4)])
glyph('x',[(0,4,2,4),(6,4,8,4),(1,4,7,11),(7,4,1,11),(0,11,2,11),(6,11,8,11)])
glyph('y',[(0,4,2,4),(6,4,8,4),(1,4,4,9),(7,4,3,12,2,13,0,13)])
glyph('z',[(1,5,1,4,7,4,1,11,7,11,7,10)])
glyph('0',[(3,0,5,0,7,2,7,9,5,11,3,11,1,9,1,2,3,0),(4,5,4,6)])
glyph('1',[(2,3,4,0,4,11),(1,11,7,11)])
glyph('2',[(1,2,3,0,5,0,7,2,7,4,1,11,7,11,7,9)])
glyph('3',[(1,1,3,0,5,0,7,2,7,3,5,5,3,5),(5,5,7,7,7,9,5,11,3,11,1,10)])
glyph('4',[(6,11,6,0,0,8,8,8),(4,11,8,11)])
glyph('5',[(7,0,1,0,1,5,5,5,7,7,7,9,5,11,3,11,1,10)])
glyph('6',[(7,1,5,0,3,0,1,3,1,9,3,11,5,11,7,9,7,7,5,5,3,5,1,7)])
glyph('7',[(1,2,1,0,8,0,4,7,3,11),(2,11,4,11)])
glyph('8',[(3,0,5,0,7,2,7,3,5,5,3,5,1,3,1,2,3,0),(3,5,1,7,1,9,3,11,5,11,7,9,7,7,5,5)])
glyph('9',[(7,4,5,6,3,6,1,4,1,2,3,0,5,0,7,2,7,8,5,11,3,11,1,10)])
header='''#pragma once
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
'''
for ch,rows in glyphs.items():header+=f"        case '{ch}': {{ static constexpr SerifGlyph g{{{','.join(map(str,rows))}}}; return &g; }}\n"
header+='''        default: return nullptr;
        }
    }
}
'''
(root/'src/ui/UiSerifGlyphs.h').write_text(header)

#pragma once
#include "rendering/Renderer.h"
#include "ui/UiTypes.h"
#include <algorithm>
#include <cmath>
namespace Paladin
{
    // Native-screen chrome. All authored colors are in art-palette.hex;
    // neither frame thickness nor ornament inherits the world camera zoom.
    inline void paladinFrame(Renderer& r, UiRectangle b, RenderColor body,
                             bool active = false, bool inset = false)
    {
        b.x = std::round(b.x); b.y = std::round(b.y);
        b.width = std::round(b.width); b.height = std::round(b.height);
        const float edge = b.height < 28 || b.width < 36 ? 1.F : 2.F;
        const auto fill = [&](float x, float y, float w, float h, RenderColor c)
        { if (w > 0 && h > 0) r.fillRectangle(x,y,w,h,c); };
        fill(b.x,b.y,b.width,b.height,{8,15,27,255});
        fill(b.x+edge,b.y+edge,b.width-2*edge,b.height-2*edge,{57,70,88,255});
        fill(b.x+2*edge,b.y+2*edge,b.width-4*edge,b.height-4*edge,body);
        const RenderColor light = active ? RenderColor{213,164,84,255}
            : RenderColor{154,167,175,255};
        const RenderColor dark{53,56,62,255};
        fill(b.x+3*edge,b.y+edge,b.width-6*edge,edge,inset?dark:light);
        fill(b.x+edge,b.y+3*edge,edge,b.height-6*edge,inset?dark:light);
        fill(b.x+2*edge,b.y+b.height-2*edge,b.width-4*edge,edge,inset?light:dark);
        fill(b.x+b.width-2*edge,b.y+2*edge,edge,b.height-4*edge,inset?light:dark);
        if (b.width >= 64 && b.height >= 32)
            for (int corner=0;corner<4;++corner)
            {
                const float x = corner%2 ? b.x+b.width-4*edge : b.x+2*edge;
                const float y = corner/2 ? b.y+b.height-4*edge : b.y+2*edge;
                fill(x,y,2*edge,edge,{179,122,62,255});
                fill(x,y,edge,2*edge,{179,122,62,255});
                fill(x,y,edge,edge,{235,196,107,255});
            }
    }
}

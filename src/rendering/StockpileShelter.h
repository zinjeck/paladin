#pragma once
#include "rendering/SceneSpriteLibrary.h"
#include "world/settlements/objects/SettlementObjectState.h"

namespace Paladin
{
    struct StockpileShelterLayout
    {
        double left, back, width, depth, postHeight;
        static constexpr double pixel = 1.0 / 16;
        static constexpr double overhang = 3 * pixel;
        double front() const { return back + depth; }
        double right() const { return left + width; }
        double platformTop() const { return front() - .1875; }
        double eave() const { return front() - postHeight; }
        double roofTop() const { return back - postHeight - overhang; }
        double roofBottom() const { return eave() + overhang; }
        double roofHeight() const { return roofBottom() - roofTop(); }
        static StockpileShelterLayout fit(const SettlementObjectFootprint& f)
        {
            const auto snap = [](double n) { return std::round(n * 16) / 16; };
            const double depth = snap(std::min(2.25, std::max(.875, f.height * .40)));
            return {f.topLeft.x + .375, f.topLeft.y + .3125,
                snap(std::max(.875, std::min(4.5, f.width - .75))), depth,
                snap(std::min(1.125, depth * .65 + .25))};
        }
    };

    inline void stockpileShelter(SceneDrawQueue& q, const SceneProjection& p,
        const SceneSpriteLibrary& sprites, const StockpileShelterLayout& s, std::uint64_t id)
    {
        constexpr double pixel = 1.0 / 16;
        const RenderColor dark{73,53,47,255}, wood{122,80,56,255},
            light{193,139,90,255}, cap{215,200,162,255};
        const double front = s.front(), right = s.right();
        const auto rect = [&](double x, double y, double w, double h,
                              RenderColor color, int order)
        {
            const auto bounds = p.bounds({x,y,0,w,h,0,0});
            if (w > 0 && h > 0 && p.visible(bounds))
            { q.submit({bounds,color,front,id,0,order}); }
        };
        const auto placed = [&](const std::string& name, double x, double y,
                                double w, double h, int order)
        {
            if (const auto* sprite = sprites.find(name))
            {
                // Convert an explicit top-left rectangle to the asset's pivot.
                // Roof metadata elevation must not move it away from its posts.
                sprites.placed(q,p,name,x+w*sprite->pivotX,
                    y+sprite->elevation+h*sprite->pivotY,front,id,order,w,h);
            }
        };
        if (sprites.shadowsEnabled())
        {
            rect(s.left-.125, front-.0625, s.width+.375, .3125, {57,43,60,85}, 0);
            rect(s.left, s.back, s.width, s.depth, {57,43,60,65}, 0);
        }
        // Side foundation and platform fascia share the same ground contact.
        // The lit lip belongs above the dark face, not on a second flat floor.
        rect(s.left-.125,s.back,s.width+.25,s.depth,dark,1);
        rect(s.left-.0625,s.back,s.width+.125,s.depth-.125,wood,2);
        rect(s.left-.125,s.back,.0625,s.depth,light,3);
        // Raised loading platform has a top, a dark fascia and a worn lip.
        rect(s.left-.125,s.platformTop(),s.width+.25,.3125,dark,1);
        rect(s.left-.125,s.platformTop(),s.width+.25,.125,wood,2);
        rect(s.left-.0625,s.platformTop(),s.width+.125,pixel,light,3);
        rect(s.left+.3125,front+.125,std::min(1.0,s.width-.5),.125,wood,3);
        rect(s.left+.3125,front+.125,std::min(1.0,s.width-.5),pixel,light,4);
        // Rear storage wall and side plane remain under the roof, never flat on
        // the grass. Distinct lit and shaded faces establish the shed's volume.
        // The loading shed is open: its back is recessed into shadow, rather
        // than a flat plaster panel drawn over the front posts.
        rect(s.left,s.eave()+.125,s.width,s.postHeight-.3125,dark,4);
        rect(s.left+.1875,s.eave()+.1875,s.width-.375,
             std::max(pixel,s.postHeight-.4375),{78,59,57,255},5);
        rect(s.left+.1875,s.platformTop()-.25,s.width-.375,pixel,wood,7);
        rect(s.left+.1875,s.platformTop()-.1875,s.width-.375,pixel,{116,81,63,255},8);
        rect(right-.1875,s.back-s.postHeight,.1875,s.depth+s.postHeight-.125,dark,6);
        rect(right-.1875,s.back-s.postHeight,pixel,s.depth+s.postHeight-.125,wood,7);
        rect(s.left,s.eave()+.125,.1875,s.postHeight-.25,wood,7);
        rect(s.left,s.eave()+.125,pixel,s.postHeight-.25,light,8);
        // Joist ends and a lower fascia establish the platform's thickness.
        for (int i=0; i<int(s.width*2); ++i)
        {
            const double x = s.left + i*.5;
            rect(x,front-.0625,pixel,.1875,{116,81,63,255},8);
            rect(x+pixel,front+.0625,pixel,pixel,dark,9);
        }
        // A stored crate sits on the platform rather than hovering below it.
        placed("stockpile.crate",s.left+.375,front-.625,.625,.4375,9);
        if (s.width >= 2.5)
        { placed("stockpile.crate",s.left+1.125,front-.75,.625,.5625,10); }
        const int bays = std::max(1,int(std::ceil(s.width/2.25)));
        for (int i=0; i<=bays; ++i)
        {
            const double x = s.left + std::round((s.width-.1875)*i/bays*16)/16;
            rect(x-.0625,front-.125,.3125,.1875,dark,11);
            rect(x,s.eave(),.1875,s.postHeight,wood,12);
            rect(x,s.eave(),pixel,s.postHeight,light,13);
            rect(x+.125,s.eave(),pixel,s.postHeight,dark,13);
            rect(x-.0625,front-.125,.3125,pixel,cap,14);
            for (int k=0;k<4;++k)
            {
                if (i<bays) rect(x+.1875+k*pixel,s.eave()+(4-k)*pixel,pixel,.125,wood,14);
                if (i>0) rect(x-(k+1)*pixel,s.eave()+(4-k)*pixel,pixel,.125,wood,14);
            }
        }
        // All faces use one depth anchor. The eaves meet the header supported
        // by the posts; no independently offset floating roof plane remains.
        placed("roof.thatch.full",s.left-s.overhang,s.roofTop(),
               s.width+2*s.overhang,s.roofHeight(),20);
        rect(s.left-.0625,s.eave()+.0625,s.width+.125,.1875,dark,21);
        rect(s.left-.0625,s.eave()+.0625,s.width+.125,pixel,light,22);
        rect(s.left,s.eave()+.125,s.width,pixel,wood,23);
    }
}

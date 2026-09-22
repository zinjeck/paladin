#pragma once
#include "rendering/SceneSpriteLibrary.h"
#include "world/settlements/objects/SettlementObjectState.h"

namespace Paladin
{
    struct StockpileShelterLayout
    {
        double left, back, width, depth, postHeight;
        double front() const { return back + depth; }
        double eave() const { return front() - postHeight; }
        double roofTop() const { return back - postHeight - .1875; }
        double roofHeight() const { return depth + .375; }
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
        const double front = s.front(), right = s.left + s.width;
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
        // Raised loading platform has a top, a dark fascia and a worn lip.
        rect(s.left-.125,front-.1875,s.width+.25,.3125,dark,1);
        rect(s.left-.125,front-.1875,s.width+.25,.125,wood,2);
        rect(s.left-.0625,front-.1875,s.width+.125,pixel,light,3);
        rect(s.left+.3125,front+.125,std::min(1.0,s.width-.5),.125,wood,3);
        // Rear storage wall and side plane remain under the roof, never flat on
        // the grass. Distinct lit and shaded faces establish the shed's volume.
        placed("wall.adobe.front",s.left,s.eave()+.125,s.width,
               std::max(.25,s.postHeight-.3125),4);
        rect(s.left,s.eave()+.125,s.width,s.postHeight-.3125,{57,43,60,72},5);
        rect(right-.1875,s.back-s.postHeight,.1875,s.depth+s.postHeight-.125,dark,6);
        rect(right-.1875,s.back-s.postHeight,pixel,s.depth+s.postHeight-.125,wood,7);
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
        placed("roof.thatch.full",s.left-.1875,s.roofTop(),
               s.width+.375,s.roofHeight(),20);
        rect(s.left-.0625,s.eave()+.0625,s.width+.125,.1875,dark,21);
        rect(s.left-.0625,s.eave()+.0625,s.width+.125,pixel,light,22);
        rect(s.left,s.eave()+.125,s.width,pixel,wood,23);
    }
}

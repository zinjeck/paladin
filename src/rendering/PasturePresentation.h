#pragma once
#include "rendering/SceneSpriteLibrary.h"
#include "world/settlements/objects/SettlementObjectState.h"
namespace Paladin {
inline void pastureFence(SceneDrawQueue& q, const SceneProjection& p,
    const SceneSpriteLibrary& sprites, const SettlementObjectFootprint& f, std::uint64_t id)
{
    const double x = f.topLeft.x, y = f.topLeft.y;
    const auto piece = [&](double xx, double yy, bool vertical) {
        const auto bounds = p.bounds({xx, yy, 0, vertical ? .16 : 1., vertical ? 1.35 : .42, 0, 1});
        if (!p.visible(bounds)) return;
        if (!vertical && sprites.placed(q,p,"pasture.fence",xx+.5,yy,yy,id,1,1.08,.42)) return;
        const auto rect = [&](double rx,double ry,double w,double h, RenderColor color,int part) {
            q.submit({p.bounds({rx,ry,0,w,h,0,0}),color,yy,id,0,part});
        };
        const RenderColor wood{0x74,0x51,0x3F,255}, light{0xB7,0x83,0x50,255};
        if (vertical) {
            rect(xx,yy-1.28,.075,1,wood,0);
            rect(xx+.07,yy-1.1,.065,1,light,1);
        } else {
            rect(xx,yy-.32,1,.065,light,0);
            rect(xx,yy-.15,1,.065,wood,1);
        }
        rect(xx,yy-.4,.12,.4,wood,2);
        rect(xx,yy-.4,.12,.06,light,3);
    };
    // Open one-tile entrances on each side; the fence is presentation, not
    // an invisible navigation barrier for existing pasture routes.
    const int left = std::max(0,int(p.cameraX-p.screenWidth*.5/p.tilePixels-x)-2);
    const int right = std::min(f.width,int(p.cameraX+p.screenWidth*.5/p.tilePixels-x)+2);
    for (int i=left;i<right;++i) if (i != f.width/2) {
        piece(x+i,y+.08,false); piece(x+i,y+f.height-.03,false);
    }
    const int top = std::max(0,int(p.cameraY-p.screenHeight*.5/p.tilePixels-y)-2);
    const int bottom = std::min(f.height,int(p.cameraY+p.screenHeight*.5/p.tilePixels-y)+2);
    for (int i=top;i<bottom;++i) if (i != f.height/2) {
        piece(x+.03,y+i+1,true); piece(x+f.width-.14,y+i+1,true);
    }
}
}

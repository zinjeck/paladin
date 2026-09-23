#pragma once
#include "rendering/LocalTangentWorldView.h"
#include "rendering/WorldPresentation.h"
namespace Paladin
{
    // One moving surface, rather than transparent copies of two projections.
    inline WorldSurface::Point3 worldTransitionPoint(
        const GlobeView& globe, double u,double v,int width,int height,double weight)
    {
        auto p=globe.project(u,v);
        if(weight<=0) return p;
        const auto center=WorldSurface::coordinates(globe.orientation().inverse().apply({0,0,1}));
        double dx=u-center.u; dx-=std::round(dx);
        const double pixels=globe.radius*2*PlanetAstronomy::Pi/width;
        const double roll=globe.surfaceRollRadians(),c=std::cos(roll),s=std::sin(roll);
        const double dy=(v-center.v)*height;
        dx*=width;
        p.x=std::lerp(p.x,globe.cx+(dx*c-dy*s)*pixels,weight);
        p.y=std::lerp(p.y,globe.cy+(dx*s+dy*c)*pixels,weight);
        return p;
    }
}

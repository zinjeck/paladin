#pragma once
#include "rendering/GlobeView.h"
#include "rendering/Renderer.h"
#include "rendering/Texture.h"
#include "rendering/SceneDetail.h"
#include <array>
#include <vector>

namespace Paladin
{
    // Decorative atmospheric optics, not a simulated weather/climate model.
    // Seamless 3D density is advected by easterly trade/polar winds and
    // mid-latitude westerlies. Both hemispheres share the circulation bands.
    class WorldAtmosphere
    {
        std::unique_ptr<Texture> white_;
        std::vector<MeshVertex> vertices_;
        std::vector<int> indices_;
        static double noise(double x,double y,double z)
        {
            const int ix=int(std::floor(x)), iy=int(std::floor(y)), iz=int(std::floor(z));
            const auto smooth=[](double v) { return v*v*(3-2*v); };
            const double fx=smooth(x-ix),fy=smooth(y-iy),fz=smooth(z-iz);
            const auto hash=[](int a,int b,int c) {
                std::uint32_t h=std::uint32_t(a)*73856093u ^ std::uint32_t(b)*19349663u ^ std::uint32_t(c)*83492791u;
                h ^= h>>13; h*=1274126177u; h ^= h>>16;
                return double(h)/4294967295.;
            };
            const auto row=[&](int yy,int zz) { return std::lerp(hash(ix,yy,zz),hash(ix+1,yy,zz),fx); };
            return std::lerp(std::lerp(row(iy,iz),row(iy+1,iz),fy),
                             std::lerp(row(iy,iz+1),row(iy+1,iz+1),fy),fz);
        }
    public:
        static double wind(double latitude) { return -.018*std::cos(4*latitude); }
        static double opacity(double pixels) { return .30*(1-detailBlend(pixels,6.,30.)); }
        static double density(double u,double v,double days)
        {
            const double lat=PlanetAstronomy::latitude(v);
            const auto p=WorldSurface::sphere(u-wind(lat)*days,v);
            const double n=.58*noise(p.x*9,p.y*12,p.z*9)+
                           .28*noise(p.x*23+17,p.y*21,p.z*23)+
                           .14*noise(p.x*49,p.y*43+31,p.z*49);
            const double belt=.04*std::cos(lat*6);
            return detailBlend(n+belt,.44,.68);
        }
        void prepare(Renderer& r)
        {
            if (!white_) white_=r.createTextureFromPixels(1,1,std::array<RenderColor,1>{{{255,255,255,255}}});
        }
        void render(Renderer& r,const GlobeView& view,double pixels,double days,double solarSeconds)
        {
            const double alpha=opacity(pixels);
            if (alpha<.001) return;
            prepare(r); if (!white_) return;
            // Fixed screen mesh caps work independently of planet dimensions.
            constexpr int columns=80,rows=48;
            vertices_.clear(); indices_.clear();
            vertices_.reserve((columns+1)*(rows+1)); indices_.reserve(columns*rows*6);
            const double left=std::max(0.,view.cx-view.radius),top=std::max(0.,view.cy-view.radius);
            const double width=std::min(double(r.outputWidth()),view.cx+view.radius)-left;
            const double height=std::min(double(r.outputHeight()),view.cy+view.radius)-top;
            const auto inverse=view.orientation().inverse();
            const auto sun=PlanetAstronomy::sunDirection(solarSeconds);
            for(int y=0;y<=rows;++y) for(int x=0;x<=columns;++x)
            {
                const double sx=left+width*x/columns, sy=top+height*y/rows;
                const double nx=(sx-view.cx)/view.radius,ny=(view.cy-sy)/view.radius;
                const double z=std::sqrt(std::max(0.,1-nx*nx-ny*ny));
                RenderColor c{170,195,216,0};
                if(z>0)
                {
                    const auto p=inverse.apply({nx,ny,z});
                    const auto uv=WorldSurface::coordinates(p);
                    const double light=detailBlend(p.x*sun.x+p.y*sun.y+p.z*sun.z,-.15,.45);
                    c={std::uint8_t(76+171*light),std::uint8_t(103+147*light),std::uint8_t(139+115*light),
                       std::uint8_t(255*alpha*density(uv.u,uv.v,days)*detailBlend(z,0.,.18))};
                }
                vertices_.push_back({float(sx),float(sy),.5F,.5F,c});
                if(x && y)
                {
                    const int a=(y-1)*(columns+1)+x-1,b=a+columns+1;
                    if(vertices_[a].color.alpha || vertices_[a+1].color.alpha || vertices_[b].color.alpha || c.alpha)
                        indices_.insert(indices_.end(),{a,a+1,b+1,a,b+1,b});
                }
            }
            r.drawMesh(*white_,vertices_,indices_);
        }
    };
}

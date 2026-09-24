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
        std::unique_ptr<Texture> clouds_;
        std::unique_ptr<Texture> airlight_;
        std::vector<MeshVertex> grid_;
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
        static double opacity(double pixels) { return .42*(1-detailBlend(pixels,4.,30.)); }
        // Bounded shear plus a rigid rotation: the latitude derivative must
        // never grow with world age. No modulo reset or discontinuous reseed.
        static double advectedLongitude(double u,double v,double days)
        {
            return u-.012*std::remainder(days,1000.)+
                wind(PlanetAstronomy::latitude(v))*1.5*std::sin(days*.35);
        }
        struct WeatherSample
        {
            double cloudCover=1;
            double storm=0;
            double precipitation=0; // presentation signal for future weather consumers
        };
        static WeatherSample weather(double u,double v,double days,double land)
        {
            const auto p=WorldSurface::sphere(u-.006*days,v);
            const double front=detailBlend(noise(p.x*3+41,p.y*4,p.z*3),.64,.85);
            return {std::lerp(.85,.12+.78*front,std::clamp(land,0.,1.)),front,front*front};
        }
        static double density(double u,double v,double days)
        {
            const double lat=PlanetAstronomy::latitude(v);
            const auto p=WorldSurface::sphere(advectedLongitude(u,v,days),v);
            // Domain-warped spherical noise: coherent weather systems with
            // feathered fine structure, continuous across the seam and poles.
            const double warp=noise(p.x*4+19,p.y*4,p.z*4)*2;
            double n=0,weight=.52,frequency=7;
            for(int octave=0;octave<7;++octave)
            {
                n+=weight*noise(p.x*frequency+warp,p.y*frequency+31,p.z*frequency+warp);
                frequency*=2.03; weight*=.49;
            }
            return detailBlend(n+.035*std::cos(lat*6),.43,.66);
        }
        void prepare(Renderer& r)
        {
            if(clouds_) return;
            constexpr int width=2048,height=1024;
            // Two identical longitude periods permit triangles to cross the
            // date line without sampling across the entire map. Built once,
            // never regenerated while rotating, zooming or advancing time.
            std::vector<RenderColor> texels(width*2*height);
            for(int y=0;y<height;++y) for(int x=0;x<width;++x)
            {
                const double d=density(double(x)/width,double(y)/(height-1),0);
                const auto shade=std::uint8_t(224+31*d);
                const RenderColor c{shade,shade,255,std::uint8_t(255*d)};
                texels[y*width*2+x]=texels[y*width*2+x+width]=c;
            }
            clouds_=r.createTextureFromPixels(width*2,height,texels);
            if(clouds_) r.setTextureFiltering(*clouds_,true);
            airlight_=r.createTextureFromPixels(1,1,std::array<RenderColor,1>{{{255,255,255,255}}});
        }
        void render(Renderer& r,const GlobeView& view,double pixels,double days,double solarSeconds,const WorldGrid* terrain=nullptr)
        {
            const double alpha=opacity(pixels);
            if(alpha<.001 || view.radius<=0) return;
            prepare(r); if(!clouds_) return;
            // Native optical mesh. Geometry only interpolates geographic UVs;
            // density lives in the stable, filtered high resolution texture.
            constexpr int columns=128,rows=80;
            grid_.clear(); vertices_.clear(); indices_.clear();
            grid_.reserve((columns+1)*(rows+1));
            vertices_.reserve(columns*rows*6); indices_.reserve(columns*rows*6);
            const double radius=view.radius*1.006;
            const double left=std::max(0.,view.cx-radius),top=std::max(0.,view.cy-radius);
            const double width=std::min(double(r.outputWidth()),view.cx+radius)-left;
            const double height=std::min(double(r.outputHeight()),view.cy+radius)-top;
            if(width<=0 || height<=0) return;
            const auto inverse=view.orientation().inverse();
            const auto sun=PlanetAstronomy::sunDirection(solarSeconds);
            for(int y=0;y<=rows;++y) for(int x=0;x<=columns;++x)
            {
                const double sx=left+width*x/columns,sy=top+height*y/rows;
                const double nx=(sx-view.cx)/radius,ny=(view.cy-sy)/radius;
                const double z=std::sqrt(std::max(0.,1-nx*nx-ny*ny));
                const auto p=inverse.apply({nx,ny,z});
                const auto uv=WorldSurface::coordinates(p);
                const double light=detailBlend(p.x*sun.x+p.y*sun.y+p.z*sun.z,-.18,.5);
                const double u=advectedLongitude(uv.u,uv.v,days);
                double land=0;
                if(terrain)
                {
                    const double tx=uv.u*terrain->width()-.5,ty=uv.v*terrain->height()-.5;
                    const int ix=int(std::floor(tx)),iy=int(std::floor(ty));
                    const double fx=tx-ix,fy=ty-iy;
                    for(int j=0;j<2;++j) for(int i=0;i<2;++i)
                    {
                        const auto* tile=terrain->tile({(ix+i+terrain->width())%terrain->width(),std::clamp(iy+j,0,terrain->height()-1)});
                        if(tile->terrain!=TerrainType::Water) land+=(i?fx:1-fx)*(j?fy:1-fy);
                    }
                }
                const auto weatherState=weather(uv.u,uv.v,days,land);
                const RenderColor c{std::uint8_t(55+200*light),std::uint8_t(74+181*light),
                    std::uint8_t(110+145*light),std::uint8_t(255*alpha*weatherState.cloudCover*detailBlend(z,0.,.22))};
                grid_.push_back({float(sx),float(sy),float(u-std::floor(u)),float(uv.v),c});
            }
            const auto triangle=[&](int a,int b,int c)
            {
                std::array<MeshVertex,3> t{grid_[a],grid_[b],grid_[c]};
                if(!t[0].color.alpha && !t[1].color.alpha && !t[2].color.alpha) return;
                const float lo=std::min({t[0].u,t[1].u,t[2].u});
                const float hi=std::max({t[0].u,t[1].u,t[2].u});
                for(auto vertex:t)
                {
                    if(hi-lo>.5F && vertex.u<.5F) vertex.u+=1;
                    vertex.u*=.5F;
                    indices_.push_back(int(vertices_.size())); vertices_.push_back(vertex);
                }
            };
            for(int y=0;y<rows;++y) for(int x=0;x<columns;++x)
            {
                const int a=y*(columns+1)+x,b=a+columns+1;
                triangle(a,a+1,b+1); triangle(a,b+1,b);
            }
            r.drawMesh(*clouds_,vertices_,indices_);
            // Longer paths through air at the horizon produce soft blue
            // scattering. Fade with descent so local geography stays legible.
            if(airlight_)
            {
                for(auto& vertex:vertices_)
                {
                    const double nx=(vertex.x-view.cx)/radius,ny=(view.cy-vertex.y)/radius;
                    const double z=std::sqrt(std::max(0.,1-nx*nx-ny*ny));
                    const double daylight=(vertex.color.red-55)/200.;
                    const double limb=std::pow(1-z,3)*detailBlend(z,0.,.12);
                    vertex.u=vertex.v=.5F;
                    vertex.color={115,175,242,std::uint8_t(255*alpha*(.035+.25*limb)*daylight*detailBlend(z,0.,.025))};
                }
                r.drawMesh(*airlight_,vertices_,indices_);
            }
        }
    };
}

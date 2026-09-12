#pragma once

#include "TestFramework.h"
#include "rendering/CelestialSun.h"
#include "rendering/LocalTangentWorldView.h"
#include "rendering/WorldObjectRenderer.h"
#include "rendering/WorldPoliticalSurface.h"
#include "rendering/WorldRealmPresentationRenderer.h"
#include "rendering/WorldRenderer.h"
#include "rendering/WorldPixelStability.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

namespace Paladin::Test
{
    using ReviewSurface = std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>;
    inline ReviewSurface readWorldReview(SDL_Renderer* native)
    {
        ReviewSurface raw(SDL_RenderReadPixels(native, nullptr), SDL_DestroySurface);
        PALADIN_CHECK(raw);
        ReviewSurface result(SDL_ConvertSurface(raw.get(), SDL_PIXELFORMAT_RGBA32), SDL_DestroySurface);
        PALADIN_CHECK(result);
        return result;
    }
    inline RenderColor reviewPixel(SDL_Surface* s, int x, int y)
    {
        return reinterpret_cast<const RenderColor*>(static_cast<const char*>(s->pixels)+y*s->pitch)[x];
    }
    inline void saveWorldReview(SDL_Renderer* native, const std::string& name)
    {
        const char* dir=SDL_getenv("PALADIN_SMOKE_SCREENSHOTS");
        if (!dir) return;
        std::filesystem::create_directories(dir);
        const auto image=readWorldReview(native);
        PALADIN_CHECK(IMG_SavePNG(image.get(),(std::filesystem::path(dir)/name).string().c_str()));
    }
    inline void worldSurfaceRenderingChecks(Renderer& renderer, SDL_Window* window)
    {
        auto* native=SDL_GetRenderer(window);
        SDL_SetWindowSize(window,960,640); SDL_SyncWindow(window);
        WorldGenerationSettings settings; settings.width=64; settings.height=48; settings.seed=5731;
        World world(settings);
        for (int y=0;y<48;++y) for (int x=0;x<64;++x)
        {
            auto& t=*world.grid().tile({x,y});
            const bool water=(x>=30 && x<=35 && !(x>=32 && x<=33 && y>=23 && y<=24)) ||
                             (x==29 && y>=27 && y<=29);
            t.terrain=water?TerrainType::Water:TerrainType::Land;
            t.biome=water?BiomeType::Ocean:BiomeType::Plain;
            t.relief=ReliefType::Lowland; t.temperature=Temperature{.5}; t.rainfall=Rainfall{.5};
        }
        world.grid().terrainChanged();
        const auto civic=world.createRealm();
        const auto city=world.foundCapitalSettlement({28,24},civic,{"Civic coast","Coastfolk","HALDEN",{190,90,145},"civic"});
        PALADIN_CHECK(city);
        const auto tribe=world.createRealm();
        PALADIN_CHECK(world.foundCapitalSettlement({42,24},tribe,{"Tribal coast","Greenfolk","WALD",{79,140,122},"tribal"}));
        const auto civicTiles=world.territory().controlledTileCount();
        const auto influenceRevision=world.tribalInfluence().revision();

        // Mathematical coast agreement at every canonical sub-tile sample,
        // including water-side rounding, islands and concave coves.
        std::size_t dry=0,wet=0,curved=0;
        for (int y=20*16;y<31*16;++y) for (int x=26*16;x<37*16;++x)
        {
            const double xx=(x+.5)/16,yy=(y+.5)/16;
            const auto s=worldPoliticalSurfaceAt(world,xx,yy);
            const auto w=coastSample(xx,yy,true);
            PALADIN_CHECK(s.land==(worldLandField(world.grid(),w.x,w.y)>=.5));
            if (!s.land) { ++wet; PALADIN_CHECK(!s.civic); }
            else ++dry;
            curved+=s.land!=(world.grid().tile({x/16,y/16})->terrain!=TerrainType::Water);
        }
        PALADIN_CHECK(dry>0 && wet>0 && curved>0);

        WorldRealmPresentationRenderer politics;
        Camera2D camera(30,24); camera.setWorldZoom(1);
        TileRenderMetrics metrics; metrics.tilePixels=64;
        auto weights=worldPresentationState(64); weights.realmFillWeight=1; weights.realmBorderWeight=1; weights.realmLabelWeight=0;
        const auto drawMask=[&] {
            renderer.beginFrame();
            renderer.fillRectangle(0,0,960,640,{0,0,0,255});
            WorldPixelScene scene(renderer,64);
            renderer.fillRectangle(0,0,960,640,{0,0,0,255});
            politics.renderFlat(renderer,world,camera,metrics,weights,{});
        };
        for (int i=0;i<16;++i) drawMask();
        const auto warm=politics.cacheBuilds();
        const auto mask=readWorldReview(native);
        std::size_t seaInk=0,landInk=0;
        for (int y=0;y<640;y+=4) for (int x=0;x<960;x+=4)
        {
            const auto at=worldPoliticalSurfaceAt(world,30+(x+2-480)/64.,24+(y+2-320)/64.);
            const auto c=reviewPixel(mask.get(),x+2,y+2);
            const bool painted=c.red||c.green||c.blue;
            if (!at.land && painted) ++seaInk;
            if (at.land && painted) ++landInk;
        }
        std::cout<<"political coast sea_ink="<<seaInk<<" land_ink="<<landInk<<'\n';
        PALADIN_CHECK(seaInk==0 && landInk>100);
        saveWorldReview(native,"pr25-coast-mask.png");
        for (int i=0;i<8;++i) { camera.move(.005,0); drawMask(); }
        PALADIN_CHECK(politics.cacheBuilds()==warm);
        PALADIN_CHECK(politics.detailCacheBytes()<=32*1024*1024);
        // One terrain revision invalidates both layers, never only the border.
        auto tile=*world.grid().tile({29,28}); tile.terrain=TerrainType::Land; tile.biome=BiomeType::Plain;
        PALADIN_CHECK(world.grid().setTile({29,28},tile)); drawMask();
        PALADIN_CHECK(politics.cacheBuilds()>warm);

        // Real native-pixel marker rasters, compared after removing ONLY whole-
        // plate translation. Include crossing snapped-camera epochs, roll,
        // fractional zoom and high zoom where the old icon became five pixels.
        WorldObjectRenderer objects;
        std::ofstream motion;
        if (const char* dir=SDL_getenv("PALADIN_SMOKE_SCREENSHOTS"))
        { motion.open(std::filesystem::path(dir)/"pr25-marker-motion.csv"); motion<<"projection,pixels,roll,frame,left,top,width,height\n"; }
        for (bool globe : {false,true}) for (double pixels : {40.,64.,96.,127.5,256.}) for (double roll : {0.,.37})
        {
            if (!globe && roll!=0) continue;
            std::vector<RenderColor> reference;
            int rw=0,rh=0,lastLeft=0,lastTop=0;
            const auto presentation=worldPresentationState(pixels);
            for (int frame=0;frame<32;++frame)
            {
                const double cx=28.5+(frame-8)*.45/pixels,cy=24.5;
                Camera2D actual(cx,cy);
                if (globe)
                {
                    actual.setPlanetRotation(GlobeView::orientationAt({cx/64.,cy/48.},roll),64,48);
                    actual.setZoom(pixels*64/(640*.4*2*3.14159265358979323846));
                }
                const auto source=pixelStableWorldCamera(actual,world.grid(),960,640,globe);
                auto view=LocalTangentWorldView::from(source,world.grid(),960,640,pixels);
                if (!globe) view.setRollRadians(0);
                const auto residual=view.rigidOffsetToCenter(actual.tileX(),actual.tileY());
                renderer.beginFrame(); renderer.fillRectangle(0,0,960,640,{0,0,0,255});
                objects.render(renderer,world,source,pixels,globe,presentation,true,{},std::nullopt,residual);
                const auto image=readWorldReview(native);
                int x0=960,y0=640,x1=0,y1=0;
                for (int y=240;y<360;++y) for (int x=350;x<610;++x)
                {
                    const auto c=reviewPixel(image.get(),x,y);
                    if (!(c.red||c.green||c.blue)) continue;
                    x0=std::min(x0,x);x1=std::max(x1,x);y0=std::min(y0,y);y1=std::max(y1,y);
                }
                PALADIN_CHECK(x1>=x0 && y1>=y0);
                const int w=x1-x0+1,h=y1-y0+1;
                std::vector<RenderColor> plate;
                for (int y=y0;y<=y1;++y) for (int x=x0;x<=x1;++x) plate.push_back(reviewPixel(image.get(),x,y));
                if (!frame) { reference=plate; rw=w;rh=h; }
                else
                {
                    PALADIN_CHECK(w==rw && h==rh);
                    PALADIN_CHECK(plate.size()==reference.size());
                    for (std::size_t i=0;i<plate.size();++i)
                        PALADIN_CHECK(plate[i].red==reference[i].red && plate[i].green==reference[i].green && plate[i].blue==reference[i].blue);
                    PALADIN_CHECK(std::abs(x0-lastLeft)<=2 && std::abs(y0-lastTop)<=2);
                }
                PALADIN_CHECK(h>=20 && w>=30); // legible symbol AND name, not 5 crushed pixels
                lastLeft=x0;lastTop=y0;
                if (motion) motion<<(globe?"globe":"flat")<<','<<pixels<<','<<roll<<','<<frame<<','<<x0<<','<<y0<<','<<w<<','<<h<<'\n';
                if (pixels==96 && (frame==0 || frame==16 || frame==31))
                    saveWorldReview(native,std::string("pr25-marker-")+(globe?"globe":"flat")+"-"+std::to_string(int(roll*100))+"-"+std::to_string(frame)+".png");
            }
        }
        std::cout<<"marker motion: 480 frames, identical translated plates\n";

        // Actual full-world captures in both map modes/projections. Country
        // membership and the influence field must not mutate during rendering.
        WorldRenderer map;
        const auto deadline=SDL_GetTicks()+30000;
        while (!map.prepareTerrain(renderer,world) && SDL_GetTicks()<deadline) SDL_Delay(1);
        PALADIN_CHECK(map.terrainLocalDetailReady());
        world.time().advanceMinutes(6*60); // clear daytime at central longitudes
        for (bool globe : {false,true}) for (bool political : {false,true})
        {
            map.globeEnabled=globe;
            map.setMapMode(political?WorldMapMode::Political:WorldMapMode::Terrain);
            camera.setPosition(30,24); camera.setWorldZoom(1); metrics.tilePixels=64;
            if (globe)
            {
                camera.setPlanetRotation(GlobeView::orientationAt({30./64,24./48},.37),64,48);
                camera.setZoom(64.*64/(640*.4*2*3.14159265358979323846));
            }
            for (int i=0;i<16;++i) { renderer.beginFrame(); map.render(renderer,world,camera,metrics); }
            const auto full=readWorldReview(native);
            std::size_t black=0;
            for (int y=4;y<636;++y) for (int x=4;x<956;++x)
            { const auto c=reviewPixel(full.get(),x,y); black+=c.red==0 && c.green==0 && c.blue==0; }
            std::cout<<"world composite globe="<<globe<<" political="<<political<<" black="<<black<<'\n';
            PALADIN_CHECK(black==0);
            saveWorldReview(native,std::string("pr25-")+(globe?"globe":"flat")+(political?"-political.png":"-terrain.png"));
        }
        PALADIN_CHECK(world.territory().controlledTileCount()==civicTiles);
        PALADIN_CHECK(world.territory().controlledTileCount(tribe)==0);

        // Solar optical field, immutable cache and full occultation regression.
        CelestialSunRenderer sun;
        PALADIN_CHECK(sun.prepare(renderer)); const auto sunBuilds=sun.textureBuilds();
        GlobeView solar{480,320,180,0,0,PlanetRotation::axis(0,0,1,2.65)};
        renderer.beginFrame(); renderer.fillRectangle(0,0,960,640,{8,15,27,255});
        sun.render(renderer,solar,3*3600.);
        saveWorldReview(native,"pr25-sun-glare.png");
        const auto lit=readWorldReview(native); std::size_t white=0;
        for (int y=0;y<640;++y) for (int x=0;x<960;++x)
        { const auto c=reviewPixel(lit.get(),x,y); white+=c.red>245 && c.green>245 && c.blue>240; }
        PALADIN_CHECK(white>5);
        const auto sunPosition=projectCelestialSun(solar,960,640,3*3600.);
        PALADIN_CHECK(sunPosition);
        const int centerX=int(std::round(sunPosition->x)),centerY=int(std::round(sunPosition->y));
        for (int y=centerY-18;y<=centerY+18;++y)
        {
            if (y<0 || y>=640 || centerX<0 || centerX>=960) continue;
            const auto c=reviewPixel(lit.get(),centerX,y);
            PALADIN_CHECK(c.red>15); // no background-colored holes through bloom
        }
        for (int i=0;i<24;++i) { solar.rotation=PlanetRotation::axis(0,0,1,2.65+i*.002); sun.render(renderer,solar,3*3600.); }
        PALADIN_CHECK(sun.textureBuilds()==sunBuilds);
        solar.rotation=PlanetRotation{};
        renderer.beginFrame(); renderer.fillRectangle(0,0,960,640,{8,15,27,255});
        sun.render(renderer,solar,0);
        const auto hidden=readWorldReview(native);
        for (int y=0;y<640;++y) for (int x=0;x<960;++x)
        { const auto c=reviewPixel(hidden.get(),x,y); PALADIN_CHECK(c.red==8 && c.green==15 && c.blue==27); }
        // Real planet/space context, with the sun near its illuminated limb.
        world.time().advanceMinutes(9*60);
        map.globeEnabled=true; map.setMapMode(WorldMapMode::Terrain);
        camera.setPlanetRotation(PlanetRotation::axis(0,0,1,-.55),64,48); camera.setZoom(1);
        renderer.beginFrame(); map.render(renderer,world,camera,metrics);
        saveWorldReview(native,"pr25-sun-planet.png");
        std::cout<<"PR25 world-surface, marker, map-mode and solar raster checks passed\n";
    }
} // namespace Paladin::Test

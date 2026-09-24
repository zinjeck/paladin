#pragma once

#include "TestFramework.h"
#include "rendering/CelestialSun.h"
#include "rendering/LocalTangentWorldView.h"
#include "rendering/WorldObjectRenderer.h"
#include "rendering/WorldPixelStability.h"
#include "rendering/WorldPoliticalSurface.h"
#include "rendering/WorldRealmPresentationRenderer.h"
#include "rendering/WorldRenderer.h"
#include "rendering/WorldThematicPalette.h"
#include "world/generation/AiRealmGenerator.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
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

        // The compact renderer sources must produce exactly the reference
        // simulation-backed samples, including two competing tribal signals.
        // This separate world keeps the visual fixture below unchanged.
        {
            World sourceWorld(settings);
            for (int y = 0; y < settings.height; ++y)
            {
                for (int x = 0; x < settings.width; ++x)
                {
                    *sourceWorld.grid().tile({x, y}) =
                        *world.grid().tile({x, y});
                }
            }
            sourceWorld.grid().terrainChanged();
            const auto civicOwner = sourceWorld.createRealm();
            const auto firstTribe = sourceWorld.createRealm();
            const auto secondTribe = sourceWorld.createRealm();
            PALADIN_CHECK(sourceWorld.foundCapitalSettlement(
                {28, 24},
                civicOwner,
                {"Civic source", "Coastfolk", "HALDEN", {190, 90, 145}, "civic"}
            ));
            const auto firstCenter = sourceWorld.foundCapitalSettlement(
                {42, 24},
                firstTribe,
                {"First source", "Greenfolk", "WALD", {79, 140, 122}, "tribal"}
            );
            PALADIN_CHECK(firstCenter);
            PALADIN_CHECK(sourceWorld.foundCapitalSettlement(
                {56, 24},
                secondTribe,
                {"Second source", "Ashfolk", "ASH", {160, 100, 130}, "tribal"}
            ));
            const auto buildSource = [&]
            {
                WorldPoliticalSurfaceSource source(sourceWorld);
                while (!source.complete())
                {
                    source.appendRow(sourceWorld);
                }
                return source;
            };
            const auto source = buildSource();
            const auto& referenceInfluence = sourceWorld.tribalInfluence();
            const WorldTribalSurfaceSource influence(referenceInfluence);
            const auto equalSurface = [](const auto& a, const auto& b)
            {
                PALADIN_CHECK(a.land == b.land && a.civic == b.civic);
                PALADIN_CHECK(a.positions == b.positions);
                PALADIN_CHECK(a.dryWeights == b.dryWeights);
                PALADIN_CHECK(a.landWeight == b.landWeight);
            };
            const auto equalInfluence = [](const auto& a, const auto& b)
            {
                PALADIN_CHECK(a.primaryRealm == b.primaryRealm);
                PALADIN_CHECK(a.secondaryRealm == b.secondaryRealm);
                PALADIN_CHECK(a.primaryInfluence == b.primaryInfluence);
                PALADIN_CHECK(a.secondaryInfluence == b.secondaryInfluence);
            };
            std::size_t civicSamples = 0, tribalSamples = 0,
                        contestedSamples = 0;
            const auto compare = [&](double x, double y)
            {
                const auto reference =
                    worldPoliticalSurfaceAt(sourceWorld, x, y);
                const auto cached = worldPoliticalSurfaceAt(source, x, y);
                equalSurface(reference, cached);
                const auto referenceTribal =
                    worldTribalSurfaceSample(referenceInfluence, reference);
                equalInfluence(
                    referenceTribal,
                    worldTribalSurfaceSample(influence, cached)
                );
                civicSamples += bool(cached.civic);
                tribalSamples += bool(referenceTribal.primaryRealm);
                contestedSamples += bool(referenceTribal.secondaryRealm);
            };
            for (int y = 20 * 16; y < 30 * 16; ++y)
            {
                for (int x = 24 * 16; x < 64 * 16; ++x)
                {
                    compare((x + .5) / 16, (y + .5) / 16);
                }
            }
            // Longitude wrapping, polar clamping, and rejected latitudes must
            // agree too, not only the ordinary interior sampling domain.
            for (double y : {-1., 0., .03125, 24.5, 47.96875, 48.})
            {
                for (double x : {-128.03125, -.03125, 0., 63.96875, 64., 128.5})
                {
                    compare(x, y);
                }
            }
            for (int y = -1; y <= settings.height; ++y)
            {
                for (int x = -settings.width; x < settings.width * 2; ++x)
                {
                    equalInfluence(
                        referenceInfluence.sampleAt({x, y}),
                        influence.sampleAt({x, y})
                    );
                }
            }
            PALADIN_CHECK(civicSamples && tribalSamples && contestedSamples);

            // A prepared source owns its data. Later terrain and population
            // revisions must leave it intact while a new source matches the
            // changed authoritative world.
            const auto oldCoast = worldPoliticalSurfaceAt(source, 29.5, 28.5);
            auto changedTile = *sourceWorld.grid().tile({29, 28});
            changedTile.terrain = TerrainType::Land;
            changedTile.biome = BiomeType::Plain;
            PALADIN_CHECK(sourceWorld.grid().setTile({29, 28}, changedTile));
            const auto changedSource = buildSource();
            const auto changedCoast =
                worldPoliticalSurfaceAt(changedSource, 29.5, 28.5);
            equalSurface(
                changedCoast,
                worldPoliticalSurfaceAt(sourceWorld, 29.5, 28.5)
            );
            equalSurface(oldCoast, worldPoliticalSurfaceAt(source, 29.5, 28.5));
            PALADIN_CHECK(oldCoast.landWeight != changedCoast.landWeight);
            PALADIN_CHECK(sourceWorld.editRealmIdentity(
                civicOwner,
                {"Civic source",
                 "Coastfolk",
                 "HALDEN",
                 {190, 90, 145},
                 "tribal"}
            ));
            const auto changedOwners = buildSource();
            equalSurface(
                worldPoliticalSurfaceAt(changedOwners, 28.5, 24.5),
                worldPoliticalSurfaceAt(sourceWorld, 28.5, 24.5)
            );
            PALADIN_CHECK(
                !worldPoliticalSurfaceAt(changedOwners, 28.5, 24.5).civic
            );
            PALADIN_CHECK(
                worldPoliticalSurfaceAt(source, 28.5, 24.5).civic == civicOwner
            );
            const auto oldAuthority = influence.sampleAt({42, 24});
            auto& population = sourceWorld.settlement(firstCenter)
                                   ->simulationState()
                                   .population();
            population = SettlementPopulation{
                population.residents() + 1000,
                population.rates()
            };
            const auto& grownField = sourceWorld.tribalInfluence();
            const WorldTribalSurfaceSource grownInfluence(grownField);
            equalInfluence(oldAuthority, influence.sampleAt({42, 24}));
            equalInfluence(
                grownField.sampleAt({42, 24}),
                grownInfluence.sampleAt({42, 24})
            );
            PALADIN_CHECK(
                grownInfluence.sampleAt({42, 24}).primaryInfluence >
                oldAuthority.primaryInfluence
            );
        }

        WorldRealmPresentationRenderer politics;
        Camera2D camera(30,24); camera.setWorldZoom(1);
        TileRenderMetrics metrics; metrics.tilePixels=64;
        auto weights=worldPresentationState(64); weights.realmFillWeight=1; weights.realmBorderWeight=1; weights.realmLabelWeight=0;
        const auto drawMask=[&] {
            renderer.beginFrame();
            renderer.fillRectangle(0,0,960,640,{0,0,0,255});
            {
                WorldPixelScene scene(renderer,64);
                renderer.fillRectangle(0,0,960,640,{0,0,0,255});
                politics.renderFlat(renderer,world,camera,metrics,weights,{});
            }
            politics.renderOutlines(renderer,world,camera,64,false,weights);
        };
        const auto settleMask=[&] {
            const auto deadline=SDL_GetTicks()+30000;
            do { drawMask(); }
            while (politics.hasPendingWork() && SDL_GetTicks()<deadline);
            PALADIN_CHECK(!politics.hasPendingWork());
        };
        settleMask();
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
        // Close borders are an independent ink layer: full-strength lines,
        // no fill added for this regression, and still no political ink at sea.
        weights.realmFillWeight=0;
        weights.realmBorderWeight=worldPresentationState(256).realmBorderWeight;
        PALADIN_CHECK(weights.realmBorderWeight==1);
        settleMask();
        const auto borderOnly=readWorldReview(native);
        std::size_t borderInk=0, borderSea=0;
        for (int y=0;y<640;y+=4) for (int x=0;x<960;x+=4)
        {
            const auto at=worldPoliticalSurfaceAt(world,30+(x+2-480)/64.,24+(y+2-320)/64.);
            const auto color=reviewPixel(borderOnly.get(),x+2,y+2);
            const bool ink=color.red||color.green||color.blue;
            if (ink && !at.land) ++borderSea;
            if (ink && at.land) ++borderInk;
        }
        PALADIN_CHECK(borderInk>0 && borderSea==0);
        saveWorldReview(native,"pr27-close-border-only.png");
        weights.realmBorderWeight=0; drawMask();
        const auto noInk=readWorldReview(native);
        for (int y=0;y<640;y+=4) for (int x=0;x<960;x+=4)
        {
            const auto color=reviewPixel(noInk.get(),x+2,y+2);
            PALADIN_CHECK(!color.red && !color.green && !color.blue);
        }
        PALADIN_CHECK(
            std::abs(worldPresentationState(256).realmFillWeight - .20F) <
            .0001F
        );
        weights.realmFillWeight=1; weights.realmBorderWeight=1;

        for (int i=0;i<8;++i) { camera.move(.005,0); drawMask(); }
        PALADIN_CHECK(politics.cacheBuilds()==warm);
        PALADIN_CHECK(politics.detailCacheBytes()<=32*1024*1024);
        // One terrain revision invalidates both layers, never only the border.
        auto tile=*world.grid().tile({29,28}); tile.terrain=TerrainType::Land; tile.biome=BiomeType::Plain;
        PALADIN_CHECK(world.grid().setTile({29,28},tile)); settleMask();
        PALADIN_CHECK(politics.cacheBuilds()>warm);

        // Repeated real population changes must not restart an unfinished
        // presentation forever. Once updates stop, its final pixels must equal
        // a freshly built cache of that exact world revision.
        auto& people=world.settlement(world.realm(tribe)->capitalSettlementId())
                         ->simulationState().population();
        const auto originalPeople=people;
        const auto beforeRefresh=politics.cacheBuilds();
        const auto refreshDeadline=SDL_GetTicks()+30000;
        do {
            people=SettlementPopulation{people.residents()+1,people.rates()};
            drawMask();
        } while (politics.cacheBuilds()==beforeRefresh && SDL_GetTicks()<refreshDeadline);
        PALADIN_CHECK(politics.cacheBuilds()>beforeRefresh);
        settleMask();
        const auto refreshed=readWorldReview(native);
        WorldRealmPresentationRenderer referencePolitics;
        const auto referenceDeadline=SDL_GetTicks()+30000;
        do {
            renderer.beginFrame();
            renderer.fillRectangle(0,0,960,640,{0,0,0,255});
            {
                WorldPixelScene scene(renderer,64);
                renderer.fillRectangle(0,0,960,640,{0,0,0,255});
                referencePolitics.renderFlat(renderer,world,camera,metrics,weights,{});
            }
            referencePolitics.renderOutlines(renderer,world,camera,64,false,weights);
        } while (referencePolitics.hasPendingWork() && SDL_GetTicks()<referenceDeadline);
        PALADIN_CHECK(!referencePolitics.hasPendingWork());
        const auto referenceMask=readWorldReview(native);
        for (int y=0;y<640;++y) for (int x=0;x<960;++x) {
            const auto a=reviewPixel(refreshed.get(),x,y), b=reviewPixel(referenceMask.get(),x,y);
            PALADIN_CHECK(a.red==b.red && a.green==b.green && a.blue==b.blue && a.alpha==b.alpha);
        }
        people=originalPeople;
        settleMask();
        const auto influenceRevision=world.tribalInfluence().revision();

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
        bool prepared=false;
        do {
            renderer.beginFrame(); prepared=map.prepareTerrain(renderer,world);
            renderer.endFrame();
            if (!prepared) SDL_Delay(1);
        }
        while (!prepared && SDL_GetTicks()<deadline);
        PALADIN_CHECK(prepared);
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
            const auto settleDeadline=SDL_GetTicks()+30000;
            do { renderer.beginFrame(); map.render(renderer,world,camera,metrics); }
            while (political && map.politicalWorkPending() && SDL_GetTicks()<settleDeadline);
            if (political) PALADIN_CHECK(!map.politicalWorkPending());
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

        // Government ink uses authoritative realms; population is a geographic census in both
        // projections; selected frontiers do not convert tribal influence into land ownership.
        PALADIN_CHECK(world.tribalInfluence().revision()==influenceRevision);
        PALADIN_CHECK(world.settlement(city)->simulationState().spawnCitizens(40));
        const auto thematicInfluenceRevision=world.tribalInfluence().revision();
        std::array<std::size_t,2> governmentWater{};
        for(const bool globe:{false,true}) for(const auto mode:{WorldMapMode::Government,WorldMapMode::Population})
        {
            map.globeEnabled=globe; map.setMapMode(mode); map.selectedRealm={};
            camera.setPosition(34,24); camera.setWorldZoom(1); metrics.tilePixels=8;
            if(globe)
            {
                camera.setPlanetRotation(GlobeView::orientationAt({34./64,24./48},.37),64,48);
                camera.setWorldZoom(8.*64/(640*.4*2*3.14159265358979323846));
            }
            const auto themeDeadline=SDL_GetTicks()+30000;
            do { renderer.beginFrame(); map.render(renderer,world,camera,metrics); }
            while(map.politicalWorkPending() && SDL_GetTicks()<themeDeadline);
            PALADIN_CHECK(!map.politicalWorkPending());
            auto image=readWorldReview(native);
            const auto a=mode==WorldMapMode::Government?GovernmentCivic:populationMapColor(world.settlement(city)->population());
            const auto b=mode==WorldMapMode::Government?GovernmentTribal:populationMapColor(world.settlement(world.realm(tribe)->capitalSettlementId())->population());
            std::size_t first=0,second=0,water=0;
            for(int y=0;y<640;++y) for(int x=0;x<960;++x)
            { const auto p=reviewPixel(image.get(),x,y); first+=p==a; second+=p==b; water+=p==ThematicWater; }
            if(mode==WorldMapMode::Government)
            {
                PALADIN_CHECK(first>10 && second>10 && water>20);
                governmentWater[globe?1:0]=water;
            }
            else
            {
                // A population view retains shaded terrain, not a handful of
                // opaque country swatches. Its urban distribution is separately
                // checked for exact census conservation and no ownership input.
                std::set<std::uint32_t> shades;
                for(int y=100;y<450;++y) for(int x=100;x<700;++x)
                {
                    const auto p=reviewPixel(image.get(),x,y);
                    shades.insert((std::uint32_t(p.red)<<16)|(std::uint32_t(p.green)<<8)|p.blue);
                }
                PALADIN_CHECK(shades.size()>64);
                PALADIN_CHECK(water<governmentWater[globe?1:0]); // Natural sea restored; space may share the old color.
            }
            const std::string prefix=std::string("pr29-")+(globe?"globe-":"flat-")+(mode==WorldMapMode::Government?"government":"population");
            saveWorldReview(native,prefix+".png");
            for(const auto selected:{civic,tribe})
            {
                map.selectedRealm=selected;
                renderer.beginFrame(); map.render(renderer,world,camera,metrics);
                auto marked=readWorldReview(native); std::size_t changed=0;
                for(int y=0;y<640;++y) for(int x=0;x<960;++x)
                    changed+=reviewPixel(marked.get(),x,y)!=reviewPixel(image.get(),x,y);
                PALADIN_CHECK(changed>5);
                saveWorldReview(native,prefix+(selected==civic?"-selected-civic.png":"-selected-tribal.png"));
            }
            map.selectedRealm={};
            // Verify close detail pages with the same mode-specific rendering policy.
            camera.setWorldZoom(globe?64.*64/(640*.4*2*3.14159265358979323846):8.);
            const auto closeDeadline=SDL_GetTicks()+30000;
            do { renderer.beginFrame(); map.render(renderer,world,camera,metrics); }
            while(map.politicalWorkPending() && SDL_GetTicks()<closeDeadline);
            PALADIN_CHECK(!map.politicalWorkPending());
            saveWorldReview(native,prefix+"-close.png");
        }
        map.selectedRealm={}; metrics.tilePixels=64;
        PALADIN_CHECK(world.territory().controlledTileCount()==civicTiles);
        PALADIN_CHECK(world.territory().controlledTileCount(tribe)==0);
        PALADIN_CHECK(world.tribalInfluence().revision()==thematicInfluenceRevision);
        std::cout<<"Thematic government/population and selected civic/tribal frontiers: globe and flat passed\n";

        // Both projections request territory pages during the curved-to-local
        // transition. Neither may cancel the other's unfinished page forever.
        map.globeEnabled=true;
        map.setMapMode(WorldMapMode::Political);
        camera.setPlanetRotation(GlobeView::orientationAt({30./64,24./48},.37),64,48);
        camera.setZoom(34.*64/(640*.4*2*3.14159265358979323846));
        const auto blendDeadline=SDL_GetTicks()+30000;
        do { renderer.beginFrame(); map.render(renderer,world,camera,metrics); }
        while (map.politicalWorkPending() && SDL_GetTicks()<blendDeadline);
        PALADIN_CHECK(!map.politicalWorkPending());
        saveWorldReview(native,"world-zoom-blended-political.png");

        // Atmospheric density wraps at longitude and freezes with game time.
        PALADIN_CHECK(WorldAtmosphere::wind(0)<0 && WorldAtmosphere::wind(.75)>0);
        PALADIN_CHECK(WorldAtmosphere::opacity(40)==0);
        double movingClouds=0;
        for(int y=1;y<20;++y) for(int x=0;x<30;++x)
        {
            const double u=x/30.,v=y/20.;
            PALADIN_CHECK(std::abs(WorldAtmosphere::density(u,v,0)-WorldAtmosphere::density(u+1,v,0))<1.e-10);
            movingClouds+=std::abs(WorldAtmosphere::density(u,v,0)-WorldAtmosphere::density(u,v,1));
        }
        // The day-nine regression was an unbounded latitude derivative.
        for(double day:{9.,30.,365.,10000.,1000000.})
        {
            const double a=WorldAtmosphere::advectedLongitude(.5,.4,day);
            const double b=WorldAtmosphere::advectedLongitude(.5,.4001,day);
            PALADIN_CHECK(std::abs(b-a)<.0001);
        }
        PALADIN_CHECK(movingClouds>1);
        // Exercise actual filtered cloud output: identical paused frames,
        // continuous small camera movements, and visible clock-driven winds.
        WorldAtmosphere atmosphere;
        atmosphere.prepare(renderer);
        GlobeView cloudView{480,320,270,0,0,GlobeView::orientationAt({.52,.4},.2)};
        const auto cloudsAt=[&](const GlobeView& view,double days) {
            renderer.beginFrame();
            atmosphere.render(renderer,view,2,days,43200);
            return readWorldReview(native);
        };
        const auto cloudStill=cloudsAt(cloudView,0);
        saveWorldReview(native,"atmosphere-detail.png");
        const auto cloudPaused=cloudsAt(cloudView,0);
        const auto cloudLater=cloudsAt(cloudView,1);
        saveWorldReview(native,"atmosphere-next-day.png");
        for(double day:{9.,365.,10000.})
        {
            cloudsAt(cloudView,day);
            saveWorldReview(native,"atmosphere-age-"+std::to_string(int(day))+".png");
        }
        auto movedView=cloudView; movedView.cx+=.25;
        const auto cloudMoved=cloudsAt(movedView,0);
        double cameraDifference=0,timeDifference=0;
        for(int y=100;y<540;++y) for(int x=260;x<700;++x)
        {
            const auto a=reviewPixel(cloudStill.get(),x,y);
            const auto b=reviewPixel(cloudPaused.get(),x,y);
            PALADIN_CHECK(a.red==b.red && a.green==b.green && a.blue==b.blue);
            cameraDifference+=std::abs(int(a.red)-reviewPixel(cloudMoved.get(),x,y).red);
            timeDifference+=std::abs(int(a.red)-reviewPixel(cloudLater.get(),x,y).red);
        }
        PALADIN_CHECK(timeDifference>1000 && cameraDifference<timeDifference*.25);
        double cloudWorstMs=0;
        for(int frame=0;frame<24;++frame)
        {
            cloudView.rotation=GlobeView::orientationAt({.52+frame*.002,.4},.2);
            renderer.beginFrame();
            const auto start=std::chrono::steady_clock::now();
            atmosphere.render(renderer,cloudView,2,frame/1440.,43200);
            const double elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            cloudWorstMs=std::max(cloudWorstMs,elapsed);
        }
        std::cout<<"Atmosphere worst frame: "<<cloudWorstMs<<" ms\n";
        PALADIN_CHECK(cloudWorstMs<80);
        for(const double pixels:{2.,12.,28.,31.,34.,37.,40.,64.})
        {
            map.globeEnabled=true; map.setMapMode(WorldMapMode::Terrain);
            camera.setPlanetRotation(GlobeView::orientationAt({30./64,24./48},.37),64,48);
            camera.setZoom(pixels*64/(640*.4*2*PlanetAstronomy::Pi));
            renderer.beginFrame(); map.render(renderer,world,camera,metrics);
            saveWorldReview(native,"economy-pass-descent-"+std::to_string(int(pixels))+".png");
            if(pixels>28 && pixels<40)
            {
                const auto source=pixelStableWorldCamera(camera,world.grid(),960,640,true);
                const auto view=GlobeView::from(source,world.grid(),960,640);
                const auto projected=worldTransitionPoint(view,31./64,25./48,64,48,worldPresentationState(pixels).localWorldWeight);
                const auto picked=WorldMapNavigation::pick(camera,world.grid(),960,640,pixels,true,projected.x,projected.y);
                PALADIN_CHECK(picked && std::abs(picked->u-31./64)<.0001 && std::abs(picked->v-25./48)<.0001);
            }
        }
        // First-click rendering must keep visible geography, even while new
        // census pages are built; warmed switches retain their bounded caches.
        for(const auto mode:{WorldMapMode::Resources,WorldMapMode::Government,WorldMapMode::Population,WorldMapMode::Terrain,WorldMapMode::Population})
        {
            map.setMapMode(mode); renderer.beginFrame(); map.render(renderer,world,camera,metrics);
            saveWorldReview(native,"economy-pass-mode-"+std::to_string(int(mode))+".png");
            const auto screen=readWorldReview(native);
            std::size_t visible=0;
            for(int y=100;y<500;y+=4) for(int x=150;x<800;x+=4)
            {
                const auto c=reviewPixel(screen.get(),x,y);
                visible+=(c.green>35 || c.red>35);
            }
            PALADIN_CHECK(visible>100);
        }
        for(const auto b:{WorldMapNavigation::politicalModeButtonBounds(960,640),WorldMapNavigation::resourceModeButtonBounds(960,640),
                         WorldMapNavigation::terrainModeButtonBounds(960,640),WorldMapNavigation::governmentModeButtonBounds(960,640),
                         WorldMapNavigation::populationModeButtonBounds(960,640)})
            PALADIN_CHECK(std::string_view(WorldMapNavigation::modeTooltip(b.x+5,b.y+5,960,640)).size()>0);

        // Solar optical field, immutable cache and full occultation regression.
        CelestialSunRenderer sun;
        PALADIN_CHECK(
            CelestialSunOptics::visibleDiscFraction(190, 180, 7) == 1
        );
        PALADIN_CHECK(
            CelestialSunOptics::visibleDiscFraction(172, 180, 7) == 0
        );
        PALADIN_CHECK(
            std::abs(
                CelestialSunOptics::visibleDiscFraction(180, 180, 7) - .5F
            ) < .01F
        );
        float previousVisibility = 0;
        for (int i = 0; i <= 280; ++i)
        {
            const auto visible =
                CelestialSunOptics::visibleDiscFraction(173 + i * .05, 180, 7);
            PALADIN_CHECK(visible >= previousVisibility);
            previousVisibility = visible;
        }
        PALADIN_CHECK(sun.prepare(renderer)); const auto sunBuilds=sun.textureBuilds();
        GlobeView solar{480,320,180,0,0,PlanetRotation::axis(0,0,1,2.65)};
        renderer.beginFrame(); renderer.fillRectangle(0,0,960,640,{8,15,27,255});
        sun.render(renderer,solar,3*3600.);
        saveWorldReview(native,"pr25-sun-glare.png");
        const auto lit=readWorldReview(native); std::size_t white=0;
        auto limb = solar;
        const auto limbSource = projectCelestialSun(limb, 960, 640, 3 * 3600.);
        PALADIN_CHECK(limbSource);
        limb.radius =
            std::hypot(limbSource->x - limb.cx, limbSource->y - limb.cy) - 5;
        renderer.beginFrame();
        renderer.fillRectangle(0, 0, 960, 640, {8, 15, 27, 255});
        sun.render(renderer, limb, 3 * 3600.);
        const auto limbImage = readWorldReview(native);
        saveWorldReview(native, "realm-pass-sun-limb.png");
        std::size_t litAtmosphere = 0;
        for (int y = 0; y < 640; ++y)
        {
            for (int x = 0; x < 960; ++x)
            {
                const double radius =
                    std::hypot(x + .5 - limb.cx, y + .5 - limb.cy);
                const auto c = reviewPixel(limbImage.get(), x, y);
                if (radius > limb.radius + 1 && radius < limb.radius * 1.028 &&
                    c.red > 10)
                {
                    ++litAtmosphere;
                }
                // Lens ghosts are camera optics and may overlay the globe; the
                // existing fully-hidden-source check below tests solid
                // occultation.
            }
        }
        PALADIN_CHECK(litAtmosphere > 15);

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
        // Rays change independently at fixed astronomy/camera; holding the
        // presentation timestamp (pause) produces exactly the same image.
        solar.rotation=PlanetRotation::axis(0,0,1,2.65);
        const auto dynamicFrame = [&](double time)
        {
            renderer.beginFrame(); renderer.fillRectangle(0,0,960,640,{8,15,27,255});
            sun.render(renderer,solar,3*3600.,1.F,time);
            return readWorldReview(native);
        };
        const auto firstDynamic = dynamicFrame(0);
        saveWorldReview(native,"dynamic-sun-0.png");
        const auto nextDynamic = dynamicFrame(1./60.);
        const auto laterDynamic = dynamicFrame(3);
        saveWorldReview(native,"dynamic-sun-3.png");
        const auto pausedDynamic = dynamicFrame(3);
        std::uint64_t nearbyDifference=0, laterDifference=0;
        std::size_t movingPixels=0;
        for (int y=0;y<640;++y) for (int x=0;x<960;++x)
        {
            const auto a=reviewPixel(firstDynamic.get(),x,y), b=reviewPixel(laterDynamic.get(),x,y);
            const auto c=reviewPixel(pausedDynamic.get(),x,y), d=reviewPixel(nextDynamic.get(),x,y);
            PALADIN_CHECK(b.red==c.red && b.green==c.green && b.blue==c.blue);
            const int change=std::abs(int(a.red)-b.red)+std::abs(int(a.green)-b.green)+std::abs(int(a.blue)-b.blue);
            movingPixels+=change>6;
            laterDifference+=change;
            nearbyDifference+=std::abs(int(a.red)-d.red)+std::abs(int(a.green)-d.green)+std::abs(int(a.blue)-d.blue);
        }
        PALADIN_CHECK(movingPixels>300);
        PALADIN_CHECK(nearbyDifference<laterDifference/4);
        const auto opticalStart=SDL_GetTicksNS();
        for (int i=0;i<120;++i)
        {
            renderer.beginFrame(); sun.render(renderer,solar,3*3600.,1.F,i/60.);
            SDL_FlushRenderer(native);
        }
        std::cout<<"Dynamic sun: changed_pixels="<<movingPixels<<" mean_ms="
            <<(SDL_GetTicksNS()-opticalStart)/1e6/120<<'\n';
        PALADIN_CHECK(sun.textureBuilds()==sunBuilds);
        solar.rotation=PlanetRotation{};
        renderer.beginFrame(); renderer.fillRectangle(0,0,960,640,{8,15,27,255});
        sun.render(renderer,solar,0,1.F,9.);
        const auto hidden=readWorldReview(native);
        for (int y=0;y<640;++y) for (int x=0;x<960;++x)
        { const auto c=reviewPixel(hidden.get(),x,y); PALADIN_CHECK(c.red==8 && c.green==15 && c.blue==27); }
        // Real planet/space context, with the sun near its illuminated limb.
        world.time().advanceMinutes(9*60);
        map.globeEnabled=true; map.setMapMode(WorldMapMode::Terrain);
        camera.setPlanetRotation(PlanetRotation::axis(0,0,1,-.55),64,48); camera.setZoom(1);
        renderer.beginFrame(); map.render(renderer,world,camera,metrics);
        saveWorldReview(native,"pr25-sun-planet.png");
        // Real planet sequence through first contact, crescent and full cover.
        int sequence = 0;
        for (double wantedGap : {18., 5., 0., -3., -5., -9., -22.})
        {
            const auto solarDirection =
                PlanetAstronomy::sunDirection(world.time().secondsIntoDay());
            const double radial = (256. + wantedGap) / (640. * .65);
            const double length = std::sqrt(1 + radial * radial);
            camera.setPlanetRotation(
                PlanetRotation::between(
                    {solarDirection.x, solarDirection.y, solarDirection.z},
                    {-.8 * radial / length, .6 * radial / length, -1. / length}
                ),
                64,
                48
            );
            renderer.beginFrame();
            map.render(renderer, world, camera, metrics);
            saveWorldReview(
                native,
                "tweak-sun-contact-" + std::to_string(sequence++) + ".png"
            );
            if (wantedGap == 0.)
            {
                for (int frame=0;frame<4;++frame)
                {
                    map.animationSeconds=frame*1.5;
                    renderer.beginFrame(); map.render(renderer,world,camera,metrics);
                    saveWorldReview(native,"dynamic-sun-limb-"+std::to_string(frame)+".png");
                }
                map.animationSeconds=0;
            }
        }
        for (const auto* origin : {"civic", "tribal"})
        {
            WorldGenerationSettings linkedSettings;
            linkedSettings.width = 128;
            linkedSettings.height = 80;
            linkedSettings.seed = 731;
            World linked(linkedSettings);
            for (int y = 0; y < 80; ++y)
            {
                for (int x = 0; x < 128; ++x)
                {
                    auto& t = *linked.grid().tile({x, y});
                    const bool water =
                        y < 12 + int(3 * std::sin(x * .15)) || y > 64;
                    t.terrain = water ? TerrainType::Water : TerrainType::Land;
                    t.biome = water ? BiomeType::Ocean : BiomeType::Plain;
                    t.relief = ReliefType::Lowland;
                    t.temperature = Temperature{.5};
                    t.rainfall = Rainfall{.5};
                }
            }
            linked.grid().terrainChanged();
            const auto owner = linked.createRealm();
            PALADIN_CHECK(linked.foundCapitalSettlement(
                {28, 36},
                owner,
                {"Connected Realm", "Folk", "HOME", {204, 133, 84}, origin}
            ));
            for (auto p :
                 {WorldTilePosition{48, 30},
                  WorldTilePosition{68, 36},
                  WorldTilePosition{103, 44}})
            {
                PALADIN_CHECK(linked.foundSettlement(p, owner));
            }
            WorldRenderer linkedMap;
            linkedMap.globeEnabled = false;
            linkedMap.setMapMode(WorldMapMode::Political);
            const auto deadline = SDL_GetTicks() + 30000;
            bool linkedPrepared=false;
            do {
                renderer.beginFrame();
                linkedPrepared=linkedMap.prepareTerrain(renderer, linked);
                renderer.endFrame();
                if (!linkedPrepared) SDL_Delay(1);
            } while (!linkedPrepared && SDL_GetTicks()<deadline);
            PALADIN_CHECK(linkedPrepared);
            PALADIN_CHECK(linkedMap.terrainLocalDetailReady());
            linked.time().advanceMinutes(12 * 60);
            Camera2D linkedCamera(65, 38);
            linkedCamera.setWorldZoom(1);
            TileRenderMetrics linkedMetrics;
            linkedMetrics.tilePixels = 8;
            for (int i = 0; i < 16; ++i)
            {
                renderer.beginFrame();
                linkedMap.render(renderer, linked, linkedCamera, linkedMetrics);
            }
            saveWorldReview(
                native,
                std::string("tweak-territory-") + origin + ".png"
            );
        }
        {
            WorldGenerationSettings largeSettings;
            largeSettings.width = 600;
            largeSettings.height = 300;
            largeSettings.seed = 9202026;
            World large(largeSettings);
            AiRealmGenerator{}.generate(large);
            PALADIN_CHECK(large.settlementCount() > 20);
            WorldRealmPresentationRenderer themes;
            Camera2D overview(300, 150);
            overview.setWorldZoom(1);
            TileRenderMetrics overviewMetrics;
            overviewMetrics.tilePixels = 1.5;
            for (const auto mode :
                 {WorldMapMode::Political,
                  WorldMapMode::Government,
                  WorldMapMode::Population})
            {
                themes.configure(mode);
                bool ready = false;
                double worst = 0;
                const auto deadline = SDL_GetTicks() + 60000;
                do
                {
                    renderer.beginFrame();
                    const auto start = std::chrono::steady_clock::now();
                    ready = themes.prepare(renderer, large);
                    worst = std::max(
                        worst,
                        std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - start
                        )
                            .count()
                    );
                    renderer.endFrame();
                } while (!ready && SDL_GetTicks() < deadline);
                PALADIN_CHECK(ready);
                std::cout << "600x300 thematic preparation mode=" << int(mode)
                          << " worst_slice_ms=" << worst << '\n';
                if (mode != WorldMapMode::Political)
                {
                    PALADIN_CHECK(worst < 80);
                }
                renderer.beginFrame();
                renderer.fillRectangle(0, 0, 960, 640, PopulationWater);
                themes.renderFlat(
                    renderer,
                    large,
                    overview,
                    overviewMetrics,
                    worldPresentationState(1.5),
                    WorldPresentationPolicy{}
                );
                saveWorldReview(
                    native,
                    "next-generated-world-mode-" + std::to_string(int(mode)) +
                        ".png"
                );
                renderer.endFrame();
            }
            for (const auto mode :
                 {WorldMapMode::Political,
                  WorldMapMode::Government,
                  WorldMapMode::Population})
            {
                renderer.beginFrame();
                themes.configure(mode);
                PALADIN_CHECK(themes.prepare(renderer, large));
                renderer.endFrame();
            }
        }
        std::cout<<"PR25 world-surface, marker, map-mode and solar raster checks passed\n";
    }
} // namespace Paladin::Test

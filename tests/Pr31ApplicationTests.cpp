#include "TestFramework.h"
#include "core/Application.h"
#include "rendering/Renderer.h"
#include "rendering/SceneSpriteLibrary.h"
#include "platform/Window.h"
#include "core/SimulationClock.h"
#include "simulation/Simulation.h"
#include "simulation/WorldShipmentSystem.h"
#include "rendering/Camera2D.h"
#include "rendering/TileRenderMetrics.h"
#include "rendering/WorldRenderer.h"
#include "rendering/WorldMapNavigation.h"
#include "rendering/WorldRealmQuery.h"
#include "rendering/WorldCaravanPresentation.h"
#include "rendering/WorldThematicPalette.h"
#include "ui/CityHud.h"
#include "ui/CaravanPanel.h"
#include "ui/DiplomacyPanel.h"
#include "ui/WorldSettlementPanel.h"
#include "world/WorldPopulationField.h"
#include "world/generation/SettlementMapGenerator.h"
#include <cmath>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <filesystem>
#include <iostream>
namespace Paladin
{
    struct ApplicationSmokeTest
    {
        static void capture(Application& app, const char* name)
        {
            const char* root = SDL_getenv("PALADIN_SMOKE_SCREENSHOTS");
            if (!root) return;
            std::filesystem::create_directories(root);
            auto* surface = SDL_RenderReadPixels(SDL_GetRenderer(app.window_->nativeHandle()), nullptr);
            PALADIN_CHECK(surface);
            const bool saved = IMG_SavePNG(surface, (std::filesystem::path(root) / name).string().c_str());
            SDL_DestroySurface(surface);
            PALADIN_CHECK(saved);
        }
        static void startup(Application& app)
        {
            PALADIN_CHECK(app.startupReady_ && !app.startupCancelled_ && app.startupError_.empty());
            PALADIN_CHECK(app.startupProgress_ == 1 && app.startupProgressFrames_ >= 3);
            PALADIN_CHECK(!app.simulation_ && !app.worldRenderer_ && !app.cityRenderer_);
            capture(app, "pr31-startup-ready.png");
            const auto manager = app.renderer_->compiledAssets();
            PALADIN_CHECK(!manager->records().empty() && manager->uploads.empty());
            for (const auto& record : manager->records())
                PALADIN_CHECK(manager->residency(record.id) && manager->residency(record.id)->state == AssetState::Resident);
            const auto bytes = manager->residentGpuBytes();
            const auto cache = app.renderer_->sceneSpriteCache();
            PALADIN_CHECK(cache && cache->ready() && bytes > 0);
            SceneSpriteLibrary second;
            second.load(*app.renderer_, (std::filesystem::path(SDL_GetBasePath()) / "assets/sprites").string());
            PALADIN_CHECK(second.ready());
            const auto* first = cache->find("citizen.militia.male.front.walk");
            const auto* reused = second.find("citizen.militia.male.front.walk");
            PALADIN_CHECK(first && reused && first->texture == reused->texture);
            PALADIN_CHECK(first->selectionSilhouette == reused->selectionSilhouette);
            PALADIN_CHECK(manager == app.renderer_->compiledAssets() && manager->residentGpuBytes() == bytes);
            std::cout << "[pr31/startup] real progress, every packaged asset resident, shared GPU/silhouette cache, no generated world passed\n";
        }

        static void frame(Application& app)
        {
            app.simulationClock_->beginFrame();
            app.layoutFrame(); app.updateFrame(); app.renderFrame();
        }
        static void click(Application& app, float x, float y)
        {
            SDL_WarpMouseInWindow(app.window_->nativeHandle(), x, y);
            SDL_Event e{}; e.type=SDL_EVENT_MOUSE_BUTTON_DOWN; e.button.button=SDL_BUTTON_LEFT;
            e.button.x=x; e.button.y=y;
            PALADIN_CHECK(app.handleEvent(e,app.simulationControlsVisible()));
            e.type=SDL_EVENT_MOUSE_BUTTON_UP;
            PALADIN_CHECK(app.handleEvent(e,app.simulationControlsVisible()));
        }
        static void focus(Application& app, double x, double y, bool globe, double pixels)
        {
            const auto& grid=app.simulation_->world().grid();
            app.worldRenderer_->globeEnabled=globe;
            app.camera_->setPosition(x,y);
            app.camera_->setWorldZoom(globe ? pixels*grid.width()/(std::min(app.renderer_->outputWidth(),app.renderer_->outputHeight())*.4*6.283185307179586) : pixels/app.tileRenderMetrics_->tilePixels);
            if(globe) app.camera_->setPlanetRotation(GlobeView::orientationAt({x/grid.width(),y/grid.height()},.15),grid.width(),grid.height());
        }
        static void worldInteractions(Application& app)
        {
            app.startWorldSession();
            WorldGenerationSettings settings; settings.width=64; settings.height=64; settings.seed=313131; settings.populateAiRealms=false;
            app.simulation_=std::make_unique<Simulation>(settings);
            auto& sim=*app.simulation_; auto& world=sim.world();
            for(int y=0;y<64;++y) for(int x=0;x<64;++x)
            {
                auto& t=*world.grid().tile({x,y}); t.terrain=TerrainType::Land; t.biome=BiomeType::Plain; t.elevation=Elevation{.5};
                if(x>57) t.terrain=TerrainType::Water;
            }
            world.grid().terrainChanged();
            const auto home=sim.foundPlayerCapital({32,32},{"Marchland","March Folk","Home",{},"tribal",{}});
            PALADIN_CHECK(home);
            auto profile=defaultSettlementFoundationProfile(); profile.initialPopulation=4000; profile.initialDetailedCitizenCount=0;
            profile.initialResources={{"food",5000},{"lumber",1000}};
            const auto source=world.foundSettlement({8,12},sim.playerRealmId(),profile);
            const auto destination=world.foundSettlement({8,44},sim.playerRealmId(),profile);
            PALADIN_CHECK(source && destination);
            PALADIN_CHECK(world.renameSettlement(source,"North Store"));
            PALADIN_CHECK(world.renameSettlement(destination,"South Store"));
            SettlementMapGenerationSettings local; local.localTilesPerWorldTile=8;
            PALADIN_CHECK(sim.prepareSettlementMap(home,local));
            app.enterPresentedSettlement();
            PALADIN_CHECK(app.screen_==Application::Screen::City);
            app.returnToWorldFromSettlement();
            PALADIN_CHECK(app.simulationControlsVisible());
            sim.setSpeed(SimulationSpeed::Paused); app.simulationClock_->setPaused(true);
            const auto w=app.renderer_->outputWidth(), h=app.renderer_->outputHeight();
            focus(app,32.5,32.5,false,24);
            const auto deadline=SDL_GetTicks()+30000;
            do { frame(app); SDL_Delay(1); } while(!app.worldRenderer_->terrainDetailReady() && SDL_GetTicks()<deadline);
            PALADIN_CHECK(app.worldRenderer_->terrainDetailReady());
            ShipmentId route;
            PALADIN_CHECK(WorldShipmentSystem::create(world,sim.playerRealmId(),source,destination,"lumber",17,true,&route)==ShipmentResult::Success);
            WorldShipmentSystem::tick(world,360,27);
            PALADIN_CHECK(world.shipment(route)->cargo==17);
            for(bool globe:{false,true})
            {
                app.caravanPanel_->close();
                const auto* caravan=world.shipment(route);
                focus(app,caravan->visualX()+.5,caravan->visualY()+.5,globe,24); frame(app);
                const auto p=WorldMapNavigation::annotationPosition(*app.camera_,world.grid(),w,h,24,globe,caravan->visualX()+.5,caravan->visualY()+.5);
                PALADIN_CHECK(p);
                const auto box=worldCaravanBounds(p->x,p->y,24);
                click(app,box.x+box.width*.5F,box.y+box.height*.5F); frame(app);
                PALADIN_CHECK(app.caravanPanel_->selection()==route && !app.diplomacyPanel_->isOpen());
                PALADIN_CHECK(app.worldRenderer_->selectedCaravan==route && !app.globePointerDown_);
                PALADIN_CHECK(world.shipment(route)->cargo==17);
                capture(app,globe?"pr31-caravan-globe.png":"pr31-caravan-flat.png");
                // Inspecting/scrolling over a panel must never drag or zoom the globe.
                const auto b=app.caravanPanel_->bounds(); const double zoom=app.camera_->zoom();
                SDL_Event e{}; e.type=SDL_EVENT_MOUSE_WHEEL; e.wheel.y=1; e.wheel.mouse_x=b.x+60; e.wheel.mouse_y=b.y+100;
                PALADIN_CHECK(app.handleEvent(e,true)); PALADIN_CHECK(app.camera_->zoom()==zoom);
            }
            const auto b=app.caravanPanel_->bounds(); click(app,b.x+120,b.y+b.height-28); frame(app);
            PALADIN_CHECK(!world.shipment(route)->repeating && world.shipment(route)->cargo==17);
            app.caravanPanel_->close();
            for(bool globe:{false,true})
            {
                focus(app,12.5,44.5,globe,4); frame(app);
                // Public HUD bounds used by the same action as a real pointer click.
                const auto hud=app.cityHud_->activeSettlementBounds();
                click(app,hud.x+hud.width*.5F,hud.y+hud.height*.5F); frame(app);
                const double pixels=globe ? GlobeView::from(*app.camera_,world.grid(),w,h).radius*6.283185307179586/world.grid().width() : app.tileRenderMetrics_->tilePixels*app.camera_->zoom();
                PALADIN_CHECK(pixels>=31.99);
                const auto pos=WorldMapNavigation::annotationPosition(*app.camera_,world.grid(),w,h,pixels,globe,32.5,32.5);
                PALADIN_CHECK(pos && std::hypot(pos->x-w*.5,pos->y-h*.5)<3.0);
                PALADIN_CHECK(!app.diplomacyPanel_->isOpen());
            }
            const auto actor=sim.playerRealmId();
            for(const char* origin:{"tribal","civic"})
            {
                PALADIN_CHECK(world.editRealmIdentity(actor,{"Marchland","March Folk","Home",{},origin,{}}));
                for(bool globe:{false,true}) for(double pixels:{8.,20.,48.})
                {
                    app.diplomacyPanel_->close(); app.worldSettlementPanel_->close();
                    focus(app,32.5,32.5,globe,pixels); frame(app);
                    std::optional<std::pair<float,float>> background;
                    for(int dy=-8;dy<=8 && !background;++dy) for(int dx=-8;dx<=8 && !background;++dx)
                    {
                        const double x=32.5+dx,y=32.5+dy;
                        if(worldRealmAt(world,x,y)!=actor) continue;
                        const auto p=WorldMapNavigation::annotationPosition(*app.camera_,world.grid(),w,h,pixels,globe,x,y);
                        if(!p || p->x<310 || p->x>w-310 || p->y<190 || p->y>h-170 || app.activeHudContainsPoint(p->x,p->y)) continue;
                        bool clear=true;
                        for(const auto& city:world.settlements())
                        {
                            const auto c=WorldMapNavigation::annotationPosition(*app.camera_,world.grid(),w,h,pixels,globe,city.position().x+.5,city.position().y+.5);
                            if(c && std::abs(c->x-p->x)<15 && std::abs(c->y-p->y)<15) clear=false;
                        }
                        if(clear) background={{float(p->x),float(p->y)}};
                    }
                    if (!background) std::cerr<<"missing background "<<origin<<" globe="<<globe<<" pixels="<<pixels<<" window="<<w<<"x"<<h<<" center-owner="<<worldRealmAt(world,32.5,32.5).value()<<" actor="<<actor.value()<<"\n";
                    PALADIN_CHECK(background); click(app,background->first,background->second); frame(app);
                    PALADIN_CHECK(app.diplomacyPanel_->selection()==actor && app.worldRenderer_->selectedRealm==actor);
                    if(pixels==48) capture(app,(std::string("pr31-realm-")+origin+(globe?"-globe.png":"-flat.png")).c_str());
                }
            }
            app.diplomacyPanel_->close();
            WorldPopulationField field(world);
            double population=0; for(const auto& city:world.settlements()) population+=city.population();
            PALADIN_CHECK(std::abs(field.total()-population)<.05);
            PALADIN_CHECK(field.at({63,12})==0 && field.at({8,12})>field.at({52,52}));
            const auto zero=populationDensityColor(0),dense=populationDensityColor(2048);
            PALADIN_CHECK(zero.red>200 && zero.green>200 && zero.blue>200 && zero.alpha>=240);
            PALADIN_CHECK(dense.red>dense.green*2 && dense.red>dense.blue*2);
            app.worldRenderer_->setMapMode(WorldMapMode::Population);
            for(bool globe:{false,true})
            {
                focus(app,26.5,29.5,globe,10);
                for(int i=0;i<8;++i) frame(app);
                capture(app,globe?"pr31-density-globe.png":"pr31-density-flat.png");
            }
            app.worldRenderer_->setMapMode(WorldMapMode::Political);
            std::cout<<"[pr31/world] caravan pick/cargo/stop/no-click-through, active HUD focus, 12 tribal/civic zoom/projection selections, mass-conserving geographic density passed\n";
        }
        static void run()
        {
            Application app;
            PALADIN_CHECK(app.renderer_ && app.renderer_->isValid());
            startup(app);
            worldInteractions(app);
        }
    };
}
int main()
{
    try { Paladin::ApplicationSmokeTest::run(); return 0; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}

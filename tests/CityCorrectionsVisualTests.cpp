#include "CityCorrectionsChecks.h"
#include "platform/Window.h"
#include "rendering/CityRenderer.h"
#include "rendering/Camera2D.h"
#include "rendering/TileRenderMetrics.h"
#include "rendering/MineSurface.h"
#include "interaction/SettlementCommandController.h"
#include "interaction/SettlementInspectionController.h"
#include "interaction/SettlementObjectPlacementController.h"
#include "ui/GrayUiRenderer.h"
#include "ui/SettlementInspectionPanel.h"
#include "ui/TradeDepotPanel.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <filesystem>
#include <memory>

namespace
{
    using namespace Paladin;
    using namespace Paladin::Test::CityCorrections;
    std::vector<std::uint32_t> capture(SDL_Window* window, const std::string& name)
    {
        std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> raw(
            SDL_RenderReadPixels(SDL_GetRenderer(window),nullptr),SDL_DestroySurface);
        PALADIN_CHECK(raw);
        std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> image(
            SDL_ConvertSurface(raw.get(),SDL_PIXELFORMAT_RGBA32),SDL_DestroySurface);
        PALADIN_CHECK(image);
        if(const auto* directory=SDL_getenv("PALADIN_SMOKE_SCREENSHOTS"))
        {
            std::filesystem::create_directories(directory);
            PALADIN_CHECK(IMG_SavePNG(image.get(),(std::filesystem::path(directory)/name).string().c_str()));
        }
        std::vector<std::uint32_t> pixels;
        for(int y=0;y<image->h;++y)
        {
            const auto* row=reinterpret_cast<const std::uint32_t*>(
                static_cast<const std::uint8_t*>(image->pixels)+y*image->pitch);
            pixels.insert(pixels.end(),row,row+image->w);
        }
        return pixels;
    }
    void mineViews(Renderer& renderer, SDL_Window* window)
    {
        auto map=flatMap();
        std::vector<SettlementObjectId> mines;
        for(int i=0;i<4;++i)
        {
            const auto& job=MiningJobs[i];
            const SettlementObjectFootprint f{{7+(i%2)*19,7+(i/2)*19},9,9};
            for(int y=f.topLeft.y;y<f.topLeft.y+f.height;++y)
            for(int x=f.topLeft.x;x<f.topLeft.x+f.width;++x)
            { map.grid().tile({x,y})->mineral=job.deposit; }
            map.objectState().invalidateTerrainCache();
            mines.push_back(complete(map,job.type,f));
        }
        SettlementCitizenState people;
        SettlementObjectPlacementController placement;
        SettlementCommandController commands;
        SettlementInspectionController selection;
        TileRenderMetrics metrics;
        CityRenderer city;
        city.artRootOverride=std::string(PALADIN_TEST_SOURCE_ROOT)+"/assets/sprites";
        city.animationTimeOverride=42;
        city.presentation.cloudsEnabled=false;
        Camera2D camera(21,21); camera.setZoom(4);
        const auto draw=[&](std::string name,double hour)
        {
            for(int i=0;i<8;++i)
            {
                renderer.beginFrame();
                city.render(renderer,map,camera,metrics,placement,commands,people,selection,1,hour);
                if(i==7)
                { const auto pixels=capture(window,name); renderer.endFrame(); return pixels; }
                renderer.endFrame();
            }
            return std::vector<std::uint32_t>{};
        };
        draw("corrected-mines-new-day.png",12);
        for(auto id:mines)
        {
            const auto* object=map.objectState().completedObject(id);
            map.mining.work(map.grid(),*object,1,81*720,100000);
        }
        draw("corrected-mines-worked-day.png",12);
        draw("corrected-mines-worked-night.png",0);
        for(int i=0;i<4;++i)
        {
            const auto* object=map.objectState().completedObject(mines[i]);
            camera.setPosition(object->footprint.topLeft.x+4.5,object->footprint.topLeft.y+4.5);
            camera.setZoom(12);
            const auto still=draw("corrected-"+std::string(MiningJobs[i].type)+"-day.png",12);
            PALADIN_CHECK(still==draw("corrected-"+std::string(MiningJobs[i].type)+"-paused.png",12));
            draw("corrected-"+std::string(MiningJobs[i].type)+"-night.png",0);
        }
        SceneSpriteLibrary sprites; sprites.load(renderer,city.artRootOverride);
        const auto* object=map.objectState().completedObject(mines[0]);
        SceneDrawQueue a,b;
        const SceneProjection view{11.5,11.5,64,960,640};
        mineSurface(a,view,sprites,*object,object->id.value(),1);
        mineSurface(b,view,sprites,*object,object->id.value(),2);
        PALADIN_CHECK(a.size()==b.size());
        bool moved=false;
        for(std::size_t i=0;i<a.size();++i)
        { moved |= a.items()[i].bounds.y!=b.items()[i].bounds.y || a.items()[i].bounds.height!=b.items()[i].bounds.height; }
        PALADIN_CHECK(moved);
        std::cout << "[city-corrections/art] four mine types, close/normal day/night, frozen pause and working hoist\n";
    }
    void rangeViews(Renderer& renderer,SDL_Window* window)
    {
        WorldGrid source(5,5);
        for(int y=0;y<5;++y) for(int x=0;x<5;++x)
        {
            auto& t=*source.tile({x,y});
            t.terrain=x<3?TerrainType::Mountain:TerrainType::Land;
            t.relief=x<3?ReliefType::Mountain:ReliefType::Hills;
            t.biome=x<3?BiomeType::Plain:BiomeType::Hills;
            t.temperature=Temperature{.5};
        }
        SettlementMapGenerationSettings settings; settings.localTilesPerWorldTile=128;
        auto map=SettlementMapGenerator{}.generate(source,{2,2},3,3,42,settings);
        PALADIN_CHECK(map);
        CityRenderer city; city.animationTimeOverride=42; city.presentation.cloudsEnabled=false;
        city.artRootOverride=std::string(PALADIN_TEST_SOURCE_ROOT)+"/assets/sprites";
        SettlementCitizenState people; SettlementInspectionController selection;
        SettlementObjectPlacementController placement; SettlementCommandController commands;
        TileRenderMetrics metrics; Camera2D camera(192,192); camera.setZoom(.4);
        for(int warm=0;warm<20;++warm)
        {
            renderer.beginFrame();
            city.render(renderer,*map,camera,metrics,placement,commands,people,selection,1,12);
            if(warm==19) capture(window,"corrected-city-ranges-overview.png");
            renderer.endFrame();
        }
        std::cout << "[city-corrections/art] mixed mountain/hill overview from actual city generator\n";
    }
    void orderUi(Renderer& renderer, SDL_Window* window)
    {
        TradeFixture f;
        auto& world=f.sim.world(); auto& map=*f.map;
        auto& people=world.settlement(f.home)->simulationState().citizens();
        map.employment().synchronize(map.objectState(),people);
        SettlementInspectionController selection; selection.selectWorkplace(f.depot,{});
        SettlementInspectionPanel inspector; TradeDepotPanel panel; GrayUiRenderer ui;
        Camera2D camera(15,15); camera.setZoom(3); TileRenderMetrics metrics;
        panel.open(f.home,f.depot);
        const auto layout=[&]()
        {
            renderer.beginFrame(); renderer.fillRectangle(0,0,960,640,{73,151,91,255});
            inspector.render(renderer,ui,selection,map,people,camera,metrics);
            panel.embed(inspector.tradeContentBounds(),inspector.tradeOrdersBounds());
            panel.layout(960,640,world,f.seller);
        };
        layout(); renderer.endFrame();
        const auto bounds=inspector.tradeContentBounds();
        const float x=bounds.x+14, y=bounds.y, span=bounds.width-28, scale=bounds.height/515;
        const auto click=[&](float xx,float yy)
        {
            SDL_Event event{}; event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;
            event.button.button=SDL_BUTTON_LEFT; event.button.x=xx; event.button.y=yy;
            PALADIN_CHECK(panel.handle(event,world,f.seller));
            event.type=SDL_EVENT_MOUSE_BUTTON_UP;
            PALADIN_CHECK(panel.handle(event,world,f.seller));
        };
        const auto resources=SettlementResourceCatalog::definitions();
        const int columns=int((resources.size()+1)/2); const float cell=(span-3*(columns-1))/columns;
        std::size_t iron=0; for(;iron<resources.size() && resources[iron].id!="iron";++iron) {}
        PALADIN_CHECK(iron<resources.size());
        click(x+float(iron%columns)*(cell+3)+cell/2,y+(24+float(iron/columns)*49+22)*scale);
        click(x+span*.75F,y+145*scale); // export
        for(int n=0;n<8;++n) click(x+57,y+199*scale); // 10 -> 2, real button routing
        click(x+span*.16F,y+431*scale);
        PALADIN_CHECK(map.trade.orders.size()==1);
        PALADIN_CHECK(map.trade.orders[0].resource=="iron" && map.trade.orders[0].quantity==2);
        PALADIN_CHECK(!map.trade.orders[0].standing);
        click(x+span*.50F,y+431*scale);
        PALADIN_CHECK(map.trade.orders.size()==2 && map.trade.orders[1].standing);
        WorldMarketSystem::tickOrders(world,0);
        layout(); panel.render(renderer,ui,world,f.seller);
        capture(window,"corrected-depot-orders.png"); renderer.endFrame();
        const auto left=inspector.tradeOrdersBounds();
        PALADIN_CHECK(left.width>100 && left.height>180);
        const auto survivor=map.trade.orders[1].id;
        click(left.x+left.width-30,left.y+25+18);
        PALADIN_CHECK(map.trade.orders.size()==1 && map.trade.orders[0].id==survivor);
        layout(); panel.render(renderer,ui,world,f.seller);
        capture(window,"corrected-depot-cancelled.png"); renderer.endFrame();
        // Reflow cannot turn a stale press into cancellation of a different order.
        SDL_Event e{}; e.type=SDL_EVENT_MOUSE_BUTTON_DOWN; e.button.button=SDL_BUTTON_LEFT;
        e.button.x=left.x+left.width-30; e.button.y=left.y+43;
        PALADIN_CHECK(panel.handle(e,world,f.seller));
        PALADIN_CHECK(WorldMarketSystem::cancelOrder(world,f.seller,f.home,f.depot,survivor));
        PALADIN_CHECK(WorldMarketSystem::placeOrder(world,f.seller,f.home,f.depot,"stone",TradeDirection::Import,5,false));
        layout(); renderer.endFrame();
        e.type=SDL_EVENT_MOUSE_BUTTON_UP; panel.handle(e,world,f.seller);
        PALADIN_CHECK(map.trade.orders.size()==1 && map.trade.orders[0].resource=="stone");
        std::cout << "[city-corrections/ui] real order buttons, amount 2, one-shot/standing, cancellation and stale-click safety\n";
    }
}
int main()
{
    if(!SDL_Init(SDL_INIT_VIDEO)) { std::cerr<<SDL_GetError()<<'\n'; return 1; }
    int result=0;
    try
    {
        Window window("Paladin city corrections",960,640); PALADIN_CHECK(window.isValid());
        SDL_HideWindow(window.nativeHandle());
        Renderer renderer(window.nativeHandle()); PALADIN_CHECK(renderer.isValid());
        mineViews(renderer,window.nativeHandle());
        rangeViews(renderer,window.nativeHandle());
        orderUi(renderer,window.nativeHandle());
    }
    catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; result=1; }
    SDL_Quit(); return result;
}

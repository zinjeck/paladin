#include "TestFramework.h"
#include "platform/Window.h"
#include "rendering/Camera2D.h"
#include "rendering/Renderer.h"
#include "rendering/SceneSpriteLibrary.h"
#include "rendering/SettlementCitizenRenderer.h"
#include "rendering/TileRenderMetrics.h"
#include "rendering/WorldObjectRenderer.h"
#include "rendering/WorldPixelGrid.h"
#include "simulation/MilitarySystem.h"
#include "simulation/Simulation.h"
#include "ui/GrayUiRenderer.h"
#include "ui/MilitaryPanel.h"
#include "world/World.h"
#include "world/settlements/SettlementMap.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <filesystem>
#include <iostream>
#include <memory>
#include <set>
#include <vector>

namespace
{
    using namespace Paladin;
    using Surface = std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>;
    std::vector<std::uint32_t> capture(SDL_Window* window, const std::string& name)
    {
        Surface raw(SDL_RenderReadPixels(SDL_GetRenderer(window), nullptr), SDL_DestroySurface);
        PALADIN_CHECK(raw);
        Surface rgba(SDL_ConvertSurface(raw.get(), SDL_PIXELFORMAT_RGBA32), SDL_DestroySurface);
        PALADIN_CHECK(rgba);
        if (const char* root = SDL_getenv("PALADIN_SMOKE_SCREENSHOTS"))
        {
            std::filesystem::create_directories(root);
            PALADIN_CHECK(IMG_SavePNG(rgba.get(), (std::filesystem::path(root) / name).string().c_str()));
        }
        std::vector<std::uint32_t> result;
        for (int y = 0; y < rgba->h; ++y)
        {
            const auto* row = reinterpret_cast<const std::uint32_t*>(
                static_cast<const Uint8*>(rgba->pixels) + y*rgba->pitch);
            result.insert(result.end(), row, row+rgba->w);
        }
        return result;
    }
    void requireBlocks(const std::vector<std::uint32_t>& pixels, int width, int height, int pitch)
    {
        std::set<std::uint32_t> colors;
        for (int y=0;y<height;y+=pitch)
            for (int x=0;x<width;x+=pitch)
            {
                const auto value=pixels[y*width+x]; colors.insert(value);
                for (int dy=0;dy<pitch && y+dy<height;++dy)
                    for (int dx=0;dx<pitch && x+dx<width;++dx)
                        PALADIN_CHECK(pixels[(y+dy)*width+x+dx] == value);
            }
        PALADIN_CHECK(colors.size()>8);
    }
    void actors(Renderer& renderer, SDL_Window* window, SceneSpriteLibrary& art)
    {
        SettlementCitizenState people;
        PALADIN_CHECK(people.initialize(1,47));
        auto& person=const_cast<SettlementCitizen&>(people.citizens().front());
        person.tilePosition={10,10}; person.insideHome=false; person.sex=CitizenSex::Male;
        person.hasVisualSnapshot=false;
        Camera2D camera(10.5,10.5); camera.setZoom(16);
        TileRenderMetrics metrics;
        SettlementCitizenRenderer citizens;
        art.setTime(42);
        const auto render=[&](const std::string& name)
        {
            renderer.beginFrame();
            {
                WorldPixelScene scene(renderer,metrics.scaledTilePixels(camera.zoom()));
                renderer.fillRectangle(0,0,960,640,{32,44,67,255});
                citizens.render(renderer,people,camera,metrics,nullptr,1,nullptr,&art);
                citizens.renderAnnotations(renderer,metrics.scaledTilePixels(camera.zoom()));
            }
            const auto result=capture(window,name);
            renderer.endFrame();
            return result;
        };
        person.path={{11,10}};
        std::vector<std::vector<std::uint32_t>> walk;
        for(int frame=0;frame<4;++frame)
        {
            person.walkDistance=frame*.25;
            walk.push_back(render("pr26-walk-"+std::to_string(frame)+".png"));
            requireBlocks(walk.back(),960,640,4);
            for(int previous=0;previous<frame;++previous) PALADIN_CHECK(walk[previous]!=walk.back());
        }
        PALADIN_CHECK(render("pr26-walk-paused.png")==walk.back());
        person.path.clear(); person.task.kind=CitizenTaskKind::Gather;
        std::vector<std::vector<std::uint32_t>> gather;
        for(int frame=0;frame<4;++frame)
        {
            person.workAnimationMinutes=frame*2;
            gather.push_back(render("pr26-gather-"+std::to_string(frame)+".png"));
            requireBlocks(gather.back(),960,640,4);
            for(int previous=0;previous<frame;++previous) PALADIN_CHECK(gather[previous]!=gather.back());
        }
        person.task={}; person.activity=CitizenActivity::Sleeping;
        const auto sleep=render("pr26-sleep-close.png"); requireBlocks(sleep,960,640,4);
        PALADIN_CHECK(render("pr26-sleep-paused.png")==sleep);
        camera.setZoom(4); render("pr26-sleep-normal.png");
        person.activity=CitizenActivity::Idle; person.soldierId=SoldierId{1};
        camera.setZoom(16);
        const auto soldier=render("pr26-soldier-close.png");
        person.soldierId={}; const auto civilian=render("pr26-citizen-close.png");
        PALADIN_CHECK(soldier!=civilian);
        person.militaryDeployed=true;
        const auto absent=render("pr26-deployed-hidden.png");
        PALADIN_CHECK(std::all_of(absent.begin(),absent.end(),[&](auto p){return p==absent.front();}));
    }
    void militaryAndCities(Renderer& renderer, SDL_Window* window, SceneSpriteLibrary& art)
    {
        WorldGenerationSettings settings;
        settings.width=settings.height=64; settings.seed=711; settings.populateAiRealms=false;
        Simulation sim(settings);
        auto& world=sim.world();
        for(int y=0;y<64;++y) for(int x=0;x<64;++x)
        {
            auto& tile=*world.grid().tile({x,y});
            tile.terrain=TerrainType::Land; tile.biome=BiomeType::Plain;
        }
        const auto city=sim.foundPlayerCapital({32,32},{"Art Realm","Art Folk","Caerwyn Haven",{},"civic",{}});
        PALADIN_CHECK(city);
        SettlementMapGenerationSettings local; local.localTilesPerWorldTile=4;
        PALADIN_CHECK(sim.prepareSettlementMap(city,local));
        auto& map=*sim.settlementMap(city);
        for(int y=0;y<map.grid().height();++y) for(int x=0;x<map.grid().width();++x)
            map.grid().tile({x,y})->terrain=TerrainType::Land;
        for(const auto type : {"city_keep","barracks"})
        {
            auto definition=*SettlementObjectCatalog::definition(type); definition.bypassesConstruction=true;
            const SettlementObjectFootprint bounds=type==std::string_view("city_keep")
                ? SettlementObjectFootprint{{2,2},5,7} : SettlementObjectFootprint{{11,2},5,5};
            PALADIN_CHECK(map.objectState().placeCompletedObject(map.grid(),definition,bounds));
            map.naturalFeatures().clear(bounds);
        }
        map.logistics.synchronize(map.objectState(),360);
        world.settlement(city)->simulationState().citizens().placeUnpositionedCitizens(map);
        MilitaryPanel panel; GrayUiRenderer ui;
        BitmapFontRenderer font;
        PALADIN_CHECK(font.measureWidth("iii",2) < font.measureWidth("MMM",2));
        panel.toggle(city); panel.layout(320,360,world,sim.playerRealmId());
        PALADIN_CHECK(panel.bounds().x + panel.bounds().width <= 320);
        PALADIN_CHECK(panel.bounds().y + panel.bounds().height <= 360);
        panel.close();
        panel.toggle(city); panel.layout(960,640,world,sim.playerRealmId());
        const auto bounds=panel.bounds();
        const float x=bounds.x+16,inner=bounds.width-32;
        const float tools=bounds.y+104+std::max(48.F,bounds.height-316)+8;
        const auto click=[&](float px,float py)
        {
            SDL_Event event{}; event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;
            event.button.button=SDL_BUTTON_LEFT; event.button.x=px; event.button.y=py;
            PALADIN_CHECK(panel.handle(event,world,sim.playerRealmId()));
            event.type=SDL_EVENT_MOUSE_BUTTON_UP;
            PALADIN_CHECK(panel.handle(event,world,sim.playerRealmId()));
        };
        click(x+16,tools+15); const auto unit=panel.selection(); PALADIN_CHECK(unit);
        click(x+(inner-12)/3+22,tools+15); click(x+(inner-12)/3+22,tools+15);
        click(x+(inner-18)/4+22,tools+93);
        PALADIN_CHECK(world.army(unit)->soldierCount()==2);
        renderer.beginFrame(); renderer.fillRectangle(0,0,960,640,{35,87,71,255});
        panel.render(renderer,ui,world,sim.playerRealmId());
        capture(window,"pr26-military-panel.png"); renderer.endFrame();
        click(x+2*((inner-18)/4+6)+16,tools+93);
        PALADIN_CHECK(world.army(unit)->soldierCount()==1);
        click(x+inner*.5F+16,bounds.y+59); // local tab
        click(x+16,bounds.y+59); // world tab
        click(x+inner*.5F+16,tools+129); // show selected unit on world map
        PALADIN_CHECK(panel.takeFocus()==unit && !panel.isOpen());
        // Sprite stages are presentation derived from actual population.
        WorldObjectRenderer objects; Camera2D camera(32.5,33.5); camera.setWorldZoom(16);
        const auto worldFrame=[&](const std::string& name, const SceneSpriteLibrary* library)
        {
            renderer.beginFrame(); renderer.fillRectangle(0,0,960,640,{35,87,71,255});
            objects.render(renderer,world,camera,64,false,worldPresentationState(64),true,{},std::nullopt,{},library);
            const auto pixels=capture(window,name); renderer.endFrame(); return pixels;
        };
        const auto fallback=worldFrame("pr26-world-without-art.png",nullptr);
        auto settlement=worldFrame("pr26-world-settlement.png",&art); PALADIN_CHECK(settlement!=fallback);
        PALADIN_CHECK(world.settlement(city)->simulationState().spawnCitizens(120));
        auto town=worldFrame("pr26-world-town.png",&art); PALADIN_CHECK(town!=settlement);
        PALADIN_CHECK(world.settlement(city)->simulationState().spawnCitizens(896));
        auto cityPixels=worldFrame("pr26-world-city.png",&art); PALADIN_CHECK(cityPixels!=town);
        PALADIN_CHECK(MilitarySystem::orderMove(world,sim.playerRealmId(),unit,{33,32})==MilitaryResult::Success);
        MilitarySystem::tick(world,360,8);
        const auto march=worldFrame("pr26-world-marching.png",&art);
        PALADIN_CHECK(worldFrame("pr26-world-marching-paused.png",&art)==march);
        renderer.beginFrame(); renderer.fillRectangle(0,0,960,640,{32,44,67,255});
        ui.drawPanel(renderer,{80,60,800,520});
        ui.drawLabel(renderer,"Paladin | Population | Realm",112,92,3);
        ui.drawLabel(renderer,"Il1 O0 S5 B8 rn m   0123456789",112,140,2);
        for(int state=0;state<5;++state)
            ui.drawButton(renderer,{112.F,196.F+64*state,736,44},"Build: Housing 20 Wood",state==1,state==2,state==3,state!=4);
        capture(window,"pr26-ui-states.png"); renderer.endFrame();
    }
}
int main()
{
    if(!SDL_Init(SDL_INIT_VIDEO)) { std::cerr<<SDL_GetError()<<'\n'; return 1; }
    int result=0;
    try
    {
        Window window("Paladin military and animation verification",960,640);
        PALADIN_CHECK(window.isValid()); SDL_HideWindow(window.nativeHandle());
        Renderer renderer(window.nativeHandle()); PALADIN_CHECK(renderer.isValid());
        SceneSpriteLibrary art; art.load(renderer,std::string(PALADIN_TEST_SOURCE_ROOT)+"/assets/sprites");
        PALADIN_CHECK(art.find("citizen.militia.male.front.walk"));
        actors(renderer,window.nativeHandle(),art);
        militaryAndCities(renderer,window.nativeHandle(),art);
        std::cout<<"Military UI, live city tiers, walking/gathering, sleep pixel blocks and pause checks passed.\n";
    }
    catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; result=1; }
    SDL_Quit(); return result;
}

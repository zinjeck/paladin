#include "TestFramework.h"
#include "platform/Window.h"
#include "rendering/Camera2D.h"
#include "rendering/Renderer.h"
#include "rendering/SceneSpriteLibrary.h"
#include "rendering/SettlementCitizenRenderer.h"
#include "rendering/TileRenderMetrics.h"
#include "rendering/WorldObjectRenderer.h"
#include "rendering/WorldArmyPresentation.h"
#include "rendering/GlobeView.h"
#include "ui/CityHud.h"
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
        const auto click=[&](MilitaryPanel::Action action, ArmyId id=ArmyId{})
        {
            const auto b=panel.controlBounds(action,id); PALADIN_CHECK(b);
            SDL_Event event{}; event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;
            event.button.button=SDL_BUTTON_LEFT;
            event.button.x=b->x+b->width*.5F; event.button.y=b->y+b->height*.5F;
            PALADIN_CHECK(panel.handle(event,world,sim.playerRealmId()));
            event.type=SDL_EVENT_MOUSE_BUTTON_UP;
            PALADIN_CHECK(panel.handle(event,world,sim.playerRealmId()));
        };
        using A=MilitaryPanel::Action;
        click(A::New); PALADIN_CHECK(!panel.selection() && world.armies().empty());
        click(A::Hire); click(A::Hire);
        PALADIN_CHECK(MilitarySystem::available(world,city)==2);
        click(A::New); const auto unit=panel.selection(); PALADIN_CHECK(unit);
        PALADIN_CHECK(world.army(unit)->soldierCount()==1);
        click(A::AddFive); PALADIN_CHECK(world.army(unit)->soldierCount()==2);
        renderer.beginFrame(); renderer.fillRectangle(0,0,960,640,{35,87,71,255});
        panel.render(renderer,ui,world,sim.playerRealmId(),&art);
        capture(window,"pr26-military-panel.png"); renderer.endFrame();
        click(A::RemoveOne); PALADIN_CHECK(world.army(unit)->soldierCount()==1);
        click(A::New); const auto second=panel.selection(); PALADIN_CHECK(second && second!=unit);
        const auto firstCard=panel.controlBounds(A::Row,unit), secondCard=panel.controlBounds(A::Row,second);
        PALADIN_CHECK(firstCard && secondCard && firstCard->height>firstCard->width);
        PALADIN_CHECK(firstCard->x==secondCard->x && firstCard->y+firstCard->height<secondCard->y);
        renderer.beginFrame(); renderer.fillRectangle(0,0,960,640,{35,87,71,255});
        panel.render(renderer,ui,world,sim.playerRealmId(),&art);
        capture(window,"pr27-portrait-unit-column.png"); renderer.endFrame();
        click(A::Disband); PALADIN_CHECK(!world.army(second));
        click(A::Row,unit); click(A::Local); click(A::World); click(A::Focus);
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
        PALADIN_CHECK(MilitarySystem::resizeUnit(world,sim.playerRealmId(),unit,1)==MilitaryResult::Success);
        const auto two=worldFrame("pr27-one-character-two-soldiers.png",&art);
        bool labelChanged=false;
        const auto countPlate=worldArmyCountBounds(480,256,2);
        for (int y=0;y<640;++y) for (int x=0;x<960;++x)
        {
            if (countPlate.contains(float(x),float(y))) labelChanged |= two[y*960+x]!=settlement[y*960+x];
            else PALADIN_CHECK(two[y*960+x]==settlement[y*960+x]);
        }
        PALADIN_CHECK(labelChanged);
        for (const double scale : {.25,4.,16.,64.,256.})
        {
            const auto* sprite=worldArmySprite(art,world,*world.army(unit));
            const auto body=worldArmySpriteBounds(480,320,scale,sprite);
            PALADIN_CHECK(body.height>=24.F);
            PALADIN_CHECK(worldArmyHitTest(body.x+body.width*.5,body.y+3,480,320,scale,sprite,2));
            PALADIN_CHECK(worldArmyHitTest(480,335,480,320,scale,sprite,2));
            PALADIN_CHECK(!worldArmyHitTest(5,5,480,320,scale,sprite,2));
        }
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
    void selectionAndConstruction(Renderer& renderer, SDL_Window* window)
    {
        GrayUiRenderer ui;
        const auto verify=[&](bool skin)
        {
            renderer.beginFrame(); renderer.fillRectangle(0,0,960,640,{32,44,67,255});
            const UiRectangle button{70,100,252,70}, card{360,100,252,70}, pressed{70,220,252,70};
            ui.drawButton(renderer,button,"Tribal",false,false,true,true);
            ui.drawChoiceCard(renderer,card,"Civic",true,false,true);
            ui.drawButton(renderer,pressed,"Selected + pressed",true,true,true,true);
            const auto pixels=capture(window,skin?"pr27-selection-skinned.png":"pr27-selection-fallback.png");
            renderer.endFrame();
            for (const auto b : {button,card,pressed})
            {
                const auto gold=[&](int x,int y)
                {
                    const auto* rgb=reinterpret_cast<const Uint8*>(&pixels[y*960+x]);
                    PALADIN_CHECK(rgb[0]==235 && rgb[1]==196 && rgb[2]==107);
                };
                for (int x=int(b.x);x<int(b.x+b.width);++x)
                { gold(x,int(b.y)); gold(x,int(b.y+b.height)-1); }
                for (int y=int(b.y);y<int(b.y+b.height);++y)
                { gold(int(b.x),y); gold(int(b.x+b.width)-1,y); }
            }
        };
        verify(false);
        std::vector<RenderColor> skinPixels(24*24,{8,15,27,255});
        ButtonSpriteSkin skin;
        skin.atlas=renderer.createTextureFromPixels(24,24,skinPixels);
        for (auto& frame:skin.frames) frame={0,0,24,24};
        skin.sliceBorder=4;
        ui.setButtonSkin("default",skin); verify(true);
        CityHud hud;
        for (const bool fortress : {false,true})
        {
            hud.setFortress(fortress); hud.setSettlementStatus(true,8); hud.layout(960,640);
            const float x=(960.F-76.F*(fortress?3:7))*.5F+38;
            const auto press=[&](float px,float py)
            { PALADIN_CHECK(hud.pointerPressed(px,py)); return hud.pointerReleased(px,py); };
            press(x,608);
            renderer.beginFrame(); renderer.fillRectangle(0,0,960,640,{35,87,71,255});
            hud.render(renderer,ui);
            capture(window,fortress?"pr27-fortress-rule-buildings.png":"pr27-rule-buildings.png"); renderer.endFrame();
            PALADIN_CHECK(hud.containsInteractivePoint(x,468));
            PALADIN_CHECK(press(x,468)==CityHudAction::BeginObjectPlacement);
            PALADIN_CHECK(hud.selectedObjectTypeId()==SettlementObjectTypes::Barracks);
            hud.closeCategoryMenus(); press(x,608);
            PALADIN_CHECK(press(x,398)==CityHudAction::BeginObjectPlacement);
            PALADIN_CHECK(hud.selectedObjectTypeId()==SettlementObjectTypes::ArmySupplyDepot);
            hud.closeCategoryMenus();
        }
    }
    void uprightSettlements(Renderer& renderer, SDL_Window* window, SceneSpriteLibrary& art)
    {
        WorldGenerationSettings settings;
        settings.width=settings.height=64; settings.seed=711; settings.populateAiRealms=false;
        Simulation sim(settings); auto& world=sim.world();
        for(int y=0;y<64;++y) for(int x=0;x<64;++x) world.grid().tile({x,y})->terrain=TerrainType::Land;
        const auto city=sim.foundPlayerCapital({32,32},{"Roll Realm","Roll Folk","Orientation",{},"civic",{}});
        PALADIN_CHECK(city);
        WorldObjectRenderer objects;
        constexpr double pi=3.14159265358979323846;
        for (const WorldTilePosition location : {WorldTilePosition{32,32},{1,12},{62,49},{30,1},{30,62}})
        {
            PALADIN_CHECK(world.setSettlementPosition(city,location));
            for (const double pixels : {16.,39.95,40.,64.,127.5,256.})
            {
                std::vector<std::uint32_t> reference;
                for (const double roll : {0.,.37,pi*.5,pi,-pi+.00001})
                {
                    Camera2D camera(location.x+.5,location.y+.5);
                    camera.setWorldZoom(pixels*64/(640*.40*2*pi));
                    camera.setPlanetRotation(GlobeView::orientationAt({(location.x+.5)/64.,(location.y+.5)/64.},roll),64,64);
                    renderer.beginFrame(); renderer.fillRectangle(0,0,960,640,{35,87,71,255});
                    objects.render(renderer,world,camera,pixels,true,worldPresentationState(pixels),true,{},std::nullopt,{},&art);
                    const auto image=capture(window,"pr27-upright-"+std::to_string(location.x)+"-"+std::to_string(location.y)+"-"+std::to_string(int(pixels))+"-"+std::to_string(roll)+".png");
                    renderer.endFrame();
                    if (reference.empty()) reference=image;
                    else PALADIN_CHECK(reference==image);
                }
            }
        }
        // The former .999 local-surface switch must not rotate sprite art.
        Camera2D camera(30.5,62.5); camera.setWorldZoom(64*64/(640*.40*2*pi));
        camera.setPlanetRotation(GlobeView::orientationAt({30.5/64,62.5/64},pi),64,64);
        std::vector<std::uint32_t> reference;
        for (const float local : {.9989F,.9991F,1.F})
        {
            auto p=worldPresentationState(64); p.localWorldWeight=local; p.regionalWeight=1-local;
            renderer.beginFrame(); renderer.fillRectangle(0,0,960,640,{35,87,71,255});
            objects.render(renderer,world,camera,64,true,p,true,{},std::nullopt,{},&art);
            const auto image=capture(window,"pr27-billboard-transition-"+std::to_string(local)+".png"); renderer.endFrame();
            if(reference.empty()) reference=image; else PALADIN_CHECK(reference==image);
        }
        std::cout<<"Upright world art: five geographic regions, six zooms, five rolls and former flip threshold passed.\n";
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
        selectionAndConstruction(renderer,window.nativeHandle());
        uprightSettlements(renderer,window.nativeHandle(),art);
        std::cout<<"Military UI, live city tiers, walking/gathering, sleep pixel blocks and pause checks passed.\n";
    }
    catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; result=1; }
    SDL_Quit(); return result;
}

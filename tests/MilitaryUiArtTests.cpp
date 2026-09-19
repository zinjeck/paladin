#include <limits>
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
#include "simulation/DiplomacySystem.h"
#include "simulation/Simulation.h"
#include "ui/GrayUiRenderer.h"
#include "ui/MilitaryPanel.h"
#include "ui/DiplomacyPanel.h"
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
        PALADIN_CHECK(firstCard->y==secondCard->y && firstCard->x+firstCard->width<secondCard->x);
        renderer.beginFrame(); renderer.fillRectangle(0,0,960,640,{35,87,71,255});
        panel.render(renderer,ui,world,sim.playerRealmId(),&art);
        capture(window,"pr29-compact-unit-grid.png"); renderer.endFrame();
        click(A::Disband); PALADIN_CHECK(!world.army(second));
        // PR30 discharge returns an unemployed civilian, not an automatic
        // barracks reserve. Rehire explicitly for the count-only render check.
        PALADIN_CHECK(MilitarySystem::recruit(world,sim.playerRealmId(),city,1)==MilitaryResult::Success);
        click(A::Row,unit); click(A::Local); click(A::World); click(A::Focus);
        PALADIN_CHECK(panel.takeFocus()==unit && !panel.isOpen());
        // Universal markers do not grow or acquire sprawl with population.
        WorldObjectRenderer objects; Camera2D camera(32.5,33.5); camera.setWorldZoom(16);
        const auto worldFrame=[&](const std::string& name, const SceneSpriteLibrary* library, ArmyId selected=ArmyId{})
        {
            renderer.beginFrame(); renderer.fillRectangle(0,0,960,640,{35,87,71,255});
            objects.render(renderer,world,camera,64,false,worldPresentationState(64),true,{},std::nullopt,{},library,selected);
            const auto pixels=capture(window,name); renderer.endFrame(); return pixels;
        };
        const auto fallback=worldFrame("pr26-world-without-art.png",nullptr);
        auto settlement=worldFrame("pr26-world-settlement.png",&art); PALADIN_CHECK(settlement!=fallback);
        PALADIN_CHECK(MilitarySystem::resizeUnit(world,sim.playerRealmId(),unit,1)==MilitaryResult::Success);
        const auto two=worldFrame("pr27-one-character-two-soldiers.png",&art);
        bool labelChanged=false;
        const auto countPlate=worldArmyCountBounds(480,256,2,64,worldArmySprite(art,world,*world.army(unit)));
        for (int y=0;y<640;++y) for (int x=0;x<960;++x)
        {
            if (countPlate.contains(float(x),float(y))) labelChanged |= two[y*960+x]!=settlement[y*960+x];
            else PALADIN_CHECK(two[y*960+x]==settlement[y*960+x]);
        }
        PALADIN_CHECK(labelChanged);
        const auto unselected=worldFrame("pr29-army-unselected.png",&art);
        const auto selected=worldFrame("pr29-army-selected.png",&art,unit);
        const auto body=worldArmySpriteBounds(480,256,64,worldArmySprite(art,world,*world.army(unit)));
        std::size_t contour=0;
        for(int y=0;y<640;++y) for(int x=0;x<960;++x)
            if(unselected[y*960+x]!=selected[y*960+x])
            {
                ++contour;
                PALADIN_CHECK(x>=body.x-5 && x<=body.x+body.width+5 && y>=body.y-5 && y<=body.y+body.height+5);
                // No rectangle spanning the bounding box: transparent sprite
                // corners remain untouched by the selected contour.
                PALADIN_CHECK(!(x<body.x+3 && y<body.y+3));
            }
        PALADIN_CHECK(contour>20);
        std::vector<std::uint32_t> far;
        for(const auto* library:{static_cast<const SceneSpriteLibrary*>(nullptr),static_cast<const SceneSpriteLibrary*>(&art)})
        {
            renderer.beginFrame(); renderer.fillRectangle(0,0,960,640,{35,87,71,255});
            objects.render(renderer,world,camera,4,false,worldPresentationState(4),true,{},std::nullopt,{},library,unit);
            const auto image=capture(window,"pr29-army-far.png"); renderer.endFrame();
            if(far.empty()) far=image; else PALADIN_CHECK(far==image);
        }
        for (const double scale : {.25,4.,16.,64.,256.})
        {
            const auto* sprite=worldArmySprite(art,world,*world.army(unit));
            const auto body=worldArmySpriteBounds(480,320,scale,sprite);
            PALADIN_CHECK(body.height>=36.F);
            const bool visible=worldArmyVisibility(scale)>.001F;
            const auto plate=worldArmyCountBounds(480,320,2,scale,sprite);
            PALADIN_CHECK(worldArmyHitTest(body.x+body.width*.5,body.y+3,480,320,scale,sprite,2)==visible);
            PALADIN_CHECK(worldArmyHitTest(480,plate.y+plate.height*.5,480,320,scale,sprite,2)==visible);
            PALADIN_CHECK(!worldArmyHitTest(5,5,480,320,scale,sprite,2));
        }
        PALADIN_CHECK(world.settlement(city)->simulationState().spawnCitizens(120));
        auto town=worldFrame("pr26-world-town.png",&art); PALADIN_CHECK(town==two);
        PALADIN_CHECK(world.settlement(city)->simulationState().spawnCitizens(896));
        auto cityPixels=worldFrame("pr26-world-city.png",&art); PALADIN_CHECK(cityPixels==town);
        auto fortressProfile=defaultSettlementFoundationProfile(); fortressProfile.kind=SettlementKind::Fortress;
        const auto fortress=world.foundSettlement({48,32},sim.playerRealmId(),fortressProfile);
        PALADIN_CHECK(fortress && world.renameSettlement(fortress,"Westgate"));
        camera.setPosition(48.5,33.5);
        const auto fortImage=worldFrame("pr29-fortress-icon.png",&art);
        PALADIN_CHECK(world.settlement(fortress)->simulationState().spawnCitizens(900));
        PALADIN_CHECK(worldFrame("pr29-fortress-population-invariant.png",&art)==fortImage);
        camera.setPosition(32.5,33.5);
        PALADIN_CHECK(MilitarySystem::orderMove(world,sim.playerRealmId(),unit,{33,32})==MilitaryResult::Success);
        MilitarySystem::tick(world,360,Army::MarchMinutesPerTile*.5);
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
    void diplomacyAndOverflow(Renderer& renderer, SDL_Window* window, SceneSpriteLibrary& art)
    {
        WorldGenerationSettings settings; settings.width=128; settings.height=64; settings.seed=573; settings.populateAiRealms=false;
        World world(settings);
        for(int y=0;y<world.grid().height();++y) for(int x=0;x<world.grid().width();++x) { world.grid().tile({x,y})->terrain=TerrainType::Land; world.grid().tile({x,y})->biome=BiomeType::Plain; }
        world.grid().terrainChanged();
        const auto actor=world.createRealm(), target=world.createRealm();
        const auto city=world.foundCapitalSettlement({12,24},actor,{"Amber Crown","Amberfolk","Amber",{},"civic"});
        PALADIN_CHECK(city);
        PALADIN_CHECK(world.foundCapitalSettlement({24,24},target,{"Blue Confederacy","Bluefolk","Blue",{},"tribal"}));
        world.realm(actor)->treasury->balance=100000;
        world.realm(target)->treasury->balance=100;
        DiplomacyPanel panel; GrayUiRenderer ui;
        panel.open(); panel.layout(960,640,world,actor);
        PALADIN_CHECK(!panel.selection());
        PALADIN_CHECK(!panel.actionBounds(DiplomaticAction::Alliance));
        const auto frame=[&](const char* name)
        {
            renderer.beginFrame(); renderer.fillRectangle(0,0,960,640,{32,44,67,255});
            panel.render(renderer,ui,world,actor); capture(window,name); renderer.endFrame();
        };
        frame("pr29-diplomacy-blank.png");
        const auto click=[&](std::optional<UiRectangle> b)
        {
            PALADIN_CHECK(b);
            SDL_Event event{}; event.type=SDL_EVENT_MOUSE_BUTTON_DOWN; event.button.button=SDL_BUTTON_LEFT;
            event.button.x=b->x+b->width*.5F; event.button.y=b->y+b->height*.5F;
            PALADIN_CHECK(panel.handle(event,world,actor)); event.type=SDL_EVENT_MOUSE_BUTTON_UP;
            PALADIN_CHECK(panel.handle(event,world,actor));
        };
        click(panel.realmBounds(target)); PALADIN_CHECK(panel.selection()==target);
        float previous=0;
        for (const auto action:{DiplomaticAction::Alliance,DiplomaticAction::Gift,DiplomaticAction::Tribute,
                               DiplomaticAction::Trade,DiplomaticAction::War,DiplomaticAction::Peace})
        {
            const auto b=panel.actionBounds(action); PALADIN_CHECK(b && b->y>previous); previous=b->y;
        }
        frame("pr29-diplomacy-actions.png");
        click(panel.actionBounds(DiplomaticAction::Alliance)); PALADIN_CHECK(world.diplomacy().between(actor,target)->allied);
        const auto gift=DiplomacySystem::suggestedGift(world,actor,target);
        click(panel.actionBounds(DiplomaticAction::Gift));
        PALADIN_CHECK(gift>0 && world.realm(actor)->treasury->balance==100000-gift && world.realm(target)->treasury->balance==100+gift);
        click(panel.actionBounds(DiplomaticAction::Tribute)); PALADIN_CHECK(world.diplomacy().overlordOf(target)==actor);
        frame("pr29-diplomacy-tributary.png");
        click(panel.actionBounds(DiplomaticAction::Tribute)); PALADIN_CHECK(!world.diplomacy().overlordOf(target));
        click(panel.actionBounds(DiplomaticAction::Trade)); PALADIN_CHECK(world.diplomacy().between(actor,target)->trading);
        click(panel.actionBounds(DiplomaticAction::War)); PALADIN_CHECK(world.diplomacy().between(actor,target)->atWar);
        click(panel.actionBounds(DiplomaticAction::Peace)); PALADIN_CHECK(!world.diplomacy().between(actor,target)->atWar);
        click(panel.sortBounds(DiplomacyPanel::Sort::Gold));
        PALADIN_CHECK(panel.realmBounds(actor)->y<panel.realmBounds(target)->y);
        click(panel.sortBounds(DiplomacyPanel::Sort::Gold));
        PALADIN_CHECK(panel.realmBounds(target)->y<panel.realmBounds(actor)->y);
        // Adjacent 64-bit balances must not collapse into a floating-point tie
        // on Windows, where long double has the same precision as double.
        world.realm(actor)->treasury->balance=std::numeric_limits<Money>::max()-1;
        world.realm(target)->treasury->balance=std::numeric_limits<Money>::max();
        click(panel.sortBounds(DiplomacyPanel::Sort::Gold));
        PALADIN_CHECK(panel.realmBounds(target)->y<panel.realmBounds(actor)->y);
        click(panel.sortBounds(DiplomacyPanel::Sort::Gold));
        PALADIN_CHECK(panel.realmBounds(actor)->y<panel.realmBounds(target)->y);
        world.realm(actor)->treasury->balance=99000;
        world.realm(target)->treasury->balance=1100;
        // A captured button must not fire if released outside, or on another action.
        auto b=*panel.actionBounds(DiplomaticAction::War);
        SDL_Event event{}; event.type=SDL_EVENT_MOUSE_BUTTON_DOWN; event.button.button=SDL_BUTTON_LEFT;
        event.button.x=b.x+8; event.button.y=b.y+8; PALADIN_CHECK(panel.handle(event,world,actor));
        event.type=SDL_EVENT_MOUSE_BUTTON_UP; event.button.x=2; event.button.y=2;
        PALADIN_CHECK(panel.handle(event,world,actor)); PALADIN_CHECK(!world.diplomacy().between(actor,target)->atWar);
        click(panel.realmBounds(actor)); click(panel.actionBounds(DiplomaticAction::War));
        PALADIN_CHECK(!world.diplomacy().between(actor,actor));
        panel.layout(320,240,world,actor); PALADIN_CHECK(panel.bounds().x>=0 && panel.bounds().y>=0);
        PALADIN_CHECK(panel.bounds().x+panel.bounds().width<=320 && panel.bounds().y+panel.bounds().height<=240);
        panel.close();

        // Grid overflow uses rows and a draggable thumb, not a sideways strip.
        std::vector<ArmyId> armies;
        for(int i=0;i<25;++i)
        { const auto id=world.createArmy({12,24}); PALADIN_CHECK(world.assignArmyToRealm(id,actor)); armies.push_back(id); }
        MilitaryPanel military; military.toggle(city); military.layout(960,640,world,actor);
        using A=MilitaryPanel::Action;
        const auto first=military.controlBounds(A::Row,armies.front());
        PALADIN_CHECK(first && first->width==76 && first->height==88);
        PALADIN_CHECK(military.controlBounds(A::Row,armies[13]) && !military.controlBounds(A::Row,armies[14]));
        PALADIN_CHECK(military.controlBounds(A::Row,armies[7])->y>first->y);
        PALADIN_CHECK(military.controlBounds(A::World)->y>military.controlBounds(A::Row,armies[13])->y+88);
        PALADIN_CHECK(military.controlBounds(A::New)->y>military.controlBounds(A::World)->y);
        renderer.beginFrame(); renderer.fillRectangle(0,0,960,640,{32,44,67,255});
        military.render(renderer,ui,world,actor,&art); capture(window,"pr29-military-overflow-top.png"); renderer.endFrame();
        event={}; event.type=SDL_EVENT_MOUSE_WHEEL; event.wheel.x=0; event.wheel.y=-20;
        event.wheel.mouse_x=first->x+10; event.wheel.mouse_y=first->y+10;
        PALADIN_CHECK(military.handle(event,world,actor));
        PALADIN_CHECK(!military.controlBounds(A::Row,armies.front()) && military.controlBounds(A::Row,armies.back()));
        renderer.beginFrame(); renderer.fillRectangle(0,0,960,640,{32,44,67,255});
        military.render(renderer,ui,world,actor,&art); capture(window,"pr29-military-overflow-bottom.png"); renderer.endFrame();
        const auto mb=military.bounds();
        event={}; event.type=SDL_EVENT_MOUSE_BUTTON_DOWN; event.button.button=SDL_BUTTON_LEFT;
        event.button.x=mb.x+mb.width-22; event.button.y=mb.y+44+178;
        PALADIN_CHECK(military.handle(event,world,actor));
        event={}; event.type=SDL_EVENT_MOUSE_MOTION; event.motion.x=mb.x+mb.width-22; event.motion.y=mb.y+44;
        PALADIN_CHECK(military.handle(event,world,actor));
        event={}; event.type=SDL_EVENT_MOUSE_BUTTON_UP; event.button.button=SDL_BUTTON_LEFT;
        event.button.x=mb.x+mb.width-22; event.button.y=mb.y+44;
        PALADIN_CHECK(military.handle(event,world,actor));
        PALADIN_CHECK(military.controlBounds(A::Row,armies.front()) && !military.selection());
        // Exercise all five sorts and a list longer than the left viewport.
        for (int i=0;i<20;++i) PALADIN_CHECK(world.createRealm());
        panel.open(); panel.layout(960,640,world,actor);
        for(auto sort:{DiplomacyPanel::Sort::Soldiers,DiplomacyPanel::Sort::Gold,DiplomacyPanel::Sort::Size,
                       DiplomacyPanel::Sort::Cities,DiplomacyPanel::Sort::Fortresses}) click(panel.sortBounds(sort));
        event={}; event.type=SDL_EVENT_MOUSE_WHEEL; event.wheel.y=-40;
        event.wheel.mouse_x=panel.bounds().x+30; event.wheel.mouse_y=panel.bounds().y+140;
        PALADIN_CHECK(panel.handle(event,world,actor));
        PALADIN_CHECK(panel.realmBounds(actor) && panel.realmBounds(target));
        std::cout<<"Diplomacy UI: blank/selected states, ordered actions, conserved gift, tributary toggle, five sorts, scroll and compact army overflow passed\n";
    }

    void uprightSettlements(Renderer& renderer, SDL_Window* window, SceneSpriteLibrary& art)
    {
        WorldGenerationSettings settings;
        settings.width=settings.height=64; settings.seed=711; settings.populateAiRealms=false;
        Simulation sim(settings); auto& world=sim.world();
        for(int y=0;y<world.grid().height();++y) for(int x=0;x<world.grid().width();++x) { world.grid().tile({x,y})->terrain=TerrainType::Land; world.grid().tile({x,y})->biome=BiomeType::Plain; }
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
        diplomacyAndOverflow(renderer,window.nativeHandle(),art);
        uprightSettlements(renderer,window.nativeHandle(),art);
        std::cout<<"Military UI, universal markers, walking/gathering, sleep pixel blocks and pause checks passed.\n";
    }
    catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; result=1; }
    SDL_Quit(); return result;
}

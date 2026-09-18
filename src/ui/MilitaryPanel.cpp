#include "ui/MilitaryPanel.h"
#include "ui/GrayUiRenderer.h"
#include "rendering/WorldArmyPresentation.h"
#include "simulation/MilitarySystem.h"
#include "world/World.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <limits>
namespace Paladin
{
    void MilitaryPanel::toggle(SettlementId city)
    {
        if (open_ && city_==city) { close(); return; }
        open_=true; city_=city; selected_={}; scroll_=0; message_.clear();
        captured_=false; draggingScroll_=false; pressed_=hovered_=-1; pressedControl_.reset();
    }
    void MilitaryPanel::close() noexcept
    { open_=false; captured_=false; draggingScroll_=false; pressed_=hovered_=-1; pressedControl_.reset(); controls_.clear(); }
    std::optional<UiRectangle> MilitaryPanel::controlBounds(Action action, ArmyId unit) const noexcept
    {
        for (const auto& c : controls_)
            if (c.action == action && (action != Action::Row || c.unit == unit)) return c.bounds;
        return std::nullopt;
    }
    void MilitaryPanel::layout(int width, int height, const World& world, RealmId actor)
    {
        viewportWidth_=width; viewportHeight_=height;
        if (!open_) return;
        controls_.clear();
        if (width < 352 || height < 432)
        {
            bounds_={4,4,std::max(0.F,float(width)-8),std::max(0.F,float(height)-8)};
            rows_=scrollTrack_=scrollThumb_={}; maxScroll_=0;
            controls_.push_back({{std::max(4.F,float(width)-40),12,28,24},Action::Close,"X",true,{}});
            return;
        }
        const float w=std::min(672.F,float(width)-16.F), h=std::min(560.F,float(height)-32.F);
        bounds_={(float(width)-w)*.5F,(float(height)-h)*.5F,w,h};
        const float x=bounds_.x+16, y=bounds_.y, inner=w-32;
        const auto button=[&](UiRectangle b, Action a, std::string label, bool enabled=true, ArmyId id=ArmyId{})
        { controls_.push_back({b,a,std::move(label),enabled,id}); };
        button({x+inner-28,y+10,28,26},Action::Close,"X");

        // Compact portrait cards are the FIRST content, before filters and all
        // recruitment/organization controls. Scroll in rows, never sideways.
        constexpr float cardWidth=76, cardHeight=88, gap=6;
        const int columns=std::max(1,int((inner-18+gap)/(cardWidth+gap)));
        const int visibleRows=h>=500?2:1;
        rows_={x,y+44,inner-18,visibleRows*(cardHeight+gap)-gap};
        std::vector<ArmyId> units;
        for (const auto& unit:world.armies())
            if (unit.ownerRealmId()==actor && (!local_ || MilitarySystem::presentAt(world,unit,city_))) units.push_back(unit.id());
        const int totalRows=(int(units.size())+columns-1)/columns;
        maxScroll_=std::max(0,totalRows-visibleRows);
        scroll_=std::clamp(scroll_,0,maxScroll_);
        const int first=scroll_*columns, visible=visibleRows*columns;
        for (int i=first;i<std::min(int(units.size()),first+visible);++i)
        {
            const int index=i-first;
            button({x+(index%columns)*(cardWidth+gap),rows_.y+(index/columns)*(cardHeight+gap),cardWidth,cardHeight},
                   Action::Row,"",true,units[i]);
        }
        scrollTrack_={x+inner-12,rows_.y,12,rows_.height};
        const float thumbHeight=maxScroll_?std::max(18.F,rows_.height*visibleRows/std::max(1.F,float(totalRows))):rows_.height;
        scrollThumb_={scrollTrack_.x,scrollTrack_.y+(rows_.height-thumbHeight)*scroll_/std::max(1,maxScroll_),12,thumbHeight};

        const float tabs=rows_.y+rows_.height+12;
        button({x,tabs,inner*.5F-4,28},Action::World,"World units");
        button({x+inner*.5F+4,tabs,inner*.5F-4,28},Action::Local,"In this city");
        const auto* city=world.settlement(city_);
        const bool owned=city && actor && city->ownerRealmId()==actor;
        const auto reserves=MilitarySystem::available(world,city_);
        const float third=(inner-12)/3;
        button({x,tabs+58,third,28},Action::New,"New unit",owned && reserves>0);
        button({x+third+6,tabs+58,third,28},Action::Hire,"Hire +1",owned);
        button({x+2*(third+6),tabs+58,third,28},Action::Dismiss,"Dismiss -1",owned && reserves>0);

        const auto* unit=world.army(selected_);
        if (!unit || unit->ownerRealmId()!=actor) { selected_={}; unit=nullptr; }
        const bool organize=unit && MilitarySystem::canOrganize(world,*unit);
        const auto available=unit?MilitarySystem::available(world,MilitarySystem::stationAt(world,*unit)):0;
        const auto releasable=unit?MilitarySystem::releasable(world,*unit):0;
        const float tools=tabs+152;
        button({x,tools,third,28},Action::AddOne,"+1 soldier",organize && available>0);
        button({x+third+6,tools,third,28},Action::AddFive,"+5 soldiers",organize && available>0);
        button({x+2*(third+6),tools,third,28},Action::Focus,"Show on map",unit && unit->soldierCount()>0);
        button({x,tools+34,third,28},Action::RemoveOne,"-1 soldier",organize && releasable>0);
        button({x+third+6,tools+34,third,28},Action::RemoveFive,"-5 soldiers",organize && releasable>0);
        button({x+2*(third+6),tools+34,third,28},Action::Disband,"Disband",unit &&
            ((organize && releasable==unit->soldierCount()) || (unit->soldierCount()==0 && unit->rations()==0)));
    }
    void MilitaryPanel::act(const Control& control, World& world, RealmId actor)
    {
        if (!control.enabled) return;
        using A=Action;
        switch(control.action)
        {
        case A::Close: close(); return;
        case A::World: local_=false; scroll_=0; break;
        case A::Local: local_=true; scroll_=0; break;
        case A::Row: selected_=control.unit; message_.clear(); break;
        case A::New:
            selected_=MilitarySystem::createUnit(world,actor,city_);
            message_=selected_?"Unit created with one employed reserve. It is on the world map now.":"Hire a barracks reserve first. An owned city is required; realm limit: 256.";
            scroll_=std::numeric_limits<int>::max(); break;
        case A::Hire: case A::Dismiss:
            message_=militaryResultText(MilitarySystem::recruit(world,actor,city_,control.action==A::Hire?1:-1)); break;
        case A::AddOne: case A::AddFive: case A::RemoveOne: case A::RemoveFive:
        {
            const int delta=control.action==A::AddOne?1:control.action==A::AddFive?5:control.action==A::RemoveOne?-1:-5;
            message_=militaryResultText(MilitarySystem::resizeUnit(world,actor,selected_,delta)); break;
        }
        case A::Disband:
            message_=militaryResultText(MilitarySystem::disbandUnit(world,actor,selected_));
            if (!world.army(selected_)) selected_={}; break;
        case A::Focus: focus_=selected_; close(); return;
        }
        layout(viewportWidth_,viewportHeight_,world,actor);
    }
    bool MilitaryPanel::handle(const SDL_Event& event, World& world, RealmId actor)
    {
        if (!open_) return false;
        if (event.type==SDL_EVENT_WINDOW_FOCUS_LOST) { captured_=draggingScroll_=false; pressedControl_.reset(); pressed_=-1; return false; }
        if (event.type==SDL_EVENT_KEY_DOWN && event.key.scancode==SDL_SCANCODE_ESCAPE)
        { close(); return true; }
        if (event.type==SDL_EVENT_MOUSE_WHEEL && contains(event.wheel.mouse_x,event.wheel.mouse_y))
        { scroll_-=int(event.wheel.y); layout(viewportWidth_,viewportHeight_,world,actor); return true; }
        if (event.type==SDL_EVENT_MOUSE_MOTION)
        {
            if (draggingScroll_)
            {
                const float travel=scrollTrack_.height-scrollThumb_.height;
                if (travel>0) scroll_=int(std::round((event.motion.y-scrollTrack_.y-scrollGrab_)/travel*maxScroll_));
                layout(viewportWidth_,viewportHeight_,world,actor); return true;
            }
            hovered_=-1;
            for (int i=0;i<int(controls_.size());++i)
                if (controls_[i].bounds.contains(event.motion.x,event.motion.y)) { hovered_=i; break; }
            return captured_ || contains(event.motion.x,event.motion.y);
        }
        if (event.type==SDL_EVENT_MOUSE_BUTTON_DOWN)
        {
            if (!contains(event.button.x,event.button.y)) return false;
            if (event.button.button==SDL_BUTTON_RIGHT) { close(); return true; }
            if (event.button.button!=SDL_BUTTON_LEFT) return true;
            captured_=true; pressed_=-1; pressedControl_.reset();
            if (maxScroll_>0 && scrollTrack_.contains(event.button.x,event.button.y))
            {
                draggingScroll_=true;
                scrollGrab_=scrollThumb_.contains(event.button.x,event.button.y) ? event.button.y-scrollThumb_.y : scrollThumb_.height*.5F;
                const float travel=scrollTrack_.height-scrollThumb_.height;
                if (travel>0) scroll_=int(std::round((event.button.y-scrollTrack_.y-scrollGrab_)/travel*maxScroll_));
                layout(viewportWidth_,viewportHeight_,world,actor); return true;
            }
            for (int i=0;i<int(controls_.size());++i)
                if (controls_[i].enabled && controls_[i].bounds.contains(event.button.x,event.button.y))
                { pressed_=i; pressedControl_=controls_[i]; break; }
            return true;
        }
        if (event.type==SDL_EVENT_MOUSE_BUTTON_UP && event.button.button==SDL_BUTTON_LEFT)
        {
            const bool consume=captured_ || contains(event.button.x,event.button.y);
            const auto pressed=pressedControl_; captured_=false; draggingScroll_=false; pressed_=-1; pressedControl_.reset();
            if (pressed)
                for (const auto& c : controls_)
                    if (c.enabled && c.action==pressed->action && c.unit==pressed->unit &&
                        c.bounds.contains(event.button.x,event.button.y))
                    { const auto action=c; act(action,world,actor); break; }
            return consume;
        }
        return false;
    }
    void MilitaryPanel::render(Renderer& renderer, const GrayUiRenderer& ui, const World& world, RealmId actor, const SceneSpriteLibrary* artwork) const
    {
        if (!open_) return;
        ui.drawPanel(renderer,bounds_);
        if (viewportWidth_ < 352 || viewportHeight_ < 432)
        {
            if (viewportWidth_ > 152) ui.drawLabel(renderer,"Military",16,18,2);
            if (viewportWidth_ > 260 && viewportHeight_ > 72)
                ui.drawLabel(renderer,"Enlarge the window to manage units.",16,56,1);
            for (const auto& c : controls_) ui.drawButton(renderer,c.bounds,c.text,false,false,false,true);
            return;
        }
        const float x=bounds_.x+16, y=bounds_.y, inner=bounds_.width-32;
        BitmapFontRenderer font;
        const auto fit=[&](std::string text,float width,float scale)
        {
            if (font.measureWidth(text,scale)<=width) return text;
            while (!text.empty() && font.measureWidth(text+"...",scale)>width) text.pop_back();
            return text+"...";
        };
        const auto label=[&](std::string_view text,float px,float py,float width,float scale=2.F)
        { ui.drawLabel(renderer,fit(std::string(text),width,scale),px,py,scale); };
        ui.drawLabel(renderer,"Military",x,y+14,3);
        if (std::none_of(controls_.begin(),controls_.end(),[](const auto& c){return c.action==Action::Row;}))
        {
            label(local_?"No units present in this city.":"No units created yet.",x,rows_.y+20,rows_.width,2);
            label("Hire barracks reserves, then create a unit below.",x,rows_.y+46,rows_.width,1);
        }
        for (int i=0;i<int(controls_.size());++i)
        {
            const auto& c=controls_[i];
            const bool selected=c.action==Action::Row?c.unit==selected_:c.action==Action::World?!local_:c.action==Action::Local?local_:false;
            ui.drawButton(renderer,c.bounds,c.text,i==hovered_,i==pressed_,selected,c.enabled);
            if (c.action!=Action::Row) continue;
            const auto* unit=world.army(c.unit); if (!unit) continue;
            label(unit->name(),c.bounds.x+6,c.bounds.y+8,c.bounds.width-12,1);
            if (const auto* sprite=artwork?worldArmySprite(*artwork,world,*unit):nullptr; sprite && sprite->texture)
            {
                const auto frame=artwork->frame(*sprite,false);
                constexpr float imageHeight=36;
                const float imageWidth=imageHeight*float(sprite->width/sprite->height);
                renderer.drawTexture(*sprite->texture,frame.x,frame.y,frame.width,frame.height,
                    c.bounds.x+(c.bounds.width-imageWidth)*.5F,c.bounds.y+24,imageWidth,imageHeight);
            }
            const auto number=std::to_string(unit->soldierCount());
            const float scale=font.measureWidth(number,2)<=c.bounds.width-12?2.F:1.F;
            ui.drawLabel(renderer,number,c.bounds.x+(c.bounds.width-font.measureWidth(number,scale))*.5F,c.bounds.y+66,scale);
        }
        if (maxScroll_>0)
        {
            renderer.fillRectangle(scrollTrack_.x,scrollTrack_.y,scrollTrack_.width,scrollTrack_.height,{10,18,31,255});
            renderer.fillRectangle(scrollThumb_.x,scrollThumb_.y,scrollThumb_.width,scrollThumb_.height,{155,137,107,255});
        }
        const float tabs=rows_.y+rows_.height+12;
        const auto* city=world.settlement(city_);
        label(std::string(city?city->name():"No city")+" | "+
            std::to_string(MilitarySystem::available(world,city_))+" employed reserves",x,tabs+38,inner,1.5F);
        const auto* unit=world.army(selected_);
        if (unit && unit->ownerRealmId()==actor)
        {
            label(unit->name()+" | "+std::to_string(unit->soldierCount())+" soldiers | "+std::to_string(unit->rations())+" rations",x,tabs+100,inner,2);
            const auto* station=world.settlement(MilitarySystem::stationAt(world,*unit));
            label(station?"Present in "+std::string(station->name()):unit->moving()?"Marching between world tiles":"Independent world field unit",x,tabs+125,inner,1.5F);
        }
        else
        {
            label("Select a unit card above.",x,tabs+100,inner,2);
            label("New unit assigns one real reserve. Add more soldiers here.",x,tabs+125,inner,1);
        }
        const float foot=bounds_.y+bounds_.height-40;
        label("Scroll or drag the unit scrollbar. Reserves are employed barracks soldiers.",x,foot,inner,1);
        label("Show on map; right-click land to march. Discharge at the recruitment city.",x,foot+12,inner,1);
        ui.drawLabel(renderer,fit(message_,inner,1),x,foot+26,1,{235,196,107,255});
    }
}

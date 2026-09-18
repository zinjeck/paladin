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
        captured_=false; pressed_=hovered_=-1; pressedControl_.reset();
    }
    void MilitaryPanel::close() noexcept
    { open_=false; captured_=false; pressed_=hovered_=-1; pressedControl_.reset(); controls_.clear(); }
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
        if (width < 352 || height < 432)
        {
            bounds_={4,4,std::max(0.F,float(width)-8),std::max(0.F,float(height)-8)};
            controls_.clear();
            controls_.push_back({{std::max(4.F,float(width)-40),12,28,24},Action::Close,"X",true,{}});
            return;
        }
        const float w=std::min(672.F,float(width)-16.F);
        const float h=std::min(560.F,float(height)-32.F);
        bounds_={(float(width)-w)*.5F,(float(height)-h)*.5F,w,h};
        const float x=bounds_.x+16, y=bounds_.y, inner=w-32;
        controls_.clear();
        const auto button=[&](UiRectangle b, Action a, std::string label, bool enabled=true, ArmyId id=ArmyId{})
        { controls_.push_back({b,a,std::move(label),enabled,id}); };
        button({x+inner-28,y+10,28,26},Action::Close,"X");
        button({x,y+44,inner*.5F-4,30},Action::World,"World units");
        button({x+inner*.5F+4,y+44,inner*.5F-4,30},Action::Local,"In this city");
        const auto* city=world.settlement(city_);
        const bool owned=city && actor && city->ownerRealmId()==actor;
        const auto reserves=MilitarySystem::available(world,city_);
        const float third=(inner-12)/3;
        button({x,y+104,third,30},Action::New,"New unit",owned && reserves>0);
        button({x+third+6,y+104,third,30},Action::Hire,"Hire +1",owned);
        button({x+2*(third+6),y+104,third,30},Action::Dismiss,"Dismiss -1",owned && reserves>0);

        // A scrollable Y-axis column of PORTRAIT cards, never wide text rows.
        // Integer card sizes stay taller than their widths at every viewport.
        const float cardWidth=std::floor(std::clamp(inner*.30F,96.F,144.F));
        const float cardHeight=std::ceil(std::max(168.F,cardWidth*1.22F));
        const float pitch=cardHeight+8;
        rows_={x,y+144,cardWidth,h-194};
        std::vector<ArmyId> units;
        for (const auto& unit:world.armies())
            if (unit.ownerRealmId()==actor && (!local_ || MilitarySystem::presentAt(world,unit,city_)))
                units.push_back(unit.id());
        const int visible=std::max(1,int((rows_.height+8)/pitch));
        scroll_=std::clamp(scroll_,0,std::max(0,int(units.size())-visible));
        for (int i=scroll_; i<std::min(int(units.size()),scroll_+visible); ++i)
            button({x,rows_.y+float(i-scroll_)*pitch,cardWidth,cardHeight},Action::Row,"",true,units[i]);

        const auto* unit=world.army(selected_);
        if (!unit || unit->ownerRealmId()!=actor) { selected_={}; unit=nullptr; }
        const bool organize=unit && MilitarySystem::canOrganize(world,*unit);
        const auto available=unit?MilitarySystem::available(world,MilitarySystem::stationAt(world,*unit)):0;
        const auto releasable=unit?MilitarySystem::releasable(world,*unit):0;
        const float right=x+cardWidth+16, side=inner-cardWidth-16, half=(side-6)*.5F;
        const float tools=rows_.y+88;
        button({right,tools,half,28},Action::AddOne,"+1 soldier",organize && available>0);
        button({right+half+6,tools,half,28},Action::AddFive,"+5 soldiers",organize && available>0);
        button({right,tools+34,half,28},Action::RemoveOne,"-1 soldier",organize && releasable>0);
        button({right+half+6,tools+34,half,28},Action::RemoveFive,"-5 soldiers",organize && releasable>0);
        button({right,tools+68,half,30},Action::Focus,"Show on map",unit && unit->soldierCount()>0);
        button({right+half+6,tools+68,half,30},Action::Disband,"Disband",unit &&
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
        if (event.type==SDL_EVENT_KEY_DOWN && event.key.scancode==SDL_SCANCODE_ESCAPE)
        { close(); return true; }
        if (event.type==SDL_EVENT_MOUSE_WHEEL && contains(event.wheel.mouse_x,event.wheel.mouse_y))
        { scroll_-=int(event.wheel.y); layout(viewportWidth_,viewportHeight_,world,actor); return true; }
        if (event.type==SDL_EVENT_MOUSE_MOTION)
        {
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
            for (int i=0;i<int(controls_.size());++i)
                if (controls_[i].enabled && controls_[i].bounds.contains(event.button.x,event.button.y))
                { pressed_=i; pressedControl_=controls_[i]; break; }
            return true;
        }
        if (event.type==SDL_EVENT_MOUSE_BUTTON_UP && event.button.button==SDL_BUTTON_LEFT)
        {
            const bool consume=captured_ || contains(event.button.x,event.button.y);
            const auto pressed=pressedControl_; captured_=false; pressed_=-1; pressedControl_.reset();
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
        const auto* city=world.settlement(city_);
        label(std::string(city?city->name():"No city")+" | "+
            std::to_string(MilitarySystem::available(world,city_))+" employed reserves",x,y+84,inner,2);
        if (std::none_of(controls_.begin(),controls_.end(),[](const auto& c){return c.action==Action::Row;}))
        {
            label("No units",x,rows_.y+12,rows_.width,2);
            label(local_?"present here.":"created yet.",x,rows_.y+32,rows_.width,1);
        }
        for (int i=0;i<int(controls_.size());++i)
        {
            const auto& c=controls_[i];
            const bool selected=c.action==Action::Row?c.unit==selected_:c.action==Action::World?!local_:c.action==Action::Local?local_:false;
            ui.drawButton(renderer,c.bounds,c.text,i==hovered_,i==pressed_,selected,c.enabled);
            if (c.action!=Action::Row) continue;
            const auto* unit=world.army(c.unit);
            if (!unit) continue;
            const float px=c.bounds.x+10, py=c.bounds.y, width=c.bounds.width-20;
            label(unit->name(),px,py+14,width,2);
            if (const auto* sprite=artwork?worldArmySprite(*artwork,world,*unit):nullptr; sprite && sprite->texture)
            {
                const auto frame=artwork->frame(*sprite,false);
                constexpr float imageHeight=54;
                const float imageWidth=imageHeight*float(sprite->width/sprite->height);
                renderer.drawTexture(*sprite->texture,frame.x,frame.y,frame.width,frame.height,
                    c.bounds.x+(c.bounds.width-imageWidth)*.5F,py+36,imageWidth,imageHeight);
            }
            const auto number=std::to_string(unit->soldierCount());
            const float scale=font.measureWidth(number,3)<=width?3.F:2.F;
            ui.drawLabel(renderer,number,c.bounds.x+(c.bounds.width-font.measureWidth(number,scale))*.5F,py+96,scale);
            label("soldiers",px,py+123,width,1);
            const auto* station=world.settlement(MilitarySystem::stationAt(world,*unit));
            label(unit->moving()?"Marching":station?station->name():"World field unit",px,py+137,width,1);
            label(std::to_string(unit->rations())+" rations",px,py+c.bounds.height-18,width,1);

        }
        const auto* unit=world.army(selected_);
        const float right=rows_.x+rows_.width+16, side=inner-rows_.width-16, top=rows_.y;
        if (unit && unit->ownerRealmId()==actor)
        {
            label(unit->name(),right,top+4,side,2);
            label(std::to_string(unit->soldierCount())+" soldiers | "+std::to_string(unit->rations())+" rations",right,top+25,side,2);
            const auto* station=world.settlement(MilitarySystem::stationAt(world,*unit));
            label(station?"Present in "+std::string(station->name()):unit->moving()?"Marching between world tiles":"Independent world field unit",right,top+48,side,1);
            label("Add reserves where the unit is present.",right,top+62,side,1);
            label("Discharge in each soldier's recruitment city.",right,top+74,side,1);
        }
        else
        {
            label("Select a unit card.",right,top+6,side,2);
            label("Hire real soldiers at a completed barracks.",right,top+36,side,1);
            label("New unit assigns one of those reserves.",right,top+52,side,1);
        }
        const float foot=bounds_.y+bounds_.height-40;
        label("Rule: Barracks + Army Supply Depot. Scroll the unit cards vertically.",x,foot,inner,1);
        label("Select the soldier on the world map; right-click land to march.",x,foot+12,inner,1);
        ui.drawLabel(renderer,fit(message_,inner,1),x,foot+26,1,{235,196,107,255});
    }
}

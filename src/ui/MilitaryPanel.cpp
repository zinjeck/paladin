#include "ui/MilitaryPanel.h"
#include "ui/GrayUiRenderer.h"
#include "simulation/MilitarySystem.h"
#include "world/World.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
namespace Paladin
{
    namespace
    {
        std::string clipped(std::string value, std::size_t count)
        { if (value.size()>count) value=value.substr(0,count-3)+"..."; return value; }
    }
    void MilitaryPanel::toggle(SettlementId city)
    {
        if (open_ && city_==city) { close(); return; }
        open_=true; city_=city; selected_={}; scroll_=0; message_.clear();
        captured_=false; pressed_=hovered_=-1;
    }
    void MilitaryPanel::close() noexcept
    { open_=false; captured_=false; pressed_=hovered_=-1; controls_.clear(); }
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
        const float w=std::min(624.F,std::max(320.F,float(width)-16.F));
        const float h=std::min(516.F,std::max(320.F,float(height)-96.F));
        bounds_={std::max(8.F,(float(width)-w)*.5F),std::max(52.F,(float(height)-h)*.5F),w,h};
        const float x=bounds_.x+16, y=bounds_.y, inner=w-32;
        controls_.clear();
        const auto button=[&](UiRectangle b, Action a, std::string label, bool enabled=true, ArmyId id=ArmyId{})
        { controls_.push_back({b,a,std::move(label),enabled,id}); };
        button({x+inner-28,y+10,28,26},Action::Close,"X");
        button({x,y+44,inner*.5F-4,30},Action::World,"World units");
        button({x+inner*.5F+4,y+44,inner*.5F-4,30},Action::Local,"In this city");
        const float listHeight=std::max(48.F,h-316);
        rows_={x,y+104,inner,listHeight};
        std::vector<ArmyId> units;
        for (const auto& unit:world.armies())
            if (unit.ownerRealmId()==actor && (!local_ || MilitarySystem::presentAt(world,unit,city_)))
                units.push_back(unit.id());
        const int visible=std::max(1,int(listHeight/36));
        scroll_=std::clamp(scroll_,0,std::max(0,int(units.size())-visible));
        for (int i=scroll_; i<std::min(int(units.size()),scroll_+visible); ++i)
        {
            const auto* unit=world.army(units[i]);
            std::string location = unit->moving() ? "Marching" :
                "At " + std::to_string(unit->position().x) + "," + std::to_string(unit->position().y);
            for (const auto& candidate : world.settlements())
                if (MilitarySystem::presentAt(world, *unit, candidate.id()))
                { location = candidate.name(); break; }
            const std::string label=unit->name()+"  "+std::to_string(unit->soldierCount())+
                " soldiers | "+location;
            button({x,rows_.y+float(i-scroll_)*36,inner,32},Action::Row,clipped(label,std::size_t(inner/12)),true,unit->id());
        }
        const auto* city=world.settlement(city_);
        const bool owned=city && actor && city->ownerRealmId()==actor;
        const auto* unit=world.army(selected_);
        if (!unit || unit->ownerRealmId()!=actor) { selected_={}; unit=nullptr; }
        const bool organize=unit && MilitarySystem::canOrganize(world,*unit);
        const auto available=unit?MilitarySystem::available(world,unit->homeSettlementId()):0;
        const float tools=rows_.y+rows_.height+8, third=(inner-12)/3;
        button({x,tools,third,30},Action::New,"New unit",owned);
        button({x+third+6,tools,third,30},Action::Hire,"Hire +1",owned);
        button({x+2*(third+6),tools,third,30},Action::Dismiss,"Dismiss -1",owned && MilitarySystem::available(world,city_)>0);
        const float detail=tools+78, quarter=(inner-18)/4;
        button({x,detail,quarter,30},Action::AddOne,"+1",organize && available>0);
        button({x+quarter+6,detail,quarter,30},Action::AddFive,"+5",organize && available>0);
        button({x+2*(quarter+6),detail,quarter,30},Action::RemoveOne,"-1",organize && unit->soldierCount()>0);
        button({x+3*(quarter+6),detail,quarter,30},Action::RemoveFive,"-5",organize && unit->soldierCount()>0);
        button({x,detail+36,inner*.5F-4,30},Action::Disband,"Disband unit",organize || (unit && unit->soldierCount()==0 && unit->rations()==0));
        button({x+inner*.5F+4,detail+36,inner*.5F-4,30},Action::Focus,"Show on map",unit!=nullptr);
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
            message_=selected_?"Empty unit created. Assign barracks recruits with +1 or +5.":"A unit needs an owned, initialized city; realm limit is 256.";
            local_=false; scroll_=std::max(0,int(world.armies().size())-1); break;
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
            captured_=true; pressed_=-1;
            for (int i=0;i<int(controls_.size());++i)
                if (controls_[i].enabled && controls_[i].bounds.contains(event.button.x,event.button.y))
                { pressed_=i; break; }
            return true;
        }
        if (event.type==SDL_EVENT_MOUSE_BUTTON_UP && event.button.button==SDL_BUTTON_LEFT)
        {
            const bool consume=captured_ || contains(event.button.x,event.button.y);
            const int index=pressed_; captured_=false; pressed_=-1;
            if (index>=0 && index<int(controls_.size()) && controls_[index].bounds.contains(event.button.x,event.button.y))
            { const auto action=controls_[index]; act(action,world,actor); }
            return consume;
        }
        return false;
    }
    void MilitaryPanel::render(Renderer& renderer, const GrayUiRenderer& ui, const World& world, RealmId actor) const
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
        ui.drawLabel(renderer,"Military",x,y+14,3);
        const auto* city=world.settlement(city_);
        const std::string heading=std::string(city?city->name():"No city")+" | Reserve "+std::to_string(MilitarySystem::available(world,city_));
        ui.drawLabel(renderer,clipped(heading,std::size_t(inner/12)),x,y+84,2);
        if (std::none_of(controls_.begin(),controls_.end(),[](const auto& c){return c.action==Action::Row;}))
            ui.drawLabel(renderer,local_?"No units present in this city.":"No world units. Create one below.",x,rows_.y+12,2);
        for (int i=0;i<int(controls_.size());++i)
        {
            const auto& c=controls_[i];
            const bool selected=c.action==Action::Row?c.unit==selected_:c.action==Action::World?!local_:c.action==Action::Local?local_:false;
            ui.drawButton(renderer,c.bounds,c.text,i==hovered_,i==pressed_,selected,c.enabled);
        }
        const auto* unit=world.army(selected_);
        const float textY=rows_.y+rows_.height+44;
        std::string detail="Select a unit to organize its soldiers.";
        if (unit && unit->ownerRealmId()==actor)
            detail=unit->name()+" | Soldiers "+std::to_string(unit->soldierCount())+" | Rations "+std::to_string(unit->rations());
        ui.drawLabel(renderer,clipped(detail,std::size_t(inner/12)),x,textY,2);
        ui.drawLabel(renderer,unit && !MilitarySystem::canOrganize(world,*unit)?"Return home to change the roster.":"Roster changes transfer recruits, not population.",x,textY+20,1);
        const float foot=bounds_.y+bounds_.height-48;
        ui.drawLabel(renderer,"Hire employs an adult at a barracks. Dismiss releases reserves.",x,foot,1);
        ui.drawLabel(renderer,"Show on map, then right-click land to march. Rations are finite.",x,foot+12,1);
        ui.drawLabel(renderer,clipped(message_,std::size_t(inner/6)),x,foot+28,1,{235,196,107,255});
    }
}

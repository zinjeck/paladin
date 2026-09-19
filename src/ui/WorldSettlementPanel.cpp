#include "ui/WorldSettlementPanel.h"
#include "ui/GrayUiRenderer.h"
#include "world/World.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include "simulation/WorldShipmentSystem.h"
#include "simulation/MilitarySystem.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <charconv>
#include <cmath>

namespace Paladin
{
    void WorldSettlementPanel::open(SettlementId city)
    { close(); city_=city; scroll_=0; repeating_=false; quantity_="1"; message_.clear(); }
    void WorldSettlementPanel::close() noexcept
    { city_={}; resource_.clear(); controls_.clear(); editing_=captured_=false; pressed_.reset(); drag_.cancel(); }
    int WorldSettlementPanel::amount() const noexcept
    { int value=0; const auto r=std::from_chars(quantity_.data(),quantity_.data()+quantity_.size(),value); return r.ec==std::errc{}?value:0; }
    void WorldSettlementPanel::layout(int width,int height,const World& world,RealmId actor)
    {
        width_=width; height_=height;
        const auto* city=world.settlement(city_); if (!city) { close(); return; }
        const bool own=city->ownerRealmId()==actor && bool(actor);
        if (!own) { resource_.clear(); editing_=false; }
        const float w=std::min(410.F,std::max(0.F,float(width)-24)), h=std::min(680.F,std::max(0.F,float(height)-100));
        bounds_=drag_.place({float(width)-w-12,64,w,h},width,height); controls_.clear(); list_={};
        const float x=bounds_.x+12,y=bounds_.y;
        auto add=[&](UiRectangle b,Kind k,std::string label,int value=0,std::uint64_t id=0,bool enabled=true)
        { controls_.push_back({b,k,std::move(label),value,id,enabled}); };
        add({x+w-54,y+10,28,26},Kind::Close,"X");
        if(w<300 || h<480) return;
        const float pitch=std::clamp((h-360.F)/9.F,19.F,27.F);
        int index=0;
        for(const auto& res:SettlementResourceCatalog::definitions())
        {
            if(own) add({x,y+121+index*pitch,46,pitch-3},Kind::Send,"Send",index,0,WorldShipmentSystem::available(*city,res.id)>0 || resource_==res.id);
            ++index;
        }
        const float start=y+133+9*pitch, side=w-24;
        if(!resource_.empty())
        {
            add({x,start+21,32,26},Kind::Adjust,"-",-1);
            add({x+36,start+21,32,26},Kind::Adjust,"-10",-10);
            add({x+72,start+21,side-218,26},Kind::Amount,quantity_);
            add({x+side-142,start+21,38,26},Kind::Adjust,"+10",10);
            add({x+side-100,start+21,32,26},Kind::Adjust,"+",1);
            add({x+side-64,start+21,64,26},Kind::All,"All");
            add({x,start+51,side*.36F-4,27},Kind::Once,"One trip");
            add({x+side*.36F,start+51,side*.36F-4,27},Kind::Repeat,"Sustained");
            add({x+side*.72F,start+51,side*.28F,27},Kind::Cancel,"Cancel");
            list_={x,start+107,side-22,std::max(30.F,h-(start-y)-150)};
            std::vector<SettlementId> targets;
            for(const auto& target:world.settlements()) if(target.ownerRealmId()==actor && target.id()!=city_) targets.push_back(target.id());
            const int visible=std::max(1,int(list_.height/30)); scroll_=std::clamp(scroll_,0,std::max(0,int(targets.size())-visible));
            for(int i=scroll_;i<std::min(int(targets.size()),scroll_+visible);++i)
                add({x,list_.y+(i-scroll_)*30.F,list_.width,26},Kind::Destination,std::string(world.settlement(targets[i])->name()),0,targets[i].value());
            add({x+side-18,list_.y,18,25},Kind::Up,"^",0,0,scroll_>0);
            add({x+side-18,list_.y+list_.height-25,18,25},Kind::Down,"v",0,0,scroll_+visible<int(targets.size()));
        }
        else
        {
            list_={x,start+25,side,std::max(35.F,h-(start-y)-67)};
            std::vector<ShipmentId> routes;
            for(const auto& route:world.shipments()) if(route.source==city_ && route.active()) routes.push_back(route.id);
            const int visible=std::max(1,int(list_.height/43)); scroll_=std::clamp(scroll_,0,std::max(0,int(routes.size())-visible));
            if(own) for(int i=scroll_;i<std::min(int(routes.size()),scroll_+visible);++i)
                add({x+side-82,list_.y+(i-scroll_)*43.F,60,25},Kind::Stop,"Stop",0,routes[i].value());
            add({x+side-18,list_.y,18,25},Kind::Up,"^",0,0,scroll_>0);
            add({x+side-18,list_.y+list_.height-25,18,25},Kind::Down,"v",0,0,scroll_+visible<int(routes.size()));
        }
    }
    bool WorldSettlementPanel::chooseDestination(World& world,RealmId actor,SettlementId target)
    {
        if(!choosingDestination()) return false;
        const auto result=WorldShipmentSystem::create(world,actor,city_,target,resource_,amount(),repeating_);
        message_=shipmentResultText(result);
        if(result==ShipmentResult::Success) { resource_.clear(); editing_=false; scroll_=0; }
        layout(width_,height_,world,actor); return true;
    }
    void WorldSettlementPanel::act(const Control& c,World& world,RealmId actor)
    {
        if(!c.enabled) return;
        editing_=c.kind==Kind::Amount;
        const auto* city=world.settlement(city_);
        switch(c.kind)
        {
        case Kind::Close: close(); return;
        case Kind::Send:
        {
            const auto id=SettlementResourceCatalog::definitions()[c.value].id;
            if(resource_==id) resource_.clear();
            else { resource_=id; quantity_=std::to_string(std::min(10,WorldShipmentSystem::available(*city,id))); }
            scroll_=0; message_.clear(); break;
        }
        case Kind::Adjust: quantity_=std::to_string(std::clamp(amount()+c.value,1,WorldShipmentSystem::MaximumShipment)); break;
        case Kind::Amount: quantity_.clear(); break;
        case Kind::All: quantity_=std::to_string(WorldShipmentSystem::available(*city,resource_)); break;
        case Kind::Once: repeating_=false; break;
        case Kind::Repeat: repeating_=true; break;
        case Kind::Cancel: resource_.clear(); scroll_=0; message_.clear(); break;
        case Kind::Destination: chooseDestination(world,actor,SettlementId{c.id}); break;
        case Kind::Stop:
            message_=WorldShipmentSystem::stop(world,actor,ShipmentId{c.id})==ShipmentResult::Success?
                "Stopping: current cargo completes its trip.":"Cannot stop this route."; break;
        case Kind::Up: --scroll_; break;
        case Kind::Down: ++scroll_; break;
        }
        layout(width_,height_,world,actor);
    }
    bool WorldSettlementPanel::handle(const SDL_Event& e,World& world,RealmId actor)
    {
        if(!isOpen()) return false;
        if(drag_.handle(e,bounds_)) { captured_=false; pressed_.reset(); layout(width_,height_,world,actor); return true; }
        if(e.type==SDL_EVENT_WINDOW_FOCUS_LOST) { editing_=captured_=false; pressed_.reset(); return false; }
        if(e.type==SDL_EVENT_KEY_DOWN && e.key.scancode==SDL_SCANCODE_ESCAPE)
        { if(choosingDestination()) { resource_.clear(); editing_=false; scroll_=0; layout(width_,height_,world,actor); } else close(); return true; }
        if(e.type==SDL_EVENT_KEY_DOWN && editing_)
        {
            int digit=-1;
            if(e.key.scancode>=SDL_SCANCODE_1 && e.key.scancode<=SDL_SCANCODE_9) digit=1+int(e.key.scancode-SDL_SCANCODE_1);
            else if(e.key.scancode==SDL_SCANCODE_0 || e.key.scancode==SDL_SCANCODE_KP_0) digit=0;
            else if(e.key.scancode>=SDL_SCANCODE_KP_1 && e.key.scancode<=SDL_SCANCODE_KP_9) digit=1+int(e.key.scancode-SDL_SCANCODE_KP_1);
            if(digit>=0 && quantity_.size()<7) quantity_+=char('0'+digit);
            if(e.key.scancode==SDL_SCANCODE_BACKSPACE && !quantity_.empty()) quantity_.pop_back();
            if(e.key.scancode==SDL_SCANCODE_RETURN || e.key.scancode==SDL_SCANCODE_KP_ENTER) editing_=false;
            layout(width_,height_,world,actor); return true;
        }
        if(e.type==SDL_EVENT_MOUSE_WHEEL && contains(e.wheel.mouse_x,e.wheel.mouse_y))
        { if(list_.contains(e.wheel.mouse_x,e.wheel.mouse_y)) { scroll_-=int(std::round(e.wheel.y)); layout(width_,height_,world,actor); } return true; }
        if(e.type==SDL_EVENT_MOUSE_MOTION)
        { mouseX_=e.motion.x; mouseY_=e.motion.y; return captured_ || contains(mouseX_,mouseY_); }
        if(e.type==SDL_EVENT_MOUSE_BUTTON_DOWN)
        {
            if(e.button.button==SDL_BUTTON_RIGHT && choosingDestination())
            { resource_.clear(); editing_=false; layout(width_,height_,world,actor); return true; }
            if(!contains(e.button.x,e.button.y)) { editing_=false; return false; }
            if(e.button.button==SDL_BUTTON_LEFT)
            { captured_=true; pressed_.reset(); for(const auto& c:controls_) if(c.enabled && c.bounds.contains(e.button.x,e.button.y)) { pressed_=c; break; } }
            return true;
        }
        if(e.type==SDL_EVENT_MOUSE_BUTTON_UP && e.button.button==SDL_BUTTON_LEFT)
        {
            const bool consume=captured_ || contains(e.button.x,e.button.y);
            const auto selected=pressed_; pressed_.reset(); captured_=false;
            if(selected) for(const auto& c:controls_)
                if(c.kind==selected->kind && c.id==selected->id && c.value==selected->value && c.bounds.contains(e.button.x,e.button.y))
                { const auto copy=c; act(copy,world,actor); break; }
            return consume;
        }
        return false;
    }
    std::optional<UiRectangle> WorldSettlementPanel::sendBounds(std::string_view id) const
    { for(const auto& c:controls_) if(c.kind==Kind::Send && SettlementResourceCatalog::definitions()[c.value].id==id) return c.bounds; return {}; }
    std::optional<UiRectangle> WorldSettlementPanel::destinationBounds(SettlementId id) const
    { for(const auto& c:controls_) if(c.kind==Kind::Destination && c.id==id.value()) return c.bounds; return {}; }
    std::optional<UiRectangle> WorldSettlementPanel::stopBounds(ShipmentId id) const
    { for(const auto& c:controls_) if(c.kind==Kind::Stop && c.id==id.value()) return c.bounds; return {}; }
    void WorldSettlementPanel::render(Renderer& renderer,const GrayUiRenderer& ui,const World& world,RealmId actor) const
    {
        const auto* city=world.settlement(city_); if(!city) return;
        ui.drawPanel(renderer,bounds_); BitmapFontRenderer font;
        auto text=[&](std::string value,float x,float y,float max,float size=1.5F)
        { while(!value.empty() && font.measureWidth(value,size)>max) value.pop_back(); ui.drawLabel(renderer,value,x,y,size); };
        const float x=bounds_.x+12,y=bounds_.y,w=bounds_.width,h=bounds_.height,side=w-24;
        text(std::string(city->name()),x,y+15,w-78,2);
        for(const auto& c:controls_)
        {
            if(c.kind==Kind::Amount) { ui.drawTextField(renderer,c.bounds,quantity_,"Amount",editing_); continue; }
            const bool selected=c.kind==Kind::Send?resource_==SettlementResourceCatalog::definitions()[c.value].id:c.kind==Kind::Once?!repeating_:c.kind==Kind::Repeat?repeating_:false;
            ui.drawButton(renderer,c.bounds,c.label,c.bounds.contains(mouseX_,mouseY_),pressed_ && pressed_->kind==c.kind && pressed_->id==c.id && pressed_->value==c.value,selected,c.enabled);
        }
        if(w<300 || h<480) { text("Enlarge the window to inspect resources.",x,y+60,side,1); return; }
        const auto* realm=world.realm(city->ownerRealmId());
        text(realm?std::string(realm->name()):"Unclaimed settlement",x,y+47,side);
        std::size_t garrison=MilitarySystem::available(world,city_);
        for(const auto& army:world.armies()) if(army.ownerRealmId()==city->ownerRealmId() && MilitarySystem::presentAt(world,army,city_)) garrison+=army.soldierCount();
        text("Population: "+std::to_string(city->population())+" | Garrison: "+std::to_string(garrison),x,y+69,side);
        text("Resources                     Total / available",x,y+103,side,1);
        const float pitch=std::clamp((h-360.F)/9.F,19.F,27.F);
        int index=0;
        for(const auto& resource:SettlementResourceCatalog::definitions())
        {
            text(std::string(resource.displayName),x+54,y+124+index*pitch,side-190,1.5F);
            text(std::to_string(std::int64_t(std::clamp(WorldShipmentSystem::total(*city,resource.id),0.,9e15)))+" / "+
                std::to_string(WorldShipmentSystem::available(*city,resource.id)),x+side-130,y+124+index*pitch,130,1.25F);
            ++index;
        }
        const float start=y+133+9*pitch;
        if(choosingDestination())
        {
            text("Send "+resource_+" per trip",x,start,side,1.5F);
            text("Choose your destination below or on the map.",x,start+88,side,1);
        }
        else
        {
            text("Outgoing caravans",x,start,side,1.5F);
            int count=0,shown=0; const int visible=std::max(1,int(list_.height/43));
            for(const auto& route:world.shipments()) if(route.source==city_ && route.active())
            {
                if(count++<scroll_ || shown>=visible) continue;
                const auto* target=world.settlement(route.destination);
                const std::string status=route.phase==ShipmentPhase::Waiting?"Waiting for stock":route.phase==ShipmentPhase::Blocked?"Blocked":route.phase==ShipmentPhase::Outbound?"Delivering":"Returning";
                const float row=list_.y+shown++*43.F;
                text(route.resource+" x"+std::to_string(route.cargo)+" -> "+(target?std::string(target->name()):"Lost city"),x,row,side-91,1.25F);
                text(status+" | "+(route.repeating?"sustained":"one trip"),x,row+17,side-91,1);
            }
            if(count==0) text(actor==city->ownerRealmId()?"Use Send beside a resource to dispatch a caravan.":"No outgoing caravans.",x,list_.y,side,1);
        }
        text(message_,x,y+h-33,side,1);
        text("Spare storage fills first; overflow goes near the keep.",x,y+h-17,side,1);
    }
}

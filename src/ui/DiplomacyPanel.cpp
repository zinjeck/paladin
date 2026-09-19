#include "ui/DiplomacyPanel.h"
#include "ui/GrayUiRenderer.h"
#include "simulation/DiplomacySystem.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
namespace Paladin
{
    void DiplomacyPanel::toggle() { if (open_) close(); else open(); }
    void DiplomacyPanel::open(RealmId realm)
    { drag_.cancel(); open_=true; selected_=realm; giftFor_={}; opinions_=false; opinionScroll_=0; scroll_=0; dirty_=true; pressed_.reset(); captured_=false; message_.clear(); }
    void DiplomacyPanel::close() noexcept
    { drag_.cancel(); open_=false; selected_={}; captured_=false; pressed_.reset(); controls_.clear(); stats_.clear(); dirty_=true; areaRevision_=~std::uint64_t{}; }
    void DiplomacyPanel::refresh(const World& world)
    {
        const auto revision=world.grid().revision() ^ (world.territory().revision()*1315423911ULL) ^
            (world.tribalInfluence().revision()*2654435761ULL);
        if (!dirty_ && statsMinute_==world.time().totalGameMinutes() && revision==areaRevision_) return;
        auto next=collectRealmStatistics(world,revision!=areaRevision_ || stats_.empty());
        if (revision==areaRevision_)
            for (auto& r:next)
                for (const auto& old:stats_) if (r.realm==old.realm) { r.size=old.size; break; }
        stats_=std::move(next); statsMinute_=world.time().totalGameMinutes(); areaRevision_=revision; dirty_=false;
        const auto compare=[&](const RealmStatistics& a,const RealmStatistics& b)
        {
            // Keep money and counts in their exact integer domains. On MSVC,
            // long double has only double precision and merges large balances.
            const auto order=[](auto x,auto y) { return (x>y)-(x<y); };
            switch(sort_) {
            case Sort::Soldiers:return order(a.soldiers,b.soldiers);
            case Sort::Gold:return order(a.gold,b.gold);
            case Sort::Size:return order(a.size,b.size);
            case Sort::Cities:return order(a.cities,b.cities);
            case Sort::Fortresses:return order(a.fortresses,b.fortresses);
            } return 0;
        };
        std::stable_sort(stats_.begin(),stats_.end(),[&](const auto& a,const auto& b)
        {
            const int order=compare(a,b);
            if (order!=0) return descending_ ? order>0 : order<0;
            const auto* ar=world.realm(a.realm); const auto* br=world.realm(b.realm);
            if (ar && br && ar->name()!=br->name()) return ar->name()<br->name();
            return a.realm.value()<b.realm.value();
        });
    }
    void DiplomacyPanel::layout(int width,int height,const World& world,RealmId actor)
    {
        width_=width; height_=height; if (!open_) return;
        refresh(world);
        if (selected_ && !world.realm(selected_)) selected_={};
        const float w=std::max(0.F,std::min(730.F,float(width)-24)), h=std::max(0.F,std::min(540.F,float(height)-24));
        bounds_=drag_.place({(width-w)*.5F,(height-h)*.5F,w,h},width,height); controls_.clear(); list_={}; opinionList_={};
        const auto add=[&](UiRectangle rect,Kind kind,int value,std::string label,bool enabled=true,RealmId realm=RealmId{})
        { controls_.push_back({rect,kind,value,realm,std::move(label),enabled}); };
        add({bounds_.x+w-42,bounds_.y+10,28,26},Kind::Close,0,"X");
        if (w<490 || h<360) return;
        const float x=bounds_.x+16,y=bounds_.y,left=std::floor((w-48)*.43F);
        static constexpr std::array<const char*,5> names{"Soldiers","Gold","Size","Cities","Fortresses"};
        const float tab=(left-8)/3;
        for (int i=0;i<5;++i)
            add({x+(i%3)*(tab+4),y+48+(i/3)*31,tab,27},Kind::Sort,i,names[i]);
        list_={x,y+116,left-18,h-144};
        const int visible=std::max(1,int(list_.height/38));
        scroll_=std::clamp(scroll_,0,std::max(0,int(stats_.size())-visible));
        for (int i=scroll_;i<std::min(int(stats_.size()),scroll_+visible);++i)
        {
            const auto& r=stats_[i]; const auto* realm=world.realm(r.realm); if (!realm) continue;
            add({x,list_.y+(i-scroll_)*38.F,list_.width,34},Kind::Realm,0,realm->name().empty()?"Realm "+std::to_string(r.realm.value()):std::string(realm->name()),true,r.realm);
        }
        add({x+left-14,list_.y,14,24},Kind::ScrollUp,0,"^",scroll_>0);
        add({x+left-14,list_.y+list_.height-24,14,24},Kind::ScrollDown,0,"v",scroll_+visible<int(stats_.size()));
        if (!selected_) return; // Right side intentionally blank until a realm is selected.
        const float right=x+left+16, side=w-48-left;
        if (giftFor_!=selected_) { giftFor_=selected_; gift_=DiplomacySystem::suggestedGift(world,actor,selected_); }
        add({right,y+160,side*.5F-3,25},Kind::ActionsTab,0,"Actions");
        add({right+side*.5F+3,y+160,side*.5F-3,25},Kind::OpinionsTab,0,"Nearby opinions");
        if (opinions_)
        {
            nearby_.clear();
            for (const auto& r:world.realms())
                if (DiplomacySystem::inRange(world,selected_,r.id())) nearby_.push_back(r.id());
            std::stable_sort(nearby_.begin(),nearby_.end(),[&](RealmId a,RealmId b){ return world.realm(a)->name()<world.realm(b)->name(); });
            opinionList_={right,y+216,side-20,h-278};
            const int count=std::max(1,int(opinionList_.height/32));
            opinionScroll_=std::clamp(opinionScroll_,0,std::max(0,int(nearby_.size())-count));
            add({right+side-16,opinionList_.y,16,24},Kind::OpinionUp,0,"^",opinionScroll_>0);
            add({right+side-16,opinionList_.y+opinionList_.height-24,16,24},Kind::OpinionDown,0,"v",opinionScroll_+count<int(nearby_.size()));
            return;
        }
        const auto* relation=world.diplomacy().between(actor,selected_);
        const bool reachable=DiplomacySystem::inRange(world,actor,selected_);
        const bool own=actor==selected_, war=relation && relation->atWar;
        const bool release=relation && relation->overlord==actor && relation->tributary==selected_;
        const std::array<const char*,6> actions{"Form Alliance","Send Gift",release?"Release Tributary":"Demand Tribute",
            "Establish Trade","Declare War","Establish Peace"};
        const float start=y+196, pitch=std::min(39.F,(h-282)/6), bh=pitch-5;
        for (int i=0;i<6;++i)
        {
            bool enabled=bool(actor) && !own && reachable;
            if (i==1) enabled=enabled && gift_>0 && world.realm(actor)->treasury && world.realm(actor)->treasury->balance>=gift_;
            if (i==0) enabled=enabled && !war && !(relation && relation->allied);
            if (i==2) enabled=enabled && (release || !war);
            if (i==3) enabled=enabled && !war && !(relation && relation->trading);
            if (i==4) enabled=enabled && !war;
            if (i==5) enabled=enabled && war;
            add({right,start+i*pitch,side,bh},Kind::Action,i,actions[i],enabled);
        }
        const float giftY=start+6*pitch+5;
        add({right,giftY,28,26},Kind::GiftLess,0,"-",reachable && gift_>0);
        add({right+side-28,giftY,28,26},Kind::GiftMore,0,"+",reachable && gift_<100000000);
    }
    std::optional<UiRectangle> DiplomacyPanel::sortBounds(Sort sort) const noexcept
    { for (const auto& c:controls_) if (c.kind==Kind::Sort && c.value==int(sort)) return c.bounds; return {}; }
    std::optional<UiRectangle> DiplomacyPanel::realmBounds(RealmId id) const noexcept
    { for (const auto& c:controls_) if (c.kind==Kind::Realm && c.realm==id) return c.bounds; return {}; }
    std::optional<UiRectangle> DiplomacyPanel::actionBounds(DiplomaticAction action) const noexcept
    { for (const auto& c:controls_) if (c.kind==Kind::Action && c.value==int(action)) return c.bounds; return {}; }
    void DiplomacyPanel::act(const Control& c,World& world,RealmId actor)
    {
        if (!c.enabled) return;
        switch(c.kind)
        {
        case Kind::Close: close(); return;
        case Kind::Sort:
            if (sort_==Sort(c.value)) descending_=!descending_; else { sort_=Sort(c.value); descending_=true; }
            scroll_=0; dirty_=true; break;
        case Kind::Realm: selected_=c.realm; giftFor_={}; opinions_=false; opinionScroll_=0; message_.clear(); break;
        case Kind::ActionsTab: opinions_=false; break;
        case Kind::OpinionsTab: opinions_=true; break;
        case Kind::OpinionUp: --opinionScroll_; break;
        case Kind::OpinionDown: ++opinionScroll_; break;
        case Kind::GiftLess: gift_=std::max<Money>(0,gift_-100); break;
        case Kind::GiftMore: gift_=std::min<Money>(100000000,gift_+100); break;
        case Kind::ScrollUp: --scroll_; break;
        case Kind::ScrollDown: ++scroll_; break;
        case Kind::Action:
            message_=diplomaticResultText(DiplomacySystem::apply(world,actor,selected_,DiplomaticAction(c.value),gift_));
            dirty_=true; break;
        }
        layout(width_,height_,world,actor);
    }
    bool DiplomacyPanel::handle(const SDL_Event& event,World& world,RealmId actor)
    {
        if (!open_) return false;
        if (drag_.handle(event,bounds_))
        { captured_=false; pressed_.reset(); layout(width_,height_,world,actor); return true; }
        if (event.type==SDL_EVENT_WINDOW_FOCUS_LOST) { captured_=false; pressed_.reset(); return false; }
        if (event.type==SDL_EVENT_KEY_DOWN && event.key.scancode==SDL_SCANCODE_ESCAPE) { close(); return true; }
        if (event.type==SDL_EVENT_MOUSE_WHEEL && contains(event.wheel.mouse_x,event.wheel.mouse_y))
        {
            if (list_.contains(event.wheel.mouse_x,event.wheel.mouse_y))
            { scroll_-=int(std::round(event.wheel.y)); layout(width_,height_,world,actor); }
            if (opinions_ && opinionList_.contains(event.wheel.mouse_x,event.wheel.mouse_y))
            { opinionScroll_-=int(std::round(event.wheel.y)); layout(width_,height_,world,actor); }
            return true;
        }
        if (event.type==SDL_EVENT_MOUSE_MOTION)
        { mouseX_=event.motion.x; mouseY_=event.motion.y; return captured_ || contains(mouseX_,mouseY_); }
        if (event.type==SDL_EVENT_MOUSE_BUTTON_DOWN && contains(event.button.x,event.button.y))
        {
            if (event.button.button==SDL_BUTTON_RIGHT) { close(); return true; }
            if (event.button.button==SDL_BUTTON_LEFT)
            {
                captured_=true; pressed_.reset();
                for (const auto& c:controls_) if (c.enabled && c.bounds.contains(event.button.x,event.button.y)) { pressed_=c; break; }
            }
            return true;
        }
        if (event.type==SDL_EVENT_MOUSE_BUTTON_UP && event.button.button==SDL_BUTTON_LEFT)
        {
            const bool consume=captured_ || contains(event.button.x,event.button.y);
            const auto pressed=pressed_; pressed_.reset(); captured_=false;
            if (pressed)
                for (const auto& c:controls_)
                    if (c.kind==pressed->kind && c.value==pressed->value && c.realm==pressed->realm &&
                        c.bounds.contains(event.button.x,event.button.y)) { const auto copy=c; act(copy,world,actor); break; }
            return consume;
        }
        return false;
    }
    void DiplomacyPanel::render(Renderer& renderer,const GrayUiRenderer& ui,const World& world,RealmId actor) const
    {
        if (!open_) return;
        ui.drawPanel(renderer,bounds_);
        BitmapFontRenderer font;
        const auto text=[&](std::string value,float x,float y,float width,float scale=1.5F)
        {
            while (!value.empty() && font.measureWidth(value,scale)>width) value.pop_back();
            ui.drawLabel(renderer,value,x,y,scale);
        };
        const float x=bounds_.x+16,y=bounds_.y,w=bounds_.width,h=bounds_.height;
        text("Diplomacy",x,y+14,w-76,3);
        for (const auto& c:controls_)
        {
            const bool selected=c.kind==Kind::Realm?c.realm==selected_:c.kind==Kind::Sort?Sort(c.value)==sort_:c.kind==Kind::ActionsTab?!opinions_:c.kind==Kind::OpinionsTab?opinions_:false;
            const bool pressed=pressed_ && pressed_->kind==c.kind && pressed_->value==c.value && pressed_->realm==c.realm;
            ui.drawButton(renderer,c.bounds,c.label,c.bounds.contains(mouseX_,mouseY_),pressed,selected,c.enabled);
        }
        if (w<490 || h<360) return;
        const float left=std::floor((w-48)*.43F), right=x+left+16, side=w-48-left;
        renderer.drawLine(right-8,y+44,right-8,y+h-36,{57,70,88,255});
        text(descending_?"Sorted highest first":"Sorted lowest first",x,y+108,left,1);
        const int visible=std::max(1,int(list_.height/38));
        if (int(stats_.size())>visible)
        {
            const float track=list_.height-56, thumb=std::max(12.F,track*visible/float(stats_.size()));
            const float offset=(track-thumb)*scroll_/std::max(1,int(stats_.size())-visible);
            renderer.fillRectangle(x+left-12,list_.y+28,10,track,{10,18,31,255});
            renderer.fillRectangle(x+left-12,list_.y+28+offset,10,thumb,{155,137,107,255});
        }
        text("Size: visible land area, not tribal ownership.",x,y+h-19,w-32,1);
        const auto* realm=world.realm(selected_); if (!realm) return;
        text(std::string(realm->name()),right,y+50,side,2);
        for (const auto& s:stats_) if (s.realm==selected_)
        {
            text(std::string(realm->usesTribalInfluence()?"Tribal":"Civic")+" | "+std::to_string(s.population)+" people",right,y+74,side);
            text(std::to_string(s.soldiers)+" soldiers | "+goldText(s.gold)+" gold",right,y+94,side);
            text(std::to_string(s.size)+" land | "+std::to_string(s.cities)+" cities | "+std::to_string(s.fortresses)+" fortresses",right,y+114,side,1);
        }
        const auto* relation=world.diplomacy().between(actor,selected_);
        std::string status=actor==selected_?"Your realm":relation && relation->atWar?"At war":"At peace";
        if (relation && relation->allied) status+=" | Alliance";
        if (relation && relation->trading) status+=" | Trade pact";
        if (relation && relation->tributary) status+=relation->tributary==selected_?" | Tributary":" | Overlord";
        text(status,right,y+130,side,1);
        const bool reachable=DiplomacySystem::inRange(world,actor,selected_);
        text(actor==selected_ ? "Your relations with neighbouring realms" : reachable ?
            "Their opinion of you: "+std::to_string(DiplomacySystem::opinion(world,selected_,actor)) :
            "Outside diplomatic range: actions unavailable",right,y+145,side,1);
        if (opinions_)
        {
            text("Realm | their view / neighbour's view",right,y+196,side,1);
            const int visible=std::max(1,int(opinionList_.height/32));
            if (nearby_.empty()) text("No realms within diplomatic range.",right,opinionList_.y,side,1);
            for (int i=opinionScroll_;i<std::min(int(nearby_.size()),opinionScroll_+visible);++i)
            {
                const auto id=nearby_[i]; const auto* other=world.realm(id); if (!other) continue;
                const float row=opinionList_.y+(i-opinionScroll_)*32.F;
                text(std::string(other->name()),right,row,side-105,1.25F);
                text(std::to_string(DiplomacySystem::opinion(world,selected_,id))+" / "+
                     std::to_string(DiplomacySystem::opinion(world,id,selected_)),right+side-100,row,80,1.25F);
            }
        }
        else
        {
            const float giftY=y+196+6*std::min(39.F,(h-282)/6)+11;
            text("Gift: "+goldText(gift_)+" gold",right+36,giftY,side-72,1.5F);
        }
        text(message_,right,y+h-45,side,1);
    }
}

#include "core/Application.h"
#include "interaction/SettlementPlacementController.h"
#include "rendering/Camera2D.h"
#include "rendering/GlobeView.h"
#include "rendering/Renderer.h"
#include "rendering/TileRenderMetrics.h"
#include "rendering/WorldMapNavigation.h"
#include "rendering/WorldRenderer.h"
#include "rendering/WorldArmyPresentation.h"
#include "simulation/Simulation.h"
#include "simulation/MilitarySystem.h"
#include "ui/FoundingPanel.h"
#include "ui/GrayUiRenderer.h"
#include "ui/MilitaryPanel.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <iterator>
#include <algorithm>
#include <vector>
namespace Paladin
{
    namespace
    {
        double worldPixels(const Camera2D& camera, const World& world, const Renderer& renderer,
                           const TileRenderMetrics& metrics, bool globe)
        {
            return globe ? GlobeView::from(camera,world.grid(),renderer.outputWidth(),renderer.outputHeight()).radius *
                6.283185307179586 / world.grid().width() : metrics.tilePixels*camera.zoom();
        }
    }
    void Application::renderMilitary()
    {
        if (!simulationControlsVisible()) return;
        auto& world=simulation_->world();
        militaryPanel_->layout(renderer_->outputWidth(),renderer_->outputHeight(),world,simulation_->playerRealmId());
        militaryPanel_->render(*renderer_,*grayUiRenderer_,world,simulation_->playerRealmId(),&worldRenderer_->artwork());
        const auto* unit=world.army(selectedWorldArmy_);
        if (screen_!=Screen::World || !unit || unit->ownerRealmId()!=simulation_->playerRealmId()) return;
        const double pixels=worldPixels(*camera_,world,*renderer_,*tileRenderMetrics_,worldRenderer_->globeEnabled);
        const auto position=WorldMapNavigation::annotationPosition(*camera_,world.grid(),renderer_->outputWidth(),renderer_->outputHeight(),pixels,
            worldRenderer_->globeEnabled,unit->visualX()+.5,unit->visualY()+.5);
        if (position)
        {
            const auto* sprite=worldArmySprite(worldRenderer_->artwork(),world,*unit);
            const auto body=worldArmySpriteBounds(position->x,position->y,pixels,sprite);
            const float left=std::min(body.x-3.F,float(position->x)-13.F);
            const float right=std::max(body.x+body.width+3.F,float(position->x)+13.F);
            // Keep the ground bracket above the count plate, never across its digits.
            const float ground=std::min(body.y+body.height+3.F,float(position->y)+6.F);
            for (int i=0;i<2;++i)
            {
                renderer_->drawLine(left-i,ground+i,right+i,ground+i,{235,196,107,255});
                renderer_->drawLine(left-i,ground-5,left-i,ground+i,{235,196,107,255});
                renderer_->drawLine(right+i,ground-5,right+i,ground+i,{235,196,107,255});
            }
        }
        if (!militaryPanel_->isOpen())
        {
            const UiRectangle status{16,76,std::min(480.F,float(renderer_->outputWidth())-32),70};
            grayUiRenderer_->drawPanel(*renderer_,status);
            const std::string label=unit->name()+" | "+std::to_string(unit->soldierCount())+" soldiers | "+std::to_string(unit->rations())+" rations";
            grayUiRenderer_->drawLabel(*renderer_,label,28,88,2);
            grayUiRenderer_->drawLabel(*renderer_,"Right-click land to march. Escape clears selection.",28,111,1);
            grayUiRenderer_->drawLabel(*renderer_,militaryOrderMessage_.substr(0,72),28,126,1,{235,196,107,255});
        }
    }
    bool Application::handleMilitaryEvent(const SDL_Event& event)
    {
        if (!simulation_ || !simulationControlsVisible()) return false;
        auto& world=simulation_->world();
        if (militaryPanel_->handle(event,world,simulation_->playerRealmId()))
        {
            const auto focus=militaryPanel_->takeFocus();
            if (focus)
            {
                if (screen_==Screen::City) returnToWorldFromCity();
                if (const auto* unit=world.army(focus))
                {
                    selectedWorldArmy_=focus;
                    WorldMapNavigation::focus(*camera_,world.grid(),renderer_->outputWidth(),renderer_->outputHeight(),worldRenderer_->globeEnabled,
                        {(unit->visualX()+.5)/world.grid().width(),(unit->visualY()+.5)/world.grid().height()});
                    militaryOrderMessage_="Unit selected.";
                }
            }
            return true;
        }
        if (screen_!=Screen::World || foundingPanel_->isOpen() || settlementPlacementController_->isActive()) return false;
        if (event.type==SDL_EVENT_KEY_DOWN && event.key.scancode==SDL_SCANCODE_ESCAPE && selectedWorldArmy_)
        { selectedWorldArmy_={}; militaryOrderMessage_.clear(); return true; }
        if (event.type==SDL_EVENT_MOUSE_BUTTON_UP && event.button.button==SDL_BUTTON_LEFT && militaryPointerCaptured_)
        { militaryPointerCaptured_=false; return true; }
        if (event.type!=SDL_EVENT_MOUSE_BUTTON_DOWN) return false;
        if (selectedWorldArmy_ && !militaryPanel_->isOpen())
        {
            const UiRectangle status{16,76,std::min(480.F,float(renderer_->outputWidth())-32),70};
            if (status.contains(event.button.x,event.button.y))
            {
                if (event.button.button==SDL_BUTTON_LEFT) militaryPointerCaptured_=true;
                return true;
            }
        }
        if (activeHudContainsPoint(event.button.x,event.button.y)) return false;
        const double pixels=worldPixels(*camera_,world,*renderer_,*tileRenderMetrics_,worldRenderer_->globeEnabled);
        if (event.button.button==SDL_BUTTON_LEFT)
        {
            std::vector<std::pair<double,ArmyId>> hits;
            for (const auto& unit:world.armies())
            {
                if (unit.ownerRealmId()!=simulation_->playerRealmId() || unit.soldierCount()==0) continue;
                const auto pos=WorldMapNavigation::annotationPosition(*camera_,world.grid(),renderer_->outputWidth(),renderer_->outputHeight(),pixels,
                    worldRenderer_->globeEnabled,unit.visualX()+.5,unit.visualY()+.5);
                if (!pos) continue;
                const double dx=pos->x-event.button.x,dy=pos->y-event.button.y,dist=dx*dx+dy*dy;
                const auto* sprite=worldArmySprite(worldRenderer_->artwork(),world,unit);
                if (worldArmyHitTest(event.button.x,event.button.y,pos->x,pos->y,pixels,sprite,unit.soldierCount()))
                    hits.emplace_back(dist,unit.id());
            }
            std::stable_sort(hits.begin(),hits.end(),[](const auto& a,const auto& b){return a.first<b.first;});
            if (!hits.empty())
            {
                auto choice=hits.begin();
                const auto current=std::find_if(hits.begin(),hits.end(),[&](const auto& h){return h.second==selectedWorldArmy_;});
                if (current!=hits.end()) { choice=std::next(current); if (choice==hits.end()) choice=hits.begin(); }
                selectedWorldArmy_=choice->second; militaryPointerCaptured_=true;
                militaryOrderMessage_=hits.size()>1?"Unit selected. Click the stack again to select another unit.":"Unit selected.";
                return true;
            }
        }
        if (event.button.button==SDL_BUTTON_RIGHT && selectedWorldArmy_)
        {
            const auto uv=WorldMapNavigation::pick(*camera_,world.grid(),renderer_->outputWidth(),renderer_->outputHeight(),pixels,
                worldRenderer_->globeEnabled,event.button.x,event.button.y);
            const auto result=uv?MilitarySystem::orderMove(world,simulation_->playerRealmId(),selectedWorldArmy_,
                {int(uv->u*world.grid().width()),int(uv->v*world.grid().height())}):MilitaryResult::InvalidDestination;
            militaryOrderMessage_=militaryResultText(result); return true;
        }
        return false;
    }
}

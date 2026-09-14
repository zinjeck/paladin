#include "core/Application.h"
#include "interaction/SettlementPlacementController.h"
#include "rendering/Camera2D.h"
#include "rendering/GlobeView.h"
#include "rendering/Renderer.h"
#include "rendering/TileRenderMetrics.h"
#include "rendering/WorldMapNavigation.h"
#include "rendering/WorldRenderer.h"
#include "simulation/Simulation.h"
#include "simulation/MilitarySystem.h"
#include "ui/FoundingPanel.h"
#include "ui/GrayUiRenderer.h"
#include "ui/MilitaryPanel.h"
#include <SDL3/SDL.h>
#include <cmath>
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
        militaryPanel_->render(*renderer_,*grayUiRenderer_,world,simulation_->playerRealmId());
        const auto* unit=world.army(selectedWorldArmy_);
        if (screen_!=Screen::World || !unit || unit->ownerRealmId()!=simulation_->playerRealmId()) return;
        const double pixels=worldPixels(*camera_,world,*renderer_,*tileRenderMetrics_,worldRenderer_->globeEnabled);
        const auto position=WorldMapNavigation::annotationPosition(*camera_,world.grid(),renderer_->outputWidth(),renderer_->outputHeight(),pixels,
            worldRenderer_->globeEnabled,unit->visualX()+.5,unit->visualY()+.5);
        if (position)
        {
            const float x=float(position->x), y=float(position->y);
            for (int i=0;i<2;++i)
            {
                renderer_->drawLine(x-13-i,y+16+i,x+13+i,y+16+i,{235,196,107,255});
                renderer_->drawLine(x-13-i,y+12+i,x-13-i,y+16+i,{235,196,107,255});
                renderer_->drawLine(x+13+i,y+12+i,x+13+i,y+16+i,{235,196,107,255});
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
        if (event.type!=SDL_EVENT_MOUSE_BUTTON_DOWN || activeHudContainsPoint(event.button.x,event.button.y)) return false;
        const double pixels=worldPixels(*camera_,world,*renderer_,*tileRenderMetrics_,worldRenderer_->globeEnabled);
        if (event.button.button==SDL_BUTTON_LEFT)
        {
            double best=22*22; ArmyId pick;
            for (const auto& unit:world.armies())
            {
                if (unit.ownerRealmId()!=simulation_->playerRealmId() || unit.soldierCount()==0) continue;
                const auto pos=WorldMapNavigation::annotationPosition(*camera_,world.grid(),renderer_->outputWidth(),renderer_->outputHeight(),pixels,
                    worldRenderer_->globeEnabled,unit.visualX()+.5,unit.visualY()+.5);
                if (!pos) continue;
                const double dx=pos->x-event.button.x,dy=pos->y-event.button.y,dist=dx*dx+dy*dy;
                if (dist<best) { best=dist; pick=unit.id(); }
            }
            if (pick) { selectedWorldArmy_=pick; militaryPointerCaptured_=true; militaryOrderMessage_="Unit selected."; return true; }
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

#include "core/Application.h"
#include "ui/WorldSettlementPanel.h"
#include "ui/CaravanPanel.h"
#include "ui/DiplomacyPanel.h"
#include "interaction/SettlementPlacementController.h"
#include "rendering/Camera2D.h"
#include "rendering/GlobeView.h"
#include "rendering/Renderer.h"
#include "rendering/TileRenderMetrics.h"
#include "rendering/WorldMapNavigation.h"
#include "rendering/WorldRenderer.h"
#include "rendering/WorldArmyPresentation.h"
#include "rendering/WorldCaravanPresentation.h"
#include "ui/LedgerPanel.h"
#include "ui/EmploymentPanel.h"
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
#include <limits>
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
    void Application::focusWorldSettlement(SettlementId id)
    {
        if (screen_ != Screen::World || !simulation_ || !camera_ || !worldRenderer_) return;
        const auto& world = simulation_->world();
        const auto* city = world.settlement(id);
        if (!city) return;
        const double pixels = worldPixels(*camera_, world, *renderer_, *tileRenderMetrics_, worldRenderer_->globeEnabled);
        camera_->setWorldZoom(camera_->zoom() * 32. / std::max(.001, pixels));
        WorldMapNavigation::focus(*camera_, world.grid(), renderer_->outputWidth(), renderer_->outputHeight(),
            worldRenderer_->globeEnabled, {(city->position().x+.5)/world.grid().width(), (city->position().y+.5)/world.grid().height()});
        globePointerDown_ = globeDragging_ = false;
        clampCameraToWorld();
    }
    bool Application::handleCaravanEvent(const SDL_Event& event)
    {
        if (screen_ != Screen::World || !simulation_ || foundingPanel_->isOpen() ||
            settlementPlacementController_->isActive()) return false;
        auto& world = simulation_->world();
        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
            event.button.button == SDL_BUTTON_LEFT && caravanPointerCaptured_)
        {
            caravanPointerCaptured_ = false;
            globePointerDown_ = globeDragging_ = false;
            return true;
        }
        if (caravanPanel_->handle(event, world, simulation_->playerRealmId())) return true;
        if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) caravanPointerCaptured_ = false;
        if (event.type != SDL_EVENT_MOUSE_BUTTON_DOWN || event.button.button != SDL_BUTTON_LEFT ||
            worldSettlementPanel_->choosingDestination() || activeHudContainsPoint(event.button.x, event.button.y)) return false;
        const double pixels = worldPixels(*camera_, world, *renderer_, *tileRenderMetrics_, worldRenderer_->globeEnabled);
        // Settlement symbols own their compact plate even when a caravan is
        // stopped at the same destination. The caravan remains selectable
        // outside the plate and from its shared logistics panel.
        for(const auto& city:world.settlements())
        {
            const auto p=WorldMapNavigation::annotationPosition(*camera_,world.grid(),renderer_->outputWidth(),renderer_->outputHeight(),pixels,worldRenderer_->globeEnabled,city.position().x+.5,city.position().y+.5);
            if(p && std::abs(event.button.x-p->x)<=11 && std::abs(event.button.y-p->y)<=14) return false;
        }
        ShipmentId hit;
        double best = std::numeric_limits<double>::max();
        for (const auto& caravan : world.shipments())
        {
            if (!worldCaravanVisible(caravan, pixels)) continue;
            const auto p = WorldMapNavigation::annotationPosition(*camera_, world.grid(), renderer_->outputWidth(),
                renderer_->outputHeight(), pixels, worldRenderer_->globeEnabled, caravan.visualX()+.5, caravan.visualY()+.5);
            if (!p || !worldCaravanBounds(p->x, p->y, pixels).contains(event.button.x, event.button.y)) continue;
            const double distance = std::hypot(p->x - event.button.x, p->y - event.button.y);
            if (distance < best) { best = distance; hit = caravan.id; }
        }
        if (!hit) return false;
        selectedWorldArmy_ = {}; militaryOrderMessage_.clear();
        diplomacyPanel_->close(); worldSettlementPanel_->close(); militaryPanel_->close(); ledgerPanel_->close(); employmentPanel_->close();
        caravanPanel_->open(hit);
        worldRenderer_->selectedCaravan = hit;
        worldRenderer_->selectedArmy = {};
        worldRenderer_->selectedRealm = {};
        globePointerDown_ = globeDragging_ = false;
        caravanPanel_->layout(renderer_->outputWidth(), renderer_->outputHeight(), world, simulation_->playerRealmId());
        caravanPointerCaptured_ = true;
        return true;
    }
    void Application::renderMilitary()
    {
        if (!simulationControlsVisible()) return;
        auto& world=simulation_->world();
        militaryPanel_->layout(renderer_->outputWidth(),renderer_->outputHeight(),world,simulation_->playerRealmId());
        militaryPanel_->render(*renderer_,*grayUiRenderer_,world,simulation_->playerRealmId(),&worldRenderer_->artwork());
        // Selection is shown by the world sprite contour and count only.
        // No floating status panel is allowed to cover the realm HUD.
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
                    caravanPanel_->close();
                    const double pixels=worldPixels(*camera_,world,*renderer_,*tileRenderMetrics_,worldRenderer_->globeEnabled);
                    if (pixels<20.) camera_->setWorldZoom(camera_->zoom()*20./std::max(.001,pixels));
                    diplomacyPanel_->close(); worldSettlementPanel_->close();
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
        if (worldSettlementPanel_->choosingDestination()) return false;
        if (activeHudContainsPoint(event.button.x,event.button.y)) return false;
        const double pixels=worldPixels(*camera_,world,*renderer_,*tileRenderMetrics_,worldRenderer_->globeEnabled);
        if (worldArmyVisibility(pixels)<=.001F) return false;
        if (event.button.button==SDL_BUTTON_LEFT)
        {
            std::vector<std::pair<double,ArmyId>> hits;
            for (const auto& unit:world.armies())
            {
                if (unit.garrisoned() ||
                    unit.ownerRealmId() != simulation_->playerRealmId() ||
                    unit.soldierCount() == 0)
                {
                    continue;
                }
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
                if (hits.size()==1 && choice->second==selectedWorldArmy_)
                {
                    selectedWorldArmy_={}; militaryOrderMessage_.clear(); militaryPointerCaptured_=true;
                    return true;
                }
                caravanPanel_->close();
                selectedWorldArmy_=choice->second; diplomacyPanel_->close(); worldSettlementPanel_->close(); militaryPointerCaptured_=true;
                militaryOrderMessage_=hits.size()>1?"Unit selected. Click the stack again to select another unit.":"Unit selected.";
                return true;
            }
        }
        if (event.button.button==SDL_BUTTON_LEFT && selectedWorldArmy_)
        {
            // Blank-land click clears the contour but continues to ordinary
            // settlement/realm picking. Clicking HUD never clears selection.
            selectedWorldArmy_={}; militaryOrderMessage_.clear();
        }
        if (event.button.button==SDL_BUTTON_RIGHT && selectedWorldArmy_)
        {
            if (const auto target =
                    attackTargetAt(event.button.x, event.button.y))
            {
                militaryOrderMessage_ = militaryResultText(
                    BattleSystem::orderAttack(
                        world,
                        simulation_->playerRealmId(),
                        selectedWorldArmy_,
                        target
                    )
                );
                globePointerDown_ = globeDragging_ = false;
                updateBattleEncounter();
                return true;
            }
            const auto uv=WorldMapNavigation::pick(*camera_,world.grid(),renderer_->outputWidth(),renderer_->outputHeight(),pixels,
                worldRenderer_->globeEnabled,event.button.x,event.button.y);
            const auto result=uv?MilitarySystem::orderMove(world,simulation_->playerRealmId(),selectedWorldArmy_,
                {int(uv->u*world.grid().width()),int(uv->v*world.grid().height())}):MilitaryResult::InvalidDestination;
            militaryOrderMessage_=militaryResultText(result); return true;
        }
        return false;
    }
}

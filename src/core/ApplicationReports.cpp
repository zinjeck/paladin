#include "core/Application.h"
#include "ui/DiplomacyPanel.h"
#include "interaction/SettlementCommandController.h"
#include "interaction/SettlementInspectionController.h"
#include "interaction/SettlementObjectPlacementController.h"
#include "platform/Window.h"
#include "rendering/Renderer.h"
#include "simulation/Simulation.h"
#include "ui/CityHud.h"
#include "ui/EmploymentPanel.h"
#include "ui/LedgerPanel.h"
#include "ui/MilitaryPanel.h"
#include "ui/SettlementInspectionPanel.h"
#include <SDL3/SDL.h>
namespace Paladin
{
    void Application::updateReports()
    {
        simulation_->reports.update(
            simulation_->world(),
            simulation_->playerRealmId()
        );
        ledgerPanel_->refresh(*simulation_);
        ledgerPanel_->layout(
            renderer_->outputWidth(),
            renderer_->outputHeight()
        );
    }
    bool Application::handleReportAction(CityHudAction action)
    {
        if (action == CityHudAction::Diplomacy && screen_ == Screen::World)
        {
            employmentPanel_->close(); ledgerPanel_->close(); militaryPanel_->close();
            selectedWorldArmy_ = {}; militaryOrderMessage_.clear();
            diplomacyPanel_->toggle();
            diplomacyPanel_->layout(renderer_->outputWidth(),renderer_->outputHeight(),simulation_->world(),simulation_->playerRealmId());
            return true;
        }
        if (action != CityHudAction::None) diplomacyPanel_->close();
        if (action == CityHudAction::Military)
        {
            employmentPanel_->close(); ledgerPanel_->close();
            settlementInspectionController_->clear();
            settlementInspectionPanel_->clearLayout();
            settlementObjectPlacementController_->cancelPlacement();
            settlementCommandController_->cancel();
            SDL_StopTextInput(window_->nativeHandle());
            militaryPanel_->toggle(screen_ == Screen::City ? activeCitySettlementId_ : simulation_->presentedSettlementId());
            militaryPanel_->layout(renderer_->outputWidth(), renderer_->outputHeight(), simulation_->world(), simulation_->playerRealmId());
            return true;
        }
        if (action != CityHudAction::None) militaryPanel_->close();
        if (action != CityHudAction::Ledger && action != CityHudAction::Events)
        {
            return false;
        }
        employmentPanel_->close();
        settlementInspectionController_->clear();
        settlementInspectionPanel_->clearLayout();
        settlementObjectPlacementController_->cancelPlacement();
        settlementCommandController_->cancel();
        SDL_StopTextInput(window_->nativeHandle());
        simulation_->reports
            .update(simulation_->world(), simulation_->playerRealmId(), true);
        ledgerPanel_->toggle(
            action == CityHudAction::Events,
            screen_ == Screen::World,
            activeCitySettlementId_
        );
        updateReports();
        return true;
    }
    bool Application::handleReportEvent(const SDL_Event& event)
    {
        if (ledgerPanel_->handle(event))
        {
            return true;
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
            event.button.button == SDL_BUTTON_LEFT &&
            activeHudContainsPoint(event.button.x, event.button.y) &&
            !cityHud_->reportControlAt(event.button.x, event.button.y))
        {
            ledgerPanel_->close();
        }
        return false;
    }
} // namespace Paladin

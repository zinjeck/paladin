#include "core/Application.h"
#include "interaction/SettlementCommandController.h"
#include "interaction/SettlementInspectionController.h"
#include "interaction/SettlementObjectPlacementController.h"
#include "platform/Window.h"
#include "rendering/Renderer.h"
#include "simulation/Simulation.h"
#include "ui/CityHud.h"
#include "ui/EmploymentPanel.h"
#include "ui/LedgerPanel.h"
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
            activeSettlementId_
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

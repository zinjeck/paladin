#include "core/Application.h"
#include "core/SimulationClock.h"
#include "interaction/GlobeCameraNavigation.h"
#include "interaction/SettlementCommandController.h"
#include "interaction/SettlementInspectionController.h"
#include "interaction/SettlementObjectPlacementController.h"
#include "interaction/SettlementPlacementController.h"
#include "platform/Window.h"
#include "rendering/Camera2D.h"
#include "rendering/CityRenderer.h"
#include "rendering/Renderer.h"
#include "rendering/TileRenderMetrics.h"
#include "rendering/WorldRenderer.h"
#include "simulation/Simulation.h"
#include "ui/CityHud.h"
#include "ui/DebugConsole.h"
#include "ui/EmploymentPanel.h"
#include "ui/FoundingPanel.h"
#include "ui/LedgerPanel.h"
#include "ui/MainMenu.h"
#include "ui/SettlementInspectionPanel.h"
#include "ui/SimulationSpeedControls.h"
#include "ui/WorldHud.h"
#include "world/World.h"
#include "world/settlements/SettlementMap.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <memory>

namespace Paladin
{
    void Application::startWorldSession()
    {
        ledgerPanel_->close();
        simulation_ = std::make_unique<Simulation>();

        camera_ = std::make_unique<Camera2D>(
            static_cast<double>(simulation_->world().grid().width()) * 0.5,
            static_cast<double>(simulation_->world().grid().height()) * 0.5
        );

        settlementPlacementController_ =
            std::make_unique<SettlementPlacementController>();

        settlementObjectPlacementController_ =
            std::make_unique<SettlementObjectPlacementController>();

        settlementCommandController_ =
            std::make_unique<SettlementCommandController>();

        worldRenderer_ = std::make_unique<WorldRenderer>();
        worldRenderer_->globeEnabled = true;

        cityRenderer_ = std::make_unique<CityRenderer>();

        tileRenderMetrics_ = std::make_unique<TileRenderMetrics>();

        tileRenderMetrics_->tilePixels = 4.0;

        edgeScrollDwellSeconds_ = 0.0;
        movingCapital_ = false;
        savedWorldCamera_.reset();
        activeSettlementId_ = {};
        cityHudCapturedPointer_ = false;
        simulationControlsUnlocked_ = false;
        foundingAdditionalSettlement_ = false;
        worldHud_->setAdditionalSelection(false);
        simulationControlsCapturedPointer_ = false;
        settlementInspectionController_->clear();
        settlementInspectionPanel_->clearLayout();
        simulationClock_->reset();
        screen_ = Screen::World;
        cityHud_->setWorldMode(simulationControlsUnlocked_);
        employmentPanel_->setWorldMode(true);
    }

    void Application::endWorldSession()
    {
        ledgerPanel_->close();
        debugConsole_->reset();
        cachedStats_.clear();
        nextStatsRefresh_ = 0;
        foundingPanel_->close();
        SDL_StopTextInput(window_->nativeHandle());

        tileRenderMetrics_.reset();
        settlementCameras_.clear();
        cityRenderer_.reset();
        worldRenderer_.reset();
        settlementPlacementController_.reset();
        settlementObjectPlacementController_.reset();
        settlementCommandController_.reset();
        settlementInspectionController_->clear();
        settlementInspectionPanel_->clearLayout();
        camera_.reset();
        savedWorldCamera_.reset();
        simulation_.reset();

        edgeScrollDwellSeconds_ = 0.0;
        movingCapital_ = false;
        activeSettlementId_ = {};
        cityHudCapturedPointer_ = false;
        simulationControlsUnlocked_ = false;
        foundingAdditionalSettlement_ = false;
        worldHud_->setAdditionalSelection(false);
        simulationControlsCapturedPointer_ = false;
        simulationClock_->reset();
        screen_ = Screen::MainMenu;

        mainMenu_->layout(renderer_->outputWidth(), renderer_->outputHeight());
    }

    void Application::enterPresentedSettlement()
    {
        ledgerPanel_->close();
        if (screen_ != Screen::World || !simulation_ || !camera_ ||
            !tileRenderMetrics_)
        {
            return;
        }

        const Realm* realm =
            simulation_->world().realm(simulation_->playerRealmId());

        if (!realm || !realm->capitalSettlementId().isValid())
        {
            return;
        }

        const SettlementId settlementId =
            simulation_->presentedSettlementId()
                ? simulation_->presentedSettlementId()
                : realm->capitalSettlementId();
        Settlement* settlement = simulation_->world().settlement(settlementId);

        if (!settlement || !simulation_->prepareSettlementMap(settlementId) ||
            !simulation_->setPresentedSettlement(settlementId) ||
            !simulation_->setDetailedSimulationSettlement(settlementId))
        {
            return;
        }

        const SettlementMap* settlementMap =
            simulation_->settlementMap(settlementId);

        if (!settlementMap)
        {
            static_cast<void>(simulation_->clearDetailedSimulationSettlement());
            return;
        }

        savedWorldCamera_ = std::make_unique<Camera2D>(*camera_);

        camera_ = std::make_unique<Camera2D>(
            static_cast<double>(settlementMap->grid().width()) * 0.5,
            static_cast<double>(settlementMap->grid().height()) * 0.5
        );

        tileRenderMetrics_->tilePixels = 2.0;
        settlement->simulationState().citizens().placeUnpositionedCitizens(
            *settlementMap
        );
        if (!cityRenderer_)
        {
            cityRenderer_ = std::make_unique<CityRenderer>();
        }
        for (const auto& saved : settlementCameras_)
        {
            if (saved.first == settlementId)
            {
                *camera_ = *saved.second;
            }
        }
        activeSettlementId_ = settlementId;
        cityHud_->setWorldMode(false);
        employmentPanel_->setWorldMode(false);
        edgeScrollDwellSeconds_ = 0.0;
        settlementPlacementController_->cancelSelection();
        settlementObjectPlacementController_->cancelPlacement();
        settlementCommandController_->cancel();
        settlementInspectionController_->clear();
        settlementInspectionPanel_->clearLayout();
        cityHudCapturedPointer_ = false;
        simulationControlsUnlocked_ = true;
        simulationControlsCapturedPointer_ = false;
        movingCapital_ = false;
        employmentPanel_->close();
        employmentCapturedPointer_ = false;
        cityHud_->setSettlementStatus(
            settlementMap->logistics.founded(),
            settlement->population()
        );
        screen_ = Screen::City;

        simulationSpeedControls_->layout(renderer_->outputWidth());
        simulationSpeedControls_->setPlaybackState(
            simulationClock_->isPaused(),
            simulationClock_->speedMultiplier()
        );

        clampCameraToWorld();
    }

    void Application::returnToWorldFromSettlement()
    {
        ledgerPanel_->close();
        employmentPanel_->close();
        SDL_StopTextInput(window_->nativeHandle());
        if (screen_ != Screen::City || !simulation_)
        {
            return;
        }

        const Settlement* leavingSettlement =
            simulation_->world().settlement(activeSettlementId_);
        const std::optional<WorldTilePosition> leavingPosition =
            leavingSettlement
                ? std::optional<WorldTilePosition>(leavingSettlement->position())
                : std::nullopt;

        settlementObjectPlacementController_->cancelPlacement();
        settlementCommandController_->cancel();
        settlementInspectionController_->clear();
        settlementInspectionPanel_->clearLayout();
        cityHudCapturedPointer_ = false;
        simulationControlsCapturedPointer_ = false;

        if (!simulation_->clearDetailedSimulationSettlement())
        {
            return;
        }

        auto saved = std::find_if(
            settlementCameras_.begin(),
            settlementCameras_.end(),
            [&](const auto& entry)
            { return entry.first == activeSettlementId_; }
        );
        if (saved == settlementCameras_.end())
        {
            settlementCameras_.push_back(
                {activeSettlementId_, std::make_unique<Camera2D>(*camera_)}
            );
        }
        else
        {
            *saved->second = *camera_;
        }
        if (savedWorldCamera_)
        {
            camera_ = std::move(savedWorldCamera_);
        }
        else
        {
            camera_ = std::make_unique<Camera2D>(
                static_cast<double>(simulation_->world().grid().width()) * 0.5,
                static_cast<double>(simulation_->world().grid().height()) * 0.5
            );
        }

        if (leavingPosition)
        {
            static_cast<void>(GlobeCameraNavigation::focusNorthUp(
                *camera_,
                simulation_->world().grid(),
                *leavingPosition
            ));
        }

        tileRenderMetrics_->tilePixels = 4.0;
        activeSettlementId_ = {};
        edgeScrollDwellSeconds_ = 0.0;
        screen_ = Screen::World;
        cityHud_->setWorldMode(simulationControlsUnlocked_);
        employmentPanel_->setWorldMode(true);

        worldHud_->setSimulationControlsUnlocked(true);
        worldHud_->layout(renderer_->outputWidth(), renderer_->outputHeight());

        clampCameraToWorld();
    }

    void Application::cancelFoundingFlow()
    {
        foundingAdditionalSettlement_ = false;
        worldHud_->setAdditionalSelection(false);
        foundingPanel_->close();
        SDL_StopTextInput(window_->nativeHandle());
        settlementPlacementController_->cancelSelection();
        movingCapital_ = false;
    }

    void Application::confirmFoundingFlow()
    {
        if (!foundingPanel_->isOpen() || !foundingPanel_->canConfirm())
        {
            return;
        }

        const FoundingPanelMode mode = foundingPanel_->mode();
        const FoundingIdentity identity = foundingPanel_->identity();

        bool completed = false;
        SettlementId settlementId;

        if (mode == FoundingPanelMode::Founding)
        {
            const std::optional<WorldTilePosition> lockedPosition =
                settlementPlacementController_->lockedPosition();

            if (!lockedPosition)
            {
                cancelFoundingFlow();
                return;
            }

            settlementId =
                simulation_->foundPlayerCapital(*lockedPosition, identity);
            completed = settlementId.isValid();
        }
        else if (mode == FoundingPanelMode::NewSettlement)
        {
            const auto position =
                settlementPlacementController_->lockedPosition();
            if (!position)
            {
                return;
            }
            settlementId = simulation_->foundPlayerSettlement(
                *position,
                identity.capitalName
            );
            completed = bool(settlementId);
        }
        else if (mode == FoundingPanelMode::RenameCapital)
        {
            completed = simulation_->renamePlayerCapital(identity.capitalName);
        }
        else
        {
            completed = simulation_->editPlayerRealm(identity);
        }

        if (!completed)
        {
            return;
        }

        foundingPanel_->close();
        SDL_StopTextInput(window_->nativeHandle());
        settlementPlacementController_->cancelSelection();

        foundingAdditionalSettlement_ = false;
        worldHud_->setAdditionalSelection(false);
        if (mode == FoundingPanelMode::NewSettlement && settlementId)
        {
            enterPresentedSettlement();
        }
        if (settlementId.isValid())
        {
            SDL_Log(
                "Founded player settlement %llu.",
                static_cast<unsigned long long>(settlementId.value())
            );
        }
    }

} // namespace Paladin

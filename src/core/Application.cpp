#include "core/Application.h"
#include "ui/DebugConsole.h"
#include "ui/LedgerPanel.h"

#include "core/SimulationClock.h"
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
#include "ui/EmploymentPanel.h"
#include "ui/FoundingPanel.h"
#include "ui/GrayUiRenderer.h"
#include "ui/MainMenu.h"
#include "ui/SettlementInspectionPanel.h"
#include "ui/SimulationSpeedControls.h"
#include "ui/WorldHud.h"
#include "world/settlements/SettlementMap.h"

#include <SDL3/SDL.h>

#include <memory>

namespace Paladin
{
    Application::Application()
    {
        if (!SDL_Init(SDL_INIT_VIDEO))
        {
            SDL_Log("SDL_Init failed: %s", SDL_GetError());

            return;
        }

        sdlInitialized_ = true;

        window_ = std::make_unique<Window>("Paladin", 1280, 720);

        if (!window_->isValid())
        {
            window_.reset();
            return;
        }

        renderer_ = std::make_unique<Renderer>(window_->nativeHandle());

        if (!renderer_->isValid())
        {
            renderer_.reset();
            return;
        }

        simulationClock_ = std::make_unique<SimulationClock>(20.0);

        grayUiRenderer_ = std::make_unique<GrayUiRenderer>();

        mainMenu_ = std::make_unique<MainMenu>();

        worldHud_ = std::make_unique<WorldHud>();

        employmentPanel_ = std::make_unique<EmploymentPanel>();
        debugConsole_ = std::make_unique<DebugConsole>();
        cityHud_ = std::make_unique<CityHud>();
        ledgerPanel_ = std::make_unique<LedgerPanel>();

        simulationSpeedControls_ = std::make_unique<SimulationSpeedControls>();

        foundingPanel_ = std::make_unique<FoundingPanel>();

        settlementInspectionController_ =
            std::make_unique<SettlementInspectionController>();

        settlementInspectionPanel_ =
            std::make_unique<SettlementInspectionPanel>();
    }

    Application::~Application()
    {
        if (window_)
        {
            SDL_StopTextInput(window_->nativeHandle());
        }

        tileRenderMetrics_.reset();
        cityCameras_.clear();
        cityRenderer_.reset();
        worldRenderer_.reset();
        settlementPlacementController_.reset();
        settlementObjectPlacementController_.reset();
        settlementInspectionPanel_.reset();
        settlementInspectionController_.reset();
        camera_.reset();
        savedWorldCamera_.reset();
        simulation_.reset();

        worldHud_.reset();
        debugConsole_.reset();
        employmentPanel_.reset();
        ledgerPanel_.reset();
        cityHud_.reset();
        simulationSpeedControls_.reset();
        foundingPanel_.reset();
        mainMenu_.reset();
        grayUiRenderer_.reset();
        simulationClock_.reset();
        renderer_.reset();
        window_.reset();

        if (sdlInitialized_)
        {
            SDL_Quit();
        }
    }

    int Application::run()
    {

        if (!sdlInitialized_ || !window_ || !renderer_ || !simulationClock_ ||
            !grayUiRenderer_ || !mainMenu_ || !worldHud_ || !cityHud_ ||
            !simulationSpeedControls_ || !foundingPanel_ ||
            !settlementInspectionController_ || !settlementInspectionPanel_)
        {
            return 1;
        }

        bool running = true;
        while (running)
        {
            simulationClock_->beginFrame();
            layoutFrame();
            // Preserve the frame's control-visibility snapshot while draining
            // events.
            const bool controlsVisible = simulationControlsVisible();
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                if (!handleEvent(event, controlsVisible))
                {
                    running = false;
                }
            }
            if (!running)
            {
                break;
            }
            updateFrame();
            renderFrame();
        }
        return 0;
    }

} // namespace Paladin

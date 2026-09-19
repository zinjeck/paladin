#include "core/Application.h"
#include "rendering/BattleScene.h"
#include "ui/CaravanPanel.h"
#include "ui/DebugConsole.h"
#include "ui/DiplomacyPanel.h"
#include "ui/LedgerPanel.h"
#include "ui/MilitaryPanel.h"
#include "ui/WorldSettlementPanel.h"

#include "core/SimulationClock.h"
#include "interaction/SettlementCommandController.h"
#include "interaction/SettlementInspectionController.h"
#include "interaction/SettlementObjectPlacementController.h"
#include "interaction/SettlementPlacementController.h"
#include "platform/Window.h"
#include "rendering/Camera2D.h"
#include "rendering/CityRenderer.h"
#include "rendering/Renderer.h"
#include "rendering/SceneSpriteLibrary.h"
#include "assets/AssetManager.h"
#include <algorithm>
#include <filesystem>
#include <stdexcept>
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
        startupReady_ = loadStartupAssets();
        if (!startupReady_) return;

        mainMenu_ = std::make_unique<MainMenu>();

        worldHud_ = std::make_unique<WorldHud>();

        employmentPanel_ = std::make_unique<EmploymentPanel>();
        debugConsole_ = std::make_unique<DebugConsole>();
        cityHud_ = std::make_unique<CityHud>();
        ledgerPanel_ = std::make_unique<LedgerPanel>();
        militaryPanel_ = std::make_unique<MilitaryPanel>();
        diplomacyPanel_ = std::make_unique<DiplomacyPanel>();
        worldSettlementPanel_ = std::make_unique<WorldSettlementPanel>();
        caravanPanel_ = std::make_unique<CaravanPanel>();

        simulationSpeedControls_ = std::make_unique<SimulationSpeedControls>();

        foundingPanel_ = std::make_unique<FoundingPanel>();

        settlementInspectionController_ =
            std::make_unique<SettlementInspectionController>();

        settlementInspectionPanel_ =
            std::make_unique<SettlementInspectionPanel>();
    }

    bool Application::loadStartupAssets()
    {
        // Event pumping and every GPU upload remain on the SDL/main thread.
        // Progress advances only for finished asset jobs, never for elapsed time.
        double lastDrawn = -1;
        Uint64 lastFrame = 0;
        const auto draw = [&](double fraction, std::string_view label)
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
                if (event.type == SDL_EVENT_QUIT ||
                    (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE))
                    startupCancelled_ = true;
            if (startupCancelled_) throw std::runtime_error("Loading cancelled");
            startupProgress_ = std::max(startupProgress_, std::clamp(fraction, 0., 1.));
            const auto now = SDL_GetTicks();
            if (startupProgress_ < 1 && lastDrawn >= 0 && startupProgress_ - lastDrawn < .01 && now - lastFrame < 16)
                return;
            lastDrawn = startupProgress_; lastFrame = now;
            renderer_->beginFrame();
            grayUiRenderer_->drawMainMenuBackground(*renderer_);
            const float w = float(renderer_->outputWidth()), h = float(renderer_->outputHeight());
            const float barWidth = std::min(620.F, w - 48.F), x = (w - barWidth) * .5F, y = h * .55F;
            grayUiRenderer_->drawTitle(*renderer_, "PALADIN", w * .5F, y - 90.F);
            grayUiRenderer_->drawPanel(*renderer_, {x, y, barWidth, 28.F});
            renderer_->fillRectangle(x + 4, y + 4, float((barWidth - 8) * startupProgress_), 20,
                {235, 196, 107, 255});
            std::string message(label.substr(0, std::min<std::size_t>(label.size(), 65)));
            grayUiRenderer_->drawLabel(*renderer_, message, x, y + 44, 1.5F);
            grayUiRenderer_->drawLabel(*renderer_, std::to_string(int(startupProgress_ * 100)) + "%",
                x + barWidth - 42, y - 24, 1.5F);
            renderer_->endFrame();
            ++startupProgressFrames_;
        };
        try
        {
            draw(0, "Opening asset packages");
            renderer_->compiledAssets([&](std::size_t done, std::size_t total, std::string_view stage)
            { draw(.02 + .73 * double(done) / double(std::max<std::size_t>(1, total)), stage); });
            SceneSpriteLibrary sprites;
            sprites.load(*renderer_, (std::filesystem::path(SDL_GetBasePath()) / "assets/sprites").string(),
                [&](std::size_t done, std::size_t total, std::string_view stage)
                { draw(.75 + .24 * double(done) / double(std::max<std::size_t>(1, total)), stage); });
            if (startupCancelled_) return false;
            if (!sprites.ready()) throw std::runtime_error("Compiled sprite catalogue is missing or invalid. Reinstall the assets/packages directory.");
            draw(1, "Ready");
            return true;
        }
        catch (const std::exception& error)
        {
            if (!startupCancelled_)
            {
                startupError_ = error.what();
                SDL_Log("Startup loading failed: %s", error.what());
            }
            return false;
        }
    }

    Application::~Application()
    {
        battleScene_.reset();
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

        caravanPanel_.reset();
        worldSettlementPanel_.reset();
        diplomacyPanel_.reset();
        worldHud_.reset();
        debugConsole_.reset();
        employmentPanel_.reset();
        militaryPanel_.reset();
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

        if (startupCancelled_) return 0;
        if (!startupReady_)
        {
            if (!startupError_.empty())
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Paladin asset loading failed",
                    startupError_.c_str(), window_ ? window_->nativeHandle() : nullptr);
            return 1;
        }
        if (!sdlInitialized_ || !window_ || !renderer_ || !simulationClock_ ||
            !grayUiRenderer_ || !mainMenu_ || !worldHud_ || !cityHud_ ||
            !simulationSpeedControls_ || !foundingPanel_ ||
            !settlementInspectionController_ || !settlementInspectionPanel_)
        {
            return 1;
        }

        if (SDL_getenv("PALADIN_WORLD_BENCHMARK"))
        {
            return runWorldBenchmark();
        }
        if (SDL_getenv("PALADIN_CAMERA_BENCHMARK"))
        {
            return runCameraBenchmark();
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

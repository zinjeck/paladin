#pragma once
#include "ui/UiTooltip.h"
#include <string>
#include <string_view>
#include <vector>

#include "core/StrongId.h"
#include "interaction/CameraNavigationPolicy.h"
#include "world/SettlementTilePosition.h"
#include "world/WorldTilePosition.h"

#include <memory>
#include <optional>

union SDL_Event;

namespace Paladin
{
    class Camera2D;
    class CityHud;
    class LedgerPanel;
    enum class CityHudAction;
    class EmploymentPanel;
    class DebugConsole;
    class CityRenderer;
    class GrayUiRenderer;
    class FoundingPanel;
    class MainMenu;
    class Renderer;
    class SettlementPlacementController;
    class SettlementObjectPlacementController;
    class SettlementCommandController;
    class SettlementInspectionController;
    class SettlementInspectionPanel;
    class Simulation;
    class SettlementMap;
    class SettlementCitizenState;
    class SimulationClock;
    class SimulationSpeedControls;
    class Window;
    class WorldHud;
    class WorldRenderer;

    struct TileRenderMetrics;

    class Application
    {
    public:
        Application();
        ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        int run();

    private:
        friend struct ApplicationSmokeTest;

        enum class Screen
        {
            MainMenu,
            World,
            City
        };

        // Frame orchestration. Modal input helpers report event consumption;
        // handleEvent and handleMainMenuEvent return false only on exit.
        bool simulationControlsVisible() const noexcept;
        void layoutFrame();
        void updateFrame();
        void renderFrame();
        bool handleEvent(const SDL_Event& event, bool controlsVisible);
        bool handleDebugEvent(const SDL_Event& event);
        bool handleSimulationControlEvent(const SDL_Event& event);
        void handleCameraZoomEvent(const SDL_Event& event);

        // Main menu.
        void layoutMainMenu();
        bool handleMainMenuEvent(const SDL_Event& event);
        void renderMainMenu();

        // Local settlement screen. "City" remains a presentation synonym;
        // the domain entity is always Settlement.
        void layoutCityScreen();
        void updateCityScreen();
        void synchronizeCityStatus();
        void updateCityHud();
        void updateReports();
        bool handleReportEvent(const SDL_Event&);
        bool handleReportAction(CityHudAction);
        void renderCityScreen();
        void handleCityEvent(const SDL_Event& event);
        bool handleCityRenameEvent(const SDL_Event& event, SettlementMap& map);
        bool handleCityEmploymentEvent(
            const SDL_Event& event,
            SettlementMap& map,
            SettlementCitizenState& citizens
        );
        void handleCityPointerMotion(const SDL_Event& event);
        void handleCityPointerPressed(const SDL_Event& event);
        void handleCityPointerReleased(const SDL_Event& event);

        // World screen and founding flow input.
        void layoutWorldScreen();
        void updateWorldScreen();
        void renderWorldScreen();
        void handleWorldEvent(const SDL_Event& event);
        void handleFoundingEvent(const SDL_Event& event);
        void handleWorldPointerPressed(const SDL_Event& event);
        void handleWorldPointerReleased(const SDL_Event& event);

        // Canonical settlement lifecycle. Legacy city/capital-named wrappers
        // remain temporarily so presentation code can migrate incrementally.
        void startWorldSession();
        void endWorldSession();
        void enterPresentedSettlement();
        void enterPlayerCapitalCity()
        {
            enterPresentedSettlement();
        }
        void executeConsoleCommand(std::string_view text);
        void renderDebug();
        void renderCityTooltip();
        UiTooltip tooltip_;
        void returnToWorldFromSettlement();
        void returnToWorldFromCity()
        {
            returnToWorldFromSettlement();
        }

        void cancelFoundingFlow();
        void confirmFoundingFlow();

        void updateCameraMovement(double frameDeltaSeconds);
        void updateCameraZoom(double frameDeltaSeconds);
        void clampCameraToWorld() noexcept;

        [[nodiscard]]
        bool activeHudContainsPoint(float x, float y) const noexcept;

        [[nodiscard]]
        std::optional<SettlementTilePosition> cityTileAtScreen(
            double screenX,
            double screenY
        ) const noexcept;

        void updateSettlementPlacementHover(double screenX, double screenY);

        void applyCameraZoom(double multiplier, double screenX, double screenY);

        bool sdlInitialized_ = false;
        Screen screen_ = Screen::MainMenu;

        std::unique_ptr<Window> window_;
        std::unique_ptr<Renderer> renderer_;
        std::unique_ptr<SimulationClock> simulationClock_;

        std::unique_ptr<GrayUiRenderer> grayUiRenderer_;
        std::unique_ptr<MainMenu> mainMenu_;
        std::unique_ptr<WorldHud> worldHud_;
        std::unique_ptr<CityHud> cityHud_;
        std::unique_ptr<LedgerPanel> ledgerPanel_;
        std::unique_ptr<EmploymentPanel> employmentPanel_;
        std::unique_ptr<DebugConsole> debugConsole_;
        std::vector<std::pair<SettlementId, std::unique_ptr<Camera2D>>>
            cityCameras_;
        std::string cachedStats_;
        std::uint64_t nextStatsRefresh_ = 0;
        bool employmentCapturedPointer_ = false;
        std::unique_ptr<SimulationSpeedControls> simulationSpeedControls_;
        std::unique_ptr<FoundingPanel> foundingPanel_;

        std::unique_ptr<Simulation> simulation_;
        std::unique_ptr<Camera2D> camera_;
        std::unique_ptr<SettlementPlacementController>
            settlementPlacementController_;
        std::unique_ptr<SettlementObjectPlacementController>
            settlementObjectPlacementController_;
        std::unique_ptr<SettlementCommandController>
            settlementCommandController_;
        std::unique_ptr<SettlementInspectionController>
            settlementInspectionController_;
        std::unique_ptr<SettlementInspectionPanel> settlementInspectionPanel_;
        std::unique_ptr<WorldRenderer> worldRenderer_;
        std::unique_ptr<CityRenderer> cityRenderer_;
        std::unique_ptr<TileRenderMetrics> tileRenderMetrics_;

        CameraNavigationPolicy cameraNavigationPolicy_ =
            defaultCameraNavigationPolicy();
        double edgeScrollDwellSeconds_ = 0.0;
        bool movingCapital_ = false;
        bool foundingAdditionalSettlement_ = false;
        bool handleWorldManagement(const SDL_Event& event);
        void renderWorldManagement();
        std::unique_ptr<Camera2D> savedWorldCamera_;
        SettlementId activeCitySettlementId_;
        bool cityHudCapturedPointer_ = false;
        bool globePointerDown_ = false, globeDragging_ = false;
        int worldNavigatorPress_ = 0;
        float globePressX_ = 0, globePressY_ = 0;
        bool simulationControlsUnlocked_ = false;
        bool simulationControlsCapturedPointer_ = false;
    };
} // namespace Paladin

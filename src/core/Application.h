#pragma once

#include "core/StrongId.h"
#include "interaction/CameraNavigationPolicy.h"
#include "world/WorldTilePosition.h"
#include "world/SettlementTilePosition.h"

#include <memory>
#include <optional>

namespace Paladin
{
    class Camera2D;
    class CityHud;
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
        enum class Screen
        {
            MainMenu,
            World,
            City
        };

        void startWorldSession();
        void endWorldSession();
        void enterPlayerCapitalCity();
        void returnToWorldFromCity();

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

        void updateSettlementPlacementHover(
            double screenX,
            double screenY
        );

        void applyCameraZoom(
            double multiplier,
            double screenX,
            double screenY
        );

        bool sdlInitialized_ = false;
        Screen screen_ = Screen::MainMenu;

        std::unique_ptr<Window> window_;
        std::unique_ptr<Renderer> renderer_;
        std::unique_ptr<SimulationClock> simulationClock_;

        std::unique_ptr<GrayUiRenderer> grayUiRenderer_;
        std::unique_ptr<MainMenu> mainMenu_;
        std::unique_ptr<WorldHud> worldHud_;
        std::unique_ptr<CityHud> cityHud_;
        std::unique_ptr<SimulationSpeedControls>
            simulationSpeedControls_;
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
        std::unique_ptr<SettlementInspectionPanel>
            settlementInspectionPanel_;
        std::unique_ptr<WorldRenderer> worldRenderer_;
        std::unique_ptr<CityRenderer> cityRenderer_;
        std::unique_ptr<TileRenderMetrics>
            tileRenderMetrics_;

        CameraNavigationPolicy cameraNavigationPolicy_ =
            defaultCameraNavigationPolicy();
        double edgeScrollDwellSeconds_ = 0.0;
        bool movingCapital_ = false;
        std::unique_ptr<Camera2D> savedWorldCamera_;
        SettlementId activeCitySettlementId_;
        bool cityHudCapturedPointer_ = false;
        bool simulationControlsUnlocked_ = false;
        bool simulationControlsCapturedPointer_ = false;
    };
}

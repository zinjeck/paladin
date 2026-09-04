#pragma once

#include "interaction/CameraNavigationPolicy.h"

#include <memory>

namespace Paladin
{
    class Camera2D;
    class GrayUiRenderer;
    class FoundingPanel;
    class MainMenu;
    class Renderer;
    class SettlementPlacementController;
    class Simulation;
    class SimulationClock;
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
            World
        };

        void startWorldSession();
        void endWorldSession();

        void cancelFoundingFlow();
        void confirmFoundingFlow();

        void updateCameraMovement(double frameDeltaSeconds);
        void updateCameraZoom(double frameDeltaSeconds);
        void clampCameraToWorld() noexcept;

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
        std::unique_ptr<FoundingPanel> foundingPanel_;

        std::unique_ptr<Simulation> simulation_;
        std::unique_ptr<Camera2D> camera_;
        std::unique_ptr<SettlementPlacementController>
            settlementPlacementController_;
        std::unique_ptr<WorldRenderer> worldRenderer_;
        std::unique_ptr<TileRenderMetrics>
            tileRenderMetrics_;

        CameraNavigationPolicy cameraNavigationPolicy_ =
            defaultCameraNavigationPolicy();
        double edgeScrollDwellSeconds_ = 0.0;
        bool movingCapital_ = false;
    };
}

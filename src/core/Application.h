#pragma once

#include <memory>

namespace Paladin
{
    class Camera2D;
    class GrayUiRenderer;
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

        void updateCameraMovement(double frameDeltaSeconds);
        void updateCameraZoom(double frameDeltaSeconds);

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

        std::unique_ptr<Simulation> simulation_;
        std::unique_ptr<Camera2D> camera_;
        std::unique_ptr<SettlementPlacementController>
            settlementPlacementController_;
        std::unique_ptr<WorldRenderer> worldRenderer_;
        std::unique_ptr<TileRenderMetrics>
            tileRenderMetrics_;
    };
}

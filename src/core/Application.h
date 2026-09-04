#pragma once

#include <memory>

namespace Paladin
{
    class Window;
    class Renderer;
    class SimulationClock;
    class Simulation;
    class Camera2D;
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
        void updateCameraMovement(double frameDeltaSeconds);
        void updateCameraZoom(double frameDeltaSeconds);
        void applyCameraZoom(
            double multiplier,
            double screenX,
            double screenY
        );

        bool sdlInitialized_ = false;

        std::unique_ptr<Window> window_;
        std::unique_ptr<Renderer> renderer_;
        std::unique_ptr<SimulationClock> simulationClock_;
        std::unique_ptr<Simulation> simulation_;
        std::unique_ptr<Camera2D> camera_;
        
        std::unique_ptr<WorldRenderer>
            worldRenderer_;
        
        std::unique_ptr<TileRenderMetrics>
            tileRenderMetrics_;
    };
}

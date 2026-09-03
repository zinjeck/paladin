#pragma once

#include <memory>

namespace Paladin
{
    class Window;
    class Renderer;
    class SimulationClock;

    class Application
    {
    public:
        Application();
        ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        int run();

    private:
        void simulationTick(double deltaSeconds);

        bool sdlInitialized_ = false;

        std::unique_ptr<Window> window_;
        std::unique_ptr<Renderer> renderer_;
        std::unique_ptr<SimulationClock> simulationClock_;
    };
}
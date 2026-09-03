#pragma once

#include <memory>

namespace Paladin
{
    class Window;

    class Application
    {
    public:
        Application();
        ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        int run();

    private:
        bool sdlInitialized_ = false;
        std::unique_ptr<Window> window_;
    };
}
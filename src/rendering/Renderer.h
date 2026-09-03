#pragma once

struct SDL_Renderer;
struct SDL_Window;

namespace Paladin
{
    class Renderer
    {
    public:
        explicit Renderer(SDL_Window* window);
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        bool isValid() const noexcept;

        void beginFrame();
        void endFrame();

    private:
        SDL_Renderer* renderer_ = nullptr;
    };
}
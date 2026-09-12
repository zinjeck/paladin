#include "WorldSurfaceRenderingChecks.h"
#include "platform/Window.h"
#include <exception>
#include <iostream>

int main()
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << '\n';
        return 1;
    }
    int result = 0;
    try
    {
        Paladin::Window window("Paladin world rendering validation", 960, 640);
        PALADIN_CHECK(window.isValid());
        SDL_HideWindow(window.nativeHandle());
        Paladin::Renderer renderer(window.nativeHandle());
        PALADIN_CHECK(renderer.isValid());
        Paladin::Test::worldSurfaceRenderingChecks(renderer, window.nativeHandle());
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    SDL_Quit();
    return result;
}

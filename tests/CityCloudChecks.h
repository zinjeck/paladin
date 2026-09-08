#pragma once
#include "TestFramework.h"
#include "rendering/CityClouds.h"
#include <SDL3/SDL.h>
#include <filesystem>
namespace Paladin
{
    inline void cityCloudChecks(Renderer& r, SDL_Renderer* native)
    {
        CityClouds clouds;
        SceneProjection p{40, 30, 4, r.outputWidth(), r.outputHeight()};
        auto draw = [&](double t, bool sky, bool enabled, const char* name)
        {
            r.beginFrame();
            r.fillRectangle(
                0,
                0,
                float(p.screenWidth),
                float(p.screenHeight),
                {90, 125, 85, 255}
            );
            if (enabled)
            {
                clouds.render(r, p, t, 1, sky, 256, 256);
            }
            auto* surface = SDL_RenderReadPixels(native, nullptr);
            PALADIN_CHECK(surface);
            if (name)
            {
                if (const char* root = SDL_getenv("PALADIN_SMOKE_SCREENSHOTS"))
                {
                    SDL_SaveBMP(
                        surface,
                        (std::filesystem::path(root) / name).string().c_str()
                    );
                }
            }
            std::uint64_t hash = 1469598103934665603ULL;
            for (int y = 0; y < surface->h; ++y)
            {
                for (int x = 0; x < surface->w; ++x)
                {
                    Uint8 red, green, blue, alpha;
                    SDL_ReadSurfacePixel(
                        surface,
                        x,
                        y,
                        &red,
                        &green,
                        &blue,
                        &alpha
                    );
                    hash = (hash ^ red) * 1099511628211ULL;
                    hash = (hash ^ green) * 1099511628211ULL;
                    hash = (hash ^ blue) * 1099511628211ULL;
                }
            }
            SDL_DestroySurface(surface);
            return hash;
        };
        const auto clear = draw(0, true, false, nullptr);
        const auto far = draw(120, true, true, "cloud-body-far.bmp");
        PALADIN_CHECK(far != clear);
        PALADIN_CHECK(far == draw(120, true, true, nullptr));
        PALADIN_CHECK(far != draw(240, true, true, "cloud-body-moving.bmp"));
        p.tilePixels = 32;
        PALADIN_CHECK(clear == draw(120, true, true, nullptr));
        PALADIN_CHECK(clear != draw(120, false, true, "cloud-shadow-near.bmp"));
    }
} // namespace Paladin

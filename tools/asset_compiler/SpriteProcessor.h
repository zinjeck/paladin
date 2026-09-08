#pragma once
#include "rendering/SunlitMaterials.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace Paladin
{
    // Shared material-detail standard, applied once at import to every sprite.
    // Match the final common world grid; fitting cannot bypass that grid.
    inline SDL_Surface* simplifySprite(
        SDL_Surface* source,
        double width,
        double height,
        int frames,
        bool applySunlight = false
    )
    {
        auto* rgba = SDL_ConvertSurface(source, SDL_PIXELFORMAT_RGBA32);
        if (!rgba)
        {
            return nullptr;
        }
        const int fw = rgba->w / frames;
        const int tw = std::min(
            fw,
            std::max(1, int(std::round(width * 16)))
        );
        const int th = std::min(
            rgba->h,
            std::max(1, int(std::round(height * 16)))
        );
        auto* output =
            SDL_CreateSurface(tw * frames, th, SDL_PIXELFORMAT_RGBA32);
        if (!output)
        {
            SDL_DestroySurface(rgba);
            return nullptr;
        }
        std::vector<std::array<Uint8, 4>> pixels(std::size_t(tw) * frames * th);
        const int stride = tw * frames;
        for (int y = 0; y < th; ++y)
        {
            for (int x = 0; x < stride; ++x)
            {
                const int sx = (x / tw) * fw +
                               std::min(fw - 1, int((x % tw + .5) * fw / tw)),
                          sy = std::min(
                              rgba->h - 1,
                              int((y + .5) * rgba->h / th)
                          );
                const auto* c = static_cast<const Uint8*>(rgba->pixels) +
                                sy * rgba->pitch + sx * 4;
                const auto original =
                    (unsigned(c[0]) << 16) | (unsigned(c[1]) << 8) | c[2];
                const auto color =
                    applySunlight ? sunlitMaterial(original) : original;
                pixels[std::size_t(y) * stride + x] =
                    {Uint8(color >> 16), Uint8(color >> 8), Uint8(color), c[3]};
            }
        }
        for (int y = 0; y < th; ++y)
        {
            std::copy_n(
                reinterpret_cast<const Uint8*>(
                    pixels.data() + std::size_t(y) * stride
                ),
                stride * 4,
                static_cast<Uint8*>(output->pixels) + y * output->pitch
            );
        }
        SDL_DestroySurface(rgba);
        return output;
    }
} // namespace Paladin

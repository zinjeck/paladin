#pragma once
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace Paladin
{
    // Shared material-detail standard, applied once at import to every sprite.
    // Preserve authored texture and edges at the restored 32-texel tile density.
    inline SDL_Surface* simplifySprite(
        SDL_Surface* source,
        double width,
        double height,
        int frames
    )
    {
        auto* rgba = SDL_ConvertSurface(source, SDL_PIXELFORMAT_RGBA32);
        if (!rgba)
        {
            return nullptr;
        }
        const int fw = rgba->w / frames;
        const int tw = std::min(fw, std::max(4, int(std::round(width * 32))));
        const int th =
            std::min(rgba->h, std::max(4, int(std::round(height * 32))));
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
                pixels[std::size_t(y) * stride + x] = {c[0], c[1], c[2], c[3]};
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

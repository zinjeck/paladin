#pragma once

#include "TestFramework.h"
#include "rendering/Camera2D.h"
#include "rendering/Renderer.h"
#include "rendering/SceneSpriteLibrary.h"
#include "rendering/TileRenderMetrics.h"
#include "rendering/WorldGridRenderer.h"
#include "world/SettlementGrid.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <array>
#include <filesystem>
#include <fstream>

namespace Paladin
{
    inline void environmentTerrainSmokeChecks(
        Renderer& renderer,
        SDL_Renderer* native
    )
    {
        const auto directory = std::filesystem::path(SDL_GetBasePath()) /
                               "terrain-renderer-test-fixture";
        std::filesystem::create_directories(directory);
        std::ofstream(directory / "art-palette.hex")
            << "#FF0000\n#00FF00\n#0000FF\n#FFFF00\n";
        std::ofstream(directory / "sprites.catalog")
            << "terrain.plain module.png 2 2 0 0 0 0\n";
        std::array<RenderColor, 4> colors{
            {{255, 0, 0, 255},
             {0, 255, 0, 255},
             {0, 0, 255, 255},
             {255, 255, 0, 255}}
        };
        const auto saveModule = [&]()
        {
            auto* surface = SDL_CreateSurfaceFrom(
                2,
                2,
                SDL_PIXELFORMAT_RGBA32,
                colors.data(),
                8
            );
            PALADIN_CHECK(surface);
            PALADIN_CHECK(IMG_SavePNG(
                surface,
                (directory / "module.png").string().c_str()
            ));
            SDL_DestroySurface(surface);
        };
        saveModule();
        SceneSpriteLibrary library;
        library.load(renderer, directory.string());
        PALADIN_CHECK(library.find("terrain.plain"));
        SettlementGrid grid(514, 514);
        for (int y = 0; y < grid.height(); ++y)
        {
            for (int x = 0; x < grid.width(); ++x)
            {
                grid.tile({x, y})->terrain = TerrainType::Land;
                grid.tile({x, y})->biome = BiomeType::Plain;
            }
        }
        WorldGridRenderer ground;
        Camera2D camera(32, 32);
        camera.setZoom(8);
        TileRenderMetrics metrics{4};
        const auto draw = [&]()
        {
            renderer.beginFrame();
            ground.render(renderer, grid, camera, metrics, &library);
        };
        const auto settle = [&]()
        {
            const auto until = SDL_GetTicks() + 500;
            do
            {
                draw();
                SDL_Delay(1);
            } while (SDL_GetTicks() < until);
        };
        const auto pixel = [&](int x, int y)
        {
            auto* surface = SDL_RenderReadPixels(native, nullptr);
            PALADIN_CHECK(surface);
            RenderColor color;
            PALADIN_CHECK(SDL_ReadSurfacePixel(
                surface,
                x,
                y,
                &color.red,
                &color.green,
                &color.blue,
                &color.alpha
            ));
            SDL_DestroySurface(surface);
            return color;
        };
        const int cx = renderer.outputWidth() / 2;
        const int cy = renderer.outputHeight() / 2;
        settle();
        // 2x2 source modules remain continuous across the 32-tile chunk
        // boundary. A cache must not stretch a whole module into each tile.
        const auto northwest = pixel(cx - 16, cy - 16);
        const auto northeast = pixel(cx + 16, cy - 16);
        const auto southeast = pixel(cx + 16, cy + 16);
        PALADIN_CHECK(
            northwest.red == 255 && northwest.green == 255 &&
            northwest.blue == 0
        );
        PALADIN_CHECK(
            northeast.red == 0 && northeast.green == 0 && northeast.blue == 255
        );
        PALADIN_CHECK(
            southeast.red == 255 && southeast.green == 0 && southeast.blue == 0
        );
        renderer.endFrame();
        // Zoom into uncached regions and across cache-resolution boundaries.
        // Every visible sample must contain art on the first rendered frame.
        camera.setPosition(256, 256);
        for (double zoom : {.5, 1., 2., 4., 8., 16., 4., 2., 1.})
        {
            camera.setZoom(zoom);
            draw();
            for (int dy : {-260, 0, 260})
            {
                for (int dx : {-500, 0, 500})
                {
                    const auto p = pixel(cx + dx, cy + dy);
                    PALADIN_CHECK(
                        (p.red || p.green || p.blue) && p.alpha == 255 &&
                        !(p.red == 92 && p.green == 166 && p.blue == 64)
                    );
                }
            }
            renderer.endFrame();
        }
        camera.setPosition(32, 32);
        camera.setZoom(8);
        SceneSpriteLibrary::setEnvironmentArtEnabled(false);
        draw();
        const auto disabled = pixel(cx + 16, cy + 16);
        PALADIN_CHECK(
            disabled.red == 92 && disabled.green == 166 && disabled.blue == 64
        );
        renderer.endFrame();
        SceneSpriteLibrary::setEnvironmentArtEnabled(true);
        draw();
        PALADIN_CHECK(pixel(cx + 16, cy + 16).red == 255);
        renderer.endFrame();
        // Reloading changed artwork invalidates already composed chunks.
        std::swap(colors[0], colors[1]);
        saveModule();
        library.reset();
        library.load(renderer, directory.string());
        settle();
        const auto reloaded = pixel(cx + 16, cy + 16);
        PALADIN_CHECK(
            reloaded.red == 0 && reloaded.green == 255 && reloaded.blue == 0
        );
        renderer.endFrame();
    }
} // namespace Paladin

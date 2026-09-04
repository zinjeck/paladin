#include "rendering/WorldGridRenderer.h"

#include "rendering/Camera2D.h"
#include "rendering/Renderer.h"
#include "rendering/Texture.h"
#include "rendering/TileRenderMetrics.h"
#include "world/BiomeType.h"
#include "world/TerrainType.h"
#include "world/WorldGrid.h"
#include "world/WorldTile.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Paladin
{
    namespace
    {
        RenderColor biomeColor(
            BiomeType biome
        ) noexcept
        {
            switch (biome)
            {
                case BiomeType::Plain:
                    return {92, 166, 64, 255};

                case BiomeType::Forest:
                    return {26, 107, 41, 255};

                case BiomeType::Jungle:
                    return {5, 92, 23, 255};

                case BiomeType::Desert:
                    return {219, 184, 92, 255};

                case BiomeType::Tundra:
                    return {163, 184, 173, 255};

                case BiomeType::Taiga:
                    return {51, 97, 82, 255};

                case BiomeType::Ocean:
                    return {13, 41, 92, 255};
            }

            return {255, 0, 255, 255};
        }

        RenderColor tileColor(
            const WorldTile& tile
        ) noexcept
        {
            if (tile.terrain == TerrainType::Water)
            {
                return biomeColor(BiomeType::Ocean);
            }

            if (tile.terrain == TerrainType::Mountain)
            {
                return {115, 107, 97, 255};
            }

            return biomeColor(tile.biome);
        }
    }


    WorldGridRenderer::WorldGridRenderer() = default;


    WorldGridRenderer::~WorldGridRenderer() = default;


    void WorldGridRenderer::render(
        Renderer& renderer,
        const WorldGrid& grid,
        const Camera2D& camera,
        const TileRenderMetrics& metrics
    ) const
    {
        const double tilePixels =
            metrics.scaledTilePixels(camera.zoom());

        if (tilePixels <= 0.0)
        {
            return;
        }

        if (cachedGrid_ != &grid)
        {
            cachedGrid_ = &grid;
            cachedTerrainTexture_.reset();
            cacheBuildAttempted_ = false;
        }

        if (!cacheBuildAttempted_)
        {
            cacheBuildAttempted_ = true;
            std::vector<RenderColor> pixels(grid.tileCount());

            for (std::int32_t y = 0; y < grid.height(); ++y)
            {
                for (std::int32_t x = 0; x < grid.width(); ++x)
                {
                    pixels[
                        static_cast<std::size_t>(y)
                            * static_cast<std::size_t>(grid.width())
                        + static_cast<std::size_t>(x)
                    ] = tileColor(*grid.tile({x, y}));
                }
            }

            cachedTerrainTexture_ =
                renderer.createTextureFromPixels(
                    grid.width(),
                    grid.height(),
                    pixels
                );
        }

        if (!cachedTerrainTexture_)
        {
            // Never fall back to per-tile drawing. A failed cache should
            // remain visible as a rendering failure, not freeze the game.
            return;
        }

        const double viewportWidth =
            static_cast<double>(renderer.outputWidth());

        const double viewportHeight =
            static_cast<double>(renderer.outputHeight());

        renderer.drawTexture(
            *cachedTerrainTexture_,
            0.0F,
            0.0F,
            static_cast<float>(grid.width()),
            static_cast<float>(grid.height()),
            static_cast<float>(
                viewportWidth * 0.5
                - camera.tileX() * tilePixels
            ),
            static_cast<float>(
                viewportHeight * 0.5
                - camera.tileY() * tilePixels
            ),
            static_cast<float>(
                static_cast<double>(grid.width()) * tilePixels
            ),
            static_cast<float>(
                static_cast<double>(grid.height()) * tilePixels
            )
        );
    }
}

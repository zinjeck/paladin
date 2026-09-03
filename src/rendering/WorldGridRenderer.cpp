#include "rendering/WorldGridRenderer.h"

#include "rendering/Camera2D.h"
#include "rendering/Renderer.h"
#include "rendering/TileRenderMetrics.h"

#include "world/TerrainType.h"
#include "world/WorldGrid.h"
#include "world/WorldTile.h"
#include "world/WorldTilePosition.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Paladin
{
    namespace
    {
        RenderColor terrainColor(
            TerrainType terrain
        ) noexcept
        {
            switch (terrain)
            {
                case TerrainType::Land:
                    return {
                        78,
                        118,
                        70,
                        255
                    };

                case TerrainType::Mountain:
                    return {
                        112,
                        108,
                        102,
                        255
                    };

                case TerrainType::Water:
                    return {
                        46,
                        83,
                        122,
                        255
                    };
            }

            return {
                255,
                0,
                255,
                255
            };
        }
    }


    void WorldGridRenderer::render(
        Renderer& renderer,
        const WorldGrid& grid,
        const Camera2D& camera,
        const TileRenderMetrics& metrics
    ) const
    {
        const double tilePixels =
            metrics.scaledTilePixels(
                camera.zoom()
            );

        if (tilePixels <= 0.0)
        {
            return;
        }

        const double viewportWidth =
            static_cast<double>(
                renderer.outputWidth()
            );

        const double viewportHeight =
            static_cast<double>(
                renderer.outputHeight()
            );

        const double halfVisibleTilesX =
            viewportWidth
            / (2.0 * tilePixels);

        const double halfVisibleTilesY =
            viewportHeight
            / (2.0 * tilePixels);


        const std::int32_t minimumX =
            std::max<std::int32_t>(
                0,
                static_cast<std::int32_t>(
                    std::floor(
                        camera.tileX()
                        - halfVisibleTilesX
                    )
                ) - 1
            );

        const std::int32_t maximumX =
            std::min<std::int32_t>(
                grid.width() - 1,
                static_cast<std::int32_t>(
                    std::ceil(
                        camera.tileX()
                        + halfVisibleTilesX
                    )
                ) + 1
            );


        const std::int32_t minimumY =
            std::max<std::int32_t>(
                0,
                static_cast<std::int32_t>(
                    std::floor(
                        camera.tileY()
                        - halfVisibleTilesY
                    )
                ) - 1
            );

        const std::int32_t maximumY =
            std::min<std::int32_t>(
                grid.height() - 1,
                static_cast<std::int32_t>(
                    std::ceil(
                        camera.tileY()
                        + halfVisibleTilesY
                    )
                ) + 1
            );


        for (
            std::int32_t y = minimumY;
            y <= maximumY;
            ++y
        )
        {
            for (
                std::int32_t x = minimumX;
                x <= maximumX;
                ++x
            )
            {
                const WorldTilePosition position{
                    x,
                    y
                };

                const WorldTile* tile =
                    grid.tile(position);

                if (!tile)
                {
                    continue;
                }


                const double relativeTileX =
                    static_cast<double>(x)
                    - camera.tileX();

                const double relativeTileY =
                    static_cast<double>(y)
                    - camera.tileY();


                const float screenX =
                    static_cast<float>(
                        viewportWidth * 0.5
                        + relativeTileX * tilePixels
                    );

                const float screenY =
                    static_cast<float>(
                        viewportHeight * 0.5
                        + relativeTileY * tilePixels
                    );


                renderer.fillRectangle(
                    screenX,
                    screenY,
                    static_cast<float>(tilePixels + 0.5),
                    static_cast<float>(tilePixels + 0.5),
                    terrainColor(tile->terrain)
                );
            }
        }
    }
}
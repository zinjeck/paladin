#pragma once

#include "rendering/Renderer.h"
#include "rendering/WorldPixelGrid.h"
#include "world/WorldTilePosition.h"

#include <algorithm>
#include <cmath>

namespace Paladin
{
    // Terrain deliberately stays on the 16-art-pixels-per-tile world lattice.
    // Strategic world objects receive exactly twice that authored detail and
    // pass through this one presentation grid. Cartographic labels and symbols
    // are native-screen annotations and deliberately do not enter that raster.
    inline constexpr int WorldObjectPixelsPerTile = WorldPixelsPerTile * 2;
    static_assert(WorldObjectPixelsPerTile == 32);

    [[nodiscard]]
    inline double worldObjectPixelPitch(double effectiveTilePixels) noexcept
    {
        return std::max(1.0, effectiveTilePixels / WorldObjectPixelsPerTile);
    }

    // Cartographic symbols are screen-sized annotations, not world sprites.
    // Quantize their finished origin to ONE physical pixel. Never divide their
    // 10-22 px dimensions by the zoom-dependent 32-art-pixel world raster.
    [[nodiscard]]
    inline float stableWorldObjectScreenCoordinate(
        float coordinate, double /*effectiveTilePixels*/) noexcept
    {
        return std::isfinite(coordinate) ? std::round(coordinate) : coordinate;
    }

    class WorldObjectPixelScene
    {
    public:
        WorldObjectPixelScene(Renderer& renderer, double effectiveTilePixels,
                              double rotation = 0.0, double scale = 1.0,
                              double offsetX = 0.0, double offsetY = 0.0)
            : renderer_(renderer),
              active_(renderer.beginPixelScene(
                  worldObjectPixelPitch(effectiveTilePixels),
                  true,
                  std::abs(rotation) > 1e-9 || std::abs(scale - 1) > 1e-9 ||
                      offsetX != 0 || offsetY != 0
              )), rotation_(rotation), scale_(scale), offsetX_(offsetX), offsetY_(offsetY)
        {
        }

        ~WorldObjectPixelScene()
        {
            if (active_)
            {
                renderer_.endPixelScene(255, rotation_, scale_, offsetX_, offsetY_);
            }
        }

        WorldObjectPixelScene(const WorldObjectPixelScene&) = delete;
        WorldObjectPixelScene& operator=(const WorldObjectPixelScene&) = delete;

    private:
        Renderer& renderer_;
        bool active_ = false;
        double rotation_, scale_, offsetX_, offsetY_;
    };

    struct WorldPlacementMarker
    {
        WorldTilePosition position;
        RenderColor color{255, 215, 131, 235};
    };
} // namespace Paladin

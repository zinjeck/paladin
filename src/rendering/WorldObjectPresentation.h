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
    // must all pass through this one presentation grid so settlements, markers,
    // armies and roads never drift into different pixel scales.
    inline constexpr int WorldObjectPixelsPerTile = WorldPixelsPerTile * 2;
    static_assert(WorldObjectPixelsPerTile == 32);

    [[nodiscard]]
    inline double worldObjectPixelPitch(double effectiveTilePixels) noexcept
    {
        return std::max(1.0, effectiveTilePixels / WorldObjectPixelsPerTile);
    }

    // Object centers are quantized to the same low-resolution raster that will
    // receive them. The authoritative camera remains continuous; this only
    // prevents a moving camera from changing which object texel a fractional
    // screen coordinate lands on from frame to frame.
    [[nodiscard]]
    inline float stableWorldObjectScreenCoordinate(
        float coordinate,
        double effectiveTilePixels
    ) noexcept
    {
        if (!std::isfinite(coordinate) || !std::isfinite(effectiveTilePixels) ||
            effectiveTilePixels <= 0.0)
        {
            return coordinate;
        }
        const double pitch = worldObjectPixelPitch(effectiveTilePixels);
        return static_cast<float>(std::round(double(coordinate) / pitch) * pitch);
    }

    class WorldObjectPixelScene
    {
    public:
        WorldObjectPixelScene(Renderer& renderer, double effectiveTilePixels)
            : renderer_(renderer),
              active_(renderer.beginPixelScene(
                  worldObjectPixelPitch(effectiveTilePixels),
                  true
              ))
        {
        }

        ~WorldObjectPixelScene()
        {
            if (active_)
            {
                renderer_.endPixelScene(255, 0.0, 1.0);
            }
        }

        WorldObjectPixelScene(const WorldObjectPixelScene&) = delete;
        WorldObjectPixelScene& operator=(const WorldObjectPixelScene&) = delete;

    private:
        Renderer& renderer_;
        bool active_ = false;
    };

    struct WorldPlacementMarker
    {
        WorldTilePosition position;
        RenderColor color{255, 215, 131, 235};
    };
} // namespace Paladin

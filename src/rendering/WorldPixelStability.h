#pragma once

#include "rendering/Camera2D.h"
#include "rendering/GlobeView.h"
#include "rendering/WorldPixelGrid.h"
#include "world/WorldGrid.h"

#include <algorithm>
#include <cmath>

namespace Paladin
{
    // Pixel-art stabilization is a presentation concern only. The authoritative
    // camera remains continuous for input, picking and simulation. At close
    // world scale we quantize only the render camera to the canonical 16-art-
    // pixels-per-tile lattice, so a tiny pan shifts the prepared terrain by a
    // whole art pixel instead of resampling every high-contrast cluster.
    struct WorldPixelStabilityPolicy
    {
        double enterTilePixels = 28.0;
        double exitTilePixels = 24.0;
    };

    [[nodiscard]]
    inline bool worldPixelStabilityActive(
        bool currentlyActive,
        double effectiveTilePixels,
        const WorldPixelStabilityPolicy& policy = {}
    ) noexcept
    {
        if (!std::isfinite(effectiveTilePixels) || effectiveTilePixels <= 0.0)
        {
            return false;
        }

        const double enter = std::max(1.0, policy.enterTilePixels);
        const double exit = std::clamp(policy.exitTilePixels, 1.0, enter);
        return currentlyActive ? effectiveTilePixels >= exit
                               : effectiveTilePixels >= enter;
    }

    [[nodiscard]]
    inline double snapWorldArtCoordinate(double tileCoordinate) noexcept
    {
        if (!std::isfinite(tileCoordinate))
        {
            return tileCoordinate;
        }
        return std::round(tileCoordinate * WorldPixelsPerTile) /
               WorldPixelsPerTile;
    }

    [[nodiscard]]
    inline Camera2D pixelStableWorldCamera(
        const Camera2D& camera,
        const WorldGrid& grid,
        int screenWidth,
        int screenHeight,
        bool globe
    ) noexcept
    {
        Camera2D stable = camera;
        if (grid.width() <= 0 || grid.height() <= 0 || screenWidth <= 0 ||
            screenHeight <= 0)
        {
            return stable;
        }

        const double snappedX = snapWorldArtCoordinate(camera.tileX());
        const double snappedY = std::clamp(
            snapWorldArtCoordinate(camera.tileY()),
            0.0,
            double(grid.height())
        );

        if (!globe)
        {
            stable.setPosition(snappedX, snappedY);
            return stable;
        }

        // Preserve the real camera's roll/orientation. Apply only the minimum
        // world-space correction needed to move the view center from the
        // continuous surface coordinate onto the nearest art-pixel coordinate.
        const auto view = GlobeView::from(
            camera,
            grid,
            screenWidth,
            screenHeight
        );
        const auto actualCenter = WorldSurface::sphere(
            camera.tileX() / grid.width(),
            camera.tileY() / grid.height()
        );
        const auto snappedCenter = WorldSurface::sphere(
            snappedX / grid.width(),
            snappedY / grid.height()
        );
        const auto correction =
            PlanetRotation::between(snappedCenter, actualCenter).normalized();
        stable.setPlanetRotation(
            (view.orientation() * correction).normalized(),
            grid.width(),
            grid.height()
        );
        return stable;
    }
} // namespace Paladin

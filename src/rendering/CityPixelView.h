#pragma once
#include "rendering/Camera2D.h"
#include "rendering/WorldPixelGrid.h"
#include <algorithm>
#include <cmath>

namespace Paladin
{
    // Keep every city layer on one source lattice, then move the completed
    // image in physical pixels. Picking inverts this same final transform.
    struct CityPixelView
    {
        Camera2D source;
        double offsetX = 0, offsetY = 0, scale = 1;
        double tilePixels = 1;
        CityPixelView(
            const Camera2D& camera,
            double pixels,
            int width,
            int height
        )
            : source(camera), tilePixels(pixels)
        {
            if (!std::isfinite(pixels) || pixels < 32 || width <= 0 ||
                height <= 0)
            {
                return;
            }
            source.setPosition(
                std::round(camera.tileX() * WorldPixelsPerTile) /
                    WorldPixelsPerTile,
                std::round(camera.tileY() * WorldPixelsPerTile) /
                    WorldPixelsPerTile
            );
            offsetX = std::round((source.tileX() - camera.tileX()) * pixels);
            offsetY = std::round((source.tileY() - camera.tileY()) * pixels);
            // A guard beyond each edge supplies pixels for residual
            // translation.
            scale = 1 + 2 * (pixels / WorldPixelsPerTile + 2) /
                            std::min(width, height);
        }
        double pickX(double screenX, int width) const
        {
            return source.tileX() +
                   (screenX - width * .5 - offsetX) / tilePixels;
        }
        double pickY(double screenY, int height) const
        {
            return source.tileY() +
                   (screenY - height * .5 - offsetY) / tilePixels;
        }
    };
} // namespace Paladin

#pragma once

#include "rendering/Camera2D.h"
#include "rendering/GlobeView.h"
#include "world/WorldGrid.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace Paladin
{
    // Close world presentation is a first-order chart tangent to the point
    // under the globe camera. The world remains spherical and the authoritative
    // camera remains a quaternion; only the close presentation stops bending
    // every terrain texel through a curved triangle mesh.
    //
    // Tile distances stay square in this chart because Paladin's authored world
    // art is tile based. The chart inherits the globe's screen roll, so Q/E does
    // not stop working merely because the player is close to the ground.
    struct LocalTangentWorldView
    {
        double centerTileX = 0.0;
        double centerTileY = 0.0;
        double centerScreenX = 0.0;
        double centerScreenY = 0.0;
        double tilePixels = 1.0;
        double eastX = 1.0;
        double eastY = 0.0;
        double southX = 0.0;
        double southY = 1.0;
        int worldWidth = 0;
        int worldHeight = 0;
        int viewportWidth = 0;
        int viewportHeight = 0;

        [[nodiscard]]
        static LocalTangentWorldView from(
            const Camera2D& camera,
            const WorldGrid& grid,
            int screenWidth,
            int screenHeight,
            double effectiveTilePixels
        ) noexcept
        {
            LocalTangentWorldView result;
            result.centerTileX = camera.tileX();
            result.centerTileY = camera.tileY();
            result.centerScreenX = double(screenWidth) * 0.5;
            result.centerScreenY = double(screenHeight) * 0.5;
            result.tilePixels =
                std::isfinite(effectiveTilePixels) && effectiveTilePixels > 0.0
                    ? effectiveTilePixels
                    : 1.0;
            result.worldWidth = grid.width();
            result.worldHeight = grid.height();
            result.viewportWidth = screenWidth;
            result.viewportHeight = screenHeight;

            if (grid.width() <= 0 || grid.height() <= 0 || screenWidth <= 0 ||
                screenHeight <= 0)
            {
                return result;
            }

            const auto globe = GlobeView::from(
                camera,
                grid,
                screenWidth,
                screenHeight
            );
            result.setRollRadians(globe.surfaceRollRadians());
            return result;
        }

        void setRollRadians(double angle) noexcept
        {
            if (!std::isfinite(angle))
            {
                angle = 0.0;
            }
            eastX = std::cos(angle);
            eastY = std::sin(angle);
            southX = -eastY;
            southY = eastX;
        }

        [[nodiscard]]
        Camera2D planarCamera() const noexcept
        {
            Camera2D camera(centerTileX, centerTileY);
            camera.setWorldZoom(1.0);
            return camera;
        }

        [[nodiscard]]
        double rollRadians() const noexcept
        {
            return std::atan2(eastY, eastX);
        }

        // Minimum uniform scale required for a rotated rectangle to CONTAIN the
        // viewport, rather than merely having a large enough bounding box. The
        // previous |cos|+|sin| rule is sufficient only for square viewports; on
        // a widescreen window it leaves opposite black corner wedges at close
        // zoom. Extra margin also covers the small rigid camera residual used by
        // the temporal-stability pass and integer/ceil rounding at the raster edge.
        [[nodiscard]]
        double overscanScale(double extraMarginPixels = 0.0) const noexcept
        {
            const double width = std::max(1.0, double(viewportWidth));
            const double height = std::max(1.0, double(viewportHeight));
            const double margin = std::max(0.0, extraMarginPixels);
            const double angle = rollRadians();
            const double c = std::abs(std::cos(angle));
            const double s = std::abs(std::sin(angle));

            const double requiredForX =
                c + (height / width) * s +
                (2.0 * margin / width) * (c + s);
            const double requiredForY =
                c + (width / height) * s +
                (2.0 * margin / height) * (c + s);
            return std::max({1.0, requiredForX, requiredForY});
        }

        [[nodiscard]]
        WorldSurface::Point3 projectTiles(double tileX, double tileY) const noexcept
        {
            double dx = tileX - centerTileX;
            if (worldWidth > 0)
            {
                dx -= std::round(dx / double(worldWidth)) * worldWidth;
            }
            const double dy = tileY - centerTileY;
            return {
                centerScreenX +
                    (dx * eastX + dy * southX) * tilePixels,
                centerScreenY +
                    (dx * eastY + dy * southY) * tilePixels,
                1.0
            };
        }

        // The close terrain is rasterized around a snapped world-art coordinate,
        // but the authoritative camera remains continuous. Instead of making the
        // entire terrain wait and then jump by one enlarged art pixel, translate
        // the finished raster as a rigid image by the remaining camera delta.
        // Snapping only this FINAL offset to one physical output pixel preserves
        // crisp internal texels while camera movement advances in 1px steps.
        [[nodiscard]]
        WorldSurface::Point3 rigidOffsetToCenter(
            double authoritativeTileX,
            double authoritativeTileY
        ) const noexcept
        {
            const auto projected =
                projectTiles(authoritativeTileX, authoritativeTileY);
            if (!std::isfinite(projected.x) || !std::isfinite(projected.y))
            {
                return {0.0, 0.0, 0.0};
            }
            return {
                std::round(centerScreenX - projected.x),
                std::round(centerScreenY - projected.y),
                0.0
            };
        }

        [[nodiscard]]
        LocalTangentWorldView translated(double x, double y) const noexcept
        {
            LocalTangentWorldView result = *this;
            if (std::isfinite(x))
            {
                result.centerScreenX += x;
            }
            if (std::isfinite(y))
            {
                result.centerScreenY += y;
            }
            return result;
        }

        [[nodiscard]]
        std::optional<WorldSurface::UV> pick(
            double screenX,
            double screenY
        ) const noexcept
        {
            if (worldWidth <= 0 || worldHeight <= 0 || tilePixels <= 0.0 ||
                !std::isfinite(screenX) || !std::isfinite(screenY))
            {
                return std::nullopt;
            }

            const double rx = (screenX - centerScreenX) / tilePixels;
            const double ry = (screenY - centerScreenY) / tilePixels;
            double worldX = centerTileX + rx * eastX + ry * eastY;
            const double worldY =
                centerTileY + rx * southX + ry * southY;
            if (worldY < 0.0 || worldY >= double(worldHeight))
            {
                return std::nullopt;
            }
            worldX -= std::floor(worldX / double(worldWidth)) * worldWidth;
            return WorldSurface::UV{
                worldX / double(worldWidth),
                worldY / double(worldHeight)
            };
        }
    };
} // namespace Paladin

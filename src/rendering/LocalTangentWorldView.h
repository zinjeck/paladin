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

            if (grid.width() <= 0 || grid.height() <= 0 || screenWidth <= 0 ||
                screenHeight <= 0)
            {
                return result;
            }

            constexpr double Pi = 3.14159265358979323846;
            const double u = camera.tileX() / double(grid.width());
            const double v = camera.tileY() / double(grid.height());
            const double longitude = (u - 0.5) * 2.0 * Pi;
            const WorldSurface::Point3 east{
                std::cos(longitude),
                0.0,
                -std::sin(longitude)
            };

            const auto globe = GlobeView::from(
                camera,
                grid,
                screenWidth,
                screenHeight
            );
            const auto viewEast = globe.orientation().apply(east);
            double screenEastX = viewEast.x;
            double screenEastY = -viewEast.y;
            const double length = std::hypot(screenEastX, screenEastY);
            if (std::isfinite(length) && length > 1e-9)
            {
                screenEastX /= length;
                screenEastY /= length;
                result.eastX = screenEastX;
                result.eastY = screenEastY;
                result.southX = -screenEastY;
                result.southY = screenEastX;
            }
            return result;
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

        [[nodiscard]]
        double overscanScale() const noexcept
        {
            const double angle = rollRadians();
            return std::max(
                1.0,
                std::abs(std::cos(angle)) + std::abs(std::sin(angle))
            );
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

#pragma once

#include "rendering/GlobeView.h"
#include "world/WorldTilePosition.h"

#include <cmath>

namespace Paladin
{
    // Screen-relative globe controls are independent of flat-map pan/clamping.
    // Location/orientation helpers live here so settlement, army, event, and
    // future world entities can share the same navigation behavior.
    struct GlobeCameraNavigation
    {
        static void pan(
            Camera2D& camera,
            const WorldGrid& grid,
            int width,
            int height,
            double dx,
            double dy,
            double screenPixels
        )
        {
            const auto view = GlobeView::from(camera, grid, width, height);
            const double length = std::hypot(dx, dy);
            if (length <= 0 || !std::isfinite(length) ||
                !std::isfinite(screenPixels) || !std::isfinite(view.radius) ||
                view.radius <= 1e-9)
            {
                return;
            }

            // Panning should move the point under the camera, not quietly add a
            // second roll channel. The old raw quaternion step parallel-
            // transported the local tangent frame, changing its screen angle on
            // every WASD/edge-scroll frame. At close zoom that forced the rigid
            // nearest-neighbour tangent raster to be re-rotated every frame and
            // produced the visible terrain shimmer. Preserve the exact current
            // surface roll; Q/E remains the sole intentional roll control.
            const double surfaceRoll = view.surfaceRollRadians();
            const auto step = PlanetRotation::axis(
                -dy,
                -dx,
                0,
                screenPixels * length / view.radius
            );
            const auto tentative =
                (step * view.orientation()).normalized();
            const auto center = WorldSurface::coordinates(
                tentative.inverse().apply({0, 0, 1})
            );
            camera.setPlanetRotation(
                GlobeView::orientationAt(center, surfaceRoll),
                grid.width(),
                grid.height()
            );
        }

        static void roll(
            Camera2D& camera,
            const WorldGrid& grid,
            int width,
            int height,
            double radians
        )
        {
            if (radians == 0 || !std::isfinite(radians))
            {
                return;
            }
            const auto view = GlobeView::from(camera, grid, width, height);
            camera.setPlanetRotation(
                PlanetRotation::axis(0, 0, 1, radians) * view.orientation(),
                grid.width(),
                grid.height()
            );
        }

        [[nodiscard]]
        static PlanetRotation northUpOrientationAt(
            const WorldGrid& grid,
            WorldTilePosition position
        )
        {
            if (!grid.isValidPosition(position))
            {
                return {};
            }

            return GlobeView::orientationAt(
                {(position.x + .5) / grid.width(),
                 (position.y + .5) / grid.height()},
                0.0
            );
        }

        static bool focusNorthUp(
            Camera2D& camera,
            const WorldGrid& grid,
            WorldTilePosition position
        )
        {
            if (!grid.isValidPosition(position))
            {
                return false;
            }
            camera.setPlanetRotation(
                northUpOrientationAt(grid, position),
                grid.width(),
                grid.height()
            );
            return true;
        }

        static void drag(
            Camera2D& camera,
            const WorldGrid& grid,
            int width,
            int height,
            double oldX,
            double oldY,
            double newX,
            double newY
        )
        {
            const auto view = GlobeView::from(camera, grid, width, height);
            if (!std::isfinite(view.radius) || view.radius <= 1e-9)
            {
                return;
            }
            const auto ball = [&](double x, double y)
            {
                x = (x - view.cx) / view.radius;
                y = -(y - view.cy) / view.radius;
                const double r2 = x * x + y * y;
                if (r2 > 1)
                {
                    const double n = std::sqrt(r2);
                    return WorldSurface::Point3{x / n, y / n, 0};
                }
                return WorldSurface::Point3{x, y, std::sqrt(1 - r2)};
            };

            const double surfaceRoll = view.surfaceRollRadians();
            const auto tentative =
                (PlanetRotation::between(ball(oldX, oldY), ball(newX, newY)) *
                 view.orientation())
                    .normalized();
            const auto center = WorldSurface::coordinates(
                tentative.inverse().apply({0, 0, 1})
            );
            camera.setPlanetRotation(
                GlobeView::orientationAt(center, surfaceRoll),
                grid.width(),
                grid.height()
            );
        }
    };
} // namespace Paladin

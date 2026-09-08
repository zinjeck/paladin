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
            const auto step = PlanetRotation::axis(
                -dy,
                -dx,
                0,
                screenPixels * length / view.radius
            );
            camera.setPlanetRotation(
                step * view.orientation(),
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

            constexpr double pi = 3.14159265358979323846;
            const double u = (position.x + .5) / grid.width();
            const double v = (position.y + .5) / grid.height();
            const double longitude = (u - .5) * 2 * pi;
            const double latitude = (.5 - v) * pi;
            const auto normal = WorldSurface::sphere(u, v);
            const WorldSurface::Point3 north{
                -std::sin(latitude) * std::sin(longitude),
                std::cos(latitude),
                -std::sin(latitude) * std::cos(longitude)
            };
            const auto centered =
                PlanetRotation::between(normal, {0, 0, 1}).normalized();
            const auto viewUp = centered.apply(north);
            const double rollAngle = std::atan2(viewUp.x, viewUp.y);
            return (PlanetRotation::axis(0, 0, 1, rollAngle) * centered)
                .normalized();
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
            camera.setPlanetRotation(
                PlanetRotation::between(ball(oldX, oldY), ball(newX, newY)) *
                    view.orientation(),
                grid.width(),
                grid.height()
            );
        }
    };
} // namespace Paladin

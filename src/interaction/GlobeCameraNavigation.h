#pragma once
#include "rendering/GlobeView.h"
namespace Paladin
{
    // Screen-relative globe controls are independent of flat-map pan/clamping.
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
            if (length <= 0)
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

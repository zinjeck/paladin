#pragma once
#include "rendering/Camera2D.h"
#include "rendering/WorldSurface.h"
#include "world/WorldGrid.h"

namespace Paladin
{
    struct GlobeView
    {
        double cx, cy, radius, yaw, pitch;
        static GlobeView from(
            const Camera2D& camera,
            const WorldGrid& grid,
            int width,
            int height
        )
        {
            constexpr double pi = 3.141592653589793;
            return {
                width * .5,
                height * .5,
                std::min(width, height) * .40 * camera.zoom(),
                -(camera.tileX() / grid.width() - .5) * 2 * pi,
                (.5 - camera.tileY() / grid.height()) * pi
            };
        }
        std::optional<WorldSurface::UV> pick(double x, double y) const
        {
            return WorldSurface::pick(
                (x - cx) / radius,
                -(y - cy) / radius,
                yaw,
                pitch
            );
        }
        WorldSurface::Point3 project(double u, double v) const
        {
            auto p =
                WorldSurface::orient(WorldSurface::sphere(u, v), yaw, pitch);
            return {cx + p.x * radius, cy - p.y * radius, p.z};
        }
    };
} // namespace Paladin

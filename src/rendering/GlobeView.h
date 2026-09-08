#pragma once
#include "rendering/Camera2D.h"
#include "rendering/WorldSurface.h"
#include "world/PlanetAstronomy.h"
#include "world/WorldGrid.h"

namespace Paladin
{
    struct GlobeView
    {
        double cx, cy, radius, yaw, pitch;
        std::optional<PlanetRotation> rotation;
        PlanetRotation orientation() const
        {
            return rotation ? *rotation
                            : (PlanetRotation::axis(
                                   0,
                                   0,
                                   1,
                                   PlanetAstronomy::AxialTilt
                               ) *
                               PlanetRotation::axis(1, 0, 0, pitch) *
                               PlanetRotation::axis(0, 1, 0, yaw));
        }
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
                (.5 - camera.tileY() / grid.height()) * pi,
                camera.planetRotation()
                    ? camera.planetRotation()
                    : std::optional<PlanetRotation>{
                          PlanetRotation::axis(
                              0,
                              0,
                              1,
                              PlanetAstronomy::AxialTilt
                          ) *
                          PlanetRotation::axis(
                              1,
                              0,
                              0,
                              (.5 - camera.tileY() / grid.height()) * pi
                          ) *
                          PlanetRotation::axis(
                              0,
                              1,
                              0,
                              -(camera.tileX() / grid.width() - .5) * 2 * pi
                          )
                      }
            };
        }
        std::optional<WorldSurface::UV> pick(double x, double y) const
        {
            const double xx = (x - cx) / radius, yy = -(y - cy) / radius;
            const double r2 = xx * xx + yy * yy;
            if (!std::isfinite(r2) || r2 > 1)
            {
                return std::nullopt;
            }
            return WorldSurface::coordinates(
                orientation().inverse().apply({xx, yy, std::sqrt(1 - r2)})
            );
        }
        WorldSurface::Point3 orient(WorldSurface::Point3 point) const
        {
            return orientation().apply(point);
        }
        WorldSurface::Point3 project(double u, double v) const
        {
            auto p = orient(WorldSurface::sphere(u, v));
            return {cx + p.x * radius, cy - p.y * radius, p.z};
        }
    };
} // namespace Paladin

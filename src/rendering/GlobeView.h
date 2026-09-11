#pragma once
#include "rendering/Camera2D.h"
#include "rendering/WorldSurface.h"
#include "world/PlanetAstronomy.h"
#include "world/WorldGrid.h"

#include <algorithm>
#include <cmath>

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

        // Construct an orientation that centers an exact world-surface point
        // while giving its local east axis a requested screen-space roll. This
        // is the common geometric primitive used by close rendering and camera
        // navigation, so translating the globe does not have to introduce an
        // accidental extra roll.
        static PlanetRotation orientationAt(
            WorldSurface::UV uv,
            double surfaceRollRadians = 0.0
        ) noexcept
        {
            constexpr double pi = 3.14159265358979323846;
            const double longitude = (uv.u - .5) * 2.0 * pi;
            const double latitude = (.5 - uv.v) * pi;
            const auto normal = WorldSurface::sphere(uv.u, uv.v);
            const WorldSurface::Point3 north{
                -std::sin(latitude) * std::sin(longitude),
                std::cos(latitude),
                -std::sin(latitude) * std::cos(longitude)
            };
            const auto centered =
                PlanetRotation::between(normal, {0, 0, 1}).normalized();
            const auto viewUp = centered.apply(north);
            const double northUpCorrection = std::atan2(viewUp.x, viewUp.y);
            const auto northUp =
                (PlanetRotation::axis(0, 0, 1, northUpCorrection) * centered)
                    .normalized();

            // A positive 3D Z rotation turns screen-space east clockwise because
            // screen Y points downward. Negate the requested screen roll so the
            // public angle follows ordinary 2D screen coordinates.
            return (PlanetRotation::axis(0, 0, 1, -surfaceRollRadians) * northUp)
                .normalized();
        }

        // Screen-space angle of the tangent east axis at the point currently
        // centered by this view. It includes the established axial tilt and any
        // explicit Q/E roll, but is independent of radius/zoom.
        [[nodiscard]]
        double surfaceRollRadians() const noexcept
        {
            const auto q = orientation();
            const auto center = q.inverse().apply({0, 0, 1});
            const auto uv = WorldSurface::coordinates(center);
            constexpr double pi = 3.14159265358979323846;
            const double longitude = (uv.u - .5) * 2.0 * pi;
            const WorldSurface::Point3 east{
                std::cos(longitude),
                0.0,
                -std::sin(longitude)
            };
            const auto viewEast = q.apply(east);
            const double x = viewEast.x;
            const double y = -viewEast.y;
            const double length = std::hypot(x, y);
            if (!std::isfinite(length) || length <= 1e-12)
            {
                return 0.0;
            }
            return std::atan2(y / length, x / length);
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

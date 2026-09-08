#pragma once
#include <algorithm>
#include <cmath>
#include <optional>
namespace Paladin
{
    // Presentation adapter: map topology and simulation remain tile based.
    // Future curved meshes consume the same UV/elevation samples as today's
    // flat atlas.
    struct WorldSurface
    {
        struct Point3
        {
            double x, y, z;
        };
        struct UV
        {
            double u, v;
        };
        // Orientation for a planet camera, in radians. Rotation is independent
        // of atlas storage: users turn a sphere, never pan a projected
        // rectangle.
        static Point3 orient(Point3 p, double yaw, double pitch)
        {
            const double x = p.x * std::cos(yaw) + p.z * std::sin(yaw);
            const double z = p.z * std::cos(yaw) - p.x * std::sin(yaw);
            return {
                x,
                p.y * std::cos(pitch) - z * std::sin(pitch),
                p.y * std::sin(pitch) + z * std::cos(pitch)
            };
        }
        static UV coordinates(Point3 p)
        {
            constexpr double pi = 3.14159265358979323846;
            const double length = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
            if (length == 0)
            {
                return {.5, .5};
            }
            double u = std::atan2(p.x, p.z) / (2 * pi) + .5;
            u -= std::floor(u);
            return {u, .5 - std::asin(std::clamp(p.y / length, -1., 1.)) / pi};
        }
        // Orthographic planet picking. Points outside the circular silhouette
        // are space, never clamped to a land tile. Undo pitch before yaw.
        static std::optional<UV> pick(
            double x,
            double y,
            double yaw,
            double pitch
        )
        {
            const double radius2 = x * x + y * y;
            if (!std::isfinite(radius2) || radius2 > 1)
            {
                return std::nullopt;
            }
            auto p = orient({x, y, std::sqrt(1 - radius2)}, 0, -pitch);
            p = orient(p, -yaw, 0);
            return coordinates(p);
        }
        static Point3 sphere(
            double u,
            double v,
            double radius = 1,
            double relief = 0
        )
        {
            constexpr double pi = 3.14159265358979323846;
            const double longitude = (u - .5) * 2 * pi,
                         latitude = (.5 - v) * pi;
            const double r = radius + relief;
            return {
                r * std::cos(latitude) * std::sin(longitude),
                r * std::sin(latitude),
                r * std::cos(latitude) * std::cos(longitude)
            };
        }
        // A cone unwrap can be adopted independently of world generation.
        static Point3 cone(double u, double v, double opening = .65)
        {
            const double radius = 1 - v * .8,
                         angle = (u - .5) * 6.283185307179586 * opening;
            return {radius * std::sin(angle), v, radius * std::cos(angle)};
        }
    };
} // namespace Paladin

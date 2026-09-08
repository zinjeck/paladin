#pragma once
#include <algorithm>
#include <cmath>
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

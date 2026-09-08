#pragma once
#include "rendering/WorldSurface.h"
namespace Paladin
{
    // Planet-to-view rotation. No latitude clamp or Euler-angle pole
    // singularity.
    struct PlanetRotation
    {
        double w = 1, x = 0, y = 0, z = 0;
        PlanetRotation normalized() const
        {
            const double n = std::sqrt(w * w + x * x + y * y + z * z);
            return n > 1e-12 ? PlanetRotation{w / n, x / n, y / n, z / n}
                             : PlanetRotation{};
        }
        PlanetRotation inverse() const
        {
            return {w, -x, -y, -z};
        }
        PlanetRotation operator*(const PlanetRotation& b) const
        {
            return {
                w * b.w - x * b.x - y * b.y - z * b.z,
                w * b.x + x * b.w + y * b.z - z * b.y,
                w * b.y - x * b.z + y * b.w + z * b.x,
                w * b.z + x * b.y - y * b.x + z * b.w
            };
        }
        WorldSurface::Point3 apply(WorldSurface::Point3 p) const
        {
            const auto r =
                (*this) * PlanetRotation{0, p.x, p.y, p.z} * inverse();
            return {r.x, r.y, r.z};
        }
        static PlanetRotation axis(double x, double y, double z, double angle)
        {
            const double length = std::sqrt(x * x + y * y + z * z);
            if (length < 1e-12)
            {
                return {};
            }
            const double s = std::sin(angle * .5) / length;
            return {std::cos(angle * .5), x * s, y * s, z * s};
        }
        static PlanetRotation between(
            WorldSurface::Point3 a,
            WorldSurface::Point3 b
        )
        {
            const double dot =
                std::clamp(a.x * b.x + a.y * b.y + a.z * b.z, -1., 1.);
            if (dot < -.999999)
            {
                return axis(
                    std::abs(a.x) < .9 ? 0 : -a.y,
                    std::abs(a.x) < .9 ? a.z : a.x,
                    std::abs(a.x) < .9 ? -a.y : 0,
                    3.141592653589793
                );
            }
            return PlanetRotation{
                1 + dot,
                a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x
            }
                .normalized();
        }
    };
} // namespace Paladin

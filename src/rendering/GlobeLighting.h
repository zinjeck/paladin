#pragma once
#include "rendering/Renderer.h"
#include "rendering/WorldSurface.h"

namespace Paladin
{
    // Equinox sun: one revolution per authoritative game day. Camera rotation
    // never changes which longitude is at noon. No climate simulation coupling.
    inline double globeSunDot(double u, double v, double secondsIntoDay)
    {
        const auto normal = WorldSurface::sphere(u, v);
        const auto sun =
            WorldSurface::sphere(1.0 - secondsIntoDay / 86400., .5);
        return normal.x * sun.x + normal.y * sun.y + normal.z * sun.z;
    }
    inline RenderColor globeLight(
        double u,
        double v,
        double seconds,
        double depth
    )
    {
        double daylight =
            std::clamp((globeSunDot(u, v, seconds) + .09) / .18, 0., 1.);
        daylight = daylight * daylight * (3 - 2 * daylight);
        const double limb = .90 + .10 * std::sqrt(std::max(0., depth));
        return {
            std::uint8_t(255 * limb * (.48 + .52 * daylight)),
            std::uint8_t(255 * limb * (.53 + .47 * daylight)),
            std::uint8_t(255 * limb * (.63 + .37 * daylight)),
            255
        };
    }
} // namespace Paladin

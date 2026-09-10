#pragma once
#include "rendering/Renderer.h"
#include "rendering/WorldSurface.h"
#include "world/PlanetAstronomy.h"

namespace Paladin
{
    // The world clock drives one authoritative solar direction. Camera rotation
    // never changes which longitude is at noon. No climate simulation coupling.
    inline double globeSunDot(double u, double v, double secondsIntoDay)
    {
        return PlanetAstronomy::sunIncidence(u, v, secondsIntoDay);
    }
    inline double solarIllumination(double incidence)
    {
        double daylight = std::clamp((incidence + .09) / .18, 0., 1.);
        daylight = daylight * daylight * (3 - 2 * daylight);
        return daylight * (.40 + .60 * std::sqrt(std::max(0., incidence)));
    }
    inline double oceanSunGlint(
        double u,
        double v,
        double seconds,
        double viewU,
        double viewV
    )
    {
        const auto n = WorldSurface::sphere(u, v);
        const auto solar = PlanetAstronomy::sunDirection(seconds);
        const auto eye = WorldSurface::sphere(viewU, viewV);
        const WorldSurface::Point3 sun{solar.x, solar.y, solar.z};
        const double x = sun.x + eye.x, y = sun.y + eye.y, z = sun.z + eye.z;
        const double length = std::sqrt(x * x + y * y + z * z);
        if (length < 1e-6)
        {
            return 0;
        }
        const double incidence = n.x * sun.x + n.y * sun.y + n.z * sun.z;
        if (incidence <= 0)
        {
            return 0;
        }
        const double spec =
            std::max(0., (n.x * x + n.y * y + n.z * z) / length);
        if (spec < .8)
        {
            return 0; // Below one display alpha step, avoids unnecessary
                      // powers.
        }
        return std::sqrt(incidence) *
               (.09 * std::pow(spec, 18) + .42 * std::pow(spec, 110) +
                .18 * std::pow(spec, 320));
    }
    inline RenderColor globeLight(
        double u,
        double v,
        double seconds,
        double depth
    )
    {
        const double daylight = solarIllumination(globeSunDot(u, v, seconds));
        const double limb = .92 + .08 * std::sqrt(std::max(0., depth));
        return {
            std::uint8_t(255 * limb * (.48 + .52 * daylight)),
            std::uint8_t(255 * limb * (.53 + .47 * daylight)),
            std::uint8_t(255 * limb * (.63 + .37 * daylight)),
            255
        };
    }
} // namespace Paladin

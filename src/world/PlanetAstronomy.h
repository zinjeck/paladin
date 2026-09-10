#pragma once
#include <algorithm>
#include <cmath>
namespace Paladin
{
    // Planet-fixed geography and solar time. No seasonal simulation is active.
    struct PlanetAstronomy
    {
        struct SolarDirection
        {
            double x, y, z;
        };

        static constexpr double Pi = 3.14159265358979323846;
        static constexpr double AxialTiltDegrees = 23.5;
        static constexpr double AxialTilt = AxialTiltDegrees * Pi / 180.;
        static constexpr double CurrentOrbitalPhase = 0.; // fixed equinox
        static double latitude(double v)
        {
            return (.5 - v) * Pi;
        }
        static double solarOffsetMinutes(double u)
        {
            return (u - .5) * 1440.;
        }
        static double localMinute(double globalMinute, double u)
        {
            double minute =
                std::fmod(globalMinute + solarOffsetMinutes(u), 1440.);
            return minute < 0 ? minute + 1440. : minute;
        }
        // Future orbital phase: 0=equinox, .25=northern summer solstice.
        // This only describes geometry; it does not advance seasons or climate.
        static double declination(double orbitalPhase)
        {
            return std::asin(
                std::sin(AxialTilt) * std::sin(2 * Pi * orbitalPhase)
            );
        }
        static double solarLongitude(double seconds)
        {
            return Pi - seconds * 2 * Pi / 86400.;
        }
        // Unit vector from the planet toward the sun in planet-fixed space.
        // Rendering and surface illumination share this exact source so the
        // visible star always explains the globe's daylight direction.
        static SolarDirection sunDirection(
            double seconds,
            double orbitalPhase = CurrentOrbitalPhase
        )
        {
            const double dec = declination(orbitalPhase);
            const double longitude = solarLongitude(seconds);
            const double horizontal = std::cos(dec);
            return {
                horizontal * std::sin(longitude),
                std::sin(dec),
                horizontal * std::cos(longitude)
            };
        }
        static double sunIncidence(
            double u,
            double v,
            double seconds,
            double orbitalPhase = CurrentOrbitalPhase
        )
        {
            const double lat = latitude(v);
            const double longitude = (u - .5) * 2 * Pi;
            const auto sun = sunDirection(seconds, orbitalPhase);
            return std::cos(lat) * std::sin(longitude) * sun.x +
                   std::sin(lat) * sun.y +
                   std::cos(lat) * std::cos(longitude) * sun.z;
        }
    };
} // namespace Paladin

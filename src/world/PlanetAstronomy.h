#pragma once
#include <algorithm>
#include <cmath>
namespace Paladin
{
    // Planet-fixed geography and solar time. No seasonal simulation is active.
    struct PlanetAstronomy
    {
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
        static double sunIncidence(
            double u,
            double v,
            double seconds,
            double orbitalPhase = CurrentOrbitalPhase
        )
        {
            const double lat = latitude(v), dec = declination(orbitalPhase);
            const double hourAngle =
                (localMinute(seconds / 60., u) - 720.) * Pi / 720.;
            return std::sin(lat) * std::sin(dec) +
                   std::cos(lat) * std::cos(dec) * std::cos(hourAngle);
        }
    };
} // namespace Paladin

#pragma once

#include "TestFramework.h"
#include "rendering/CelestialSun.h"
#include "rendering/PlanetRotation.h"

#include <cmath>

namespace Paladin::Test
{
    inline void runCelestialSunChecks()
    {
        constexpr int width = 1920;
        constexpr int height = 1080;
        constexpr double pi = 3.14159265358979323846;

        GlobeView view{
            width * .5,
            height * .5,
            height * .40,
            0,
            0,
            PlanetRotation{}
        };

        // At local noon the sun is on the camera side of the planet. It must
        // not be composited into the background behind the globe.
        PALADIN_CHECK(!projectCelestialSun(view, width, height, 12 * 3600.));

        // At midnight the star is directly behind the globe. Projection is
        // centered, and the draw pass subsequently hides it behind the planet.
        const auto midnight = projectCelestialSun(view, width, height, 0.);
        PALADIN_CHECK(midnight);
        PALADIN_CHECK(std::abs(midnight->x - view.cx) < 1e-4);
        PALADIN_CHECK(std::abs(midnight->y - view.cy) < 1e-4);
        PALADIN_CHECK(
            std::hypot(midnight->x - view.cx, midnight->y - view.cy) <
            view.radius
        );

        // Crescents are the important case: the visible star must appear on
        // the same side of the screen as the solar direction that lights the
        // limb, not mirrored across the globe.
        const auto earlyMorning =
            projectCelestialSun(view, width, height, 3 * 3600.);
        const auto lateEvening =
            projectCelestialSun(view, width, height, 21 * 3600.);
        PALADIN_CHECK(earlyMorning && lateEvening);
        PALADIN_CHECK(earlyMorning->x > view.cx);
        PALADIN_CHECK(lateEvening->x < view.cx);

        const auto morningDirection = celestialSunViewDirection(view, 3 * 3600.);
        const auto eveningDirection = celestialSunViewDirection(view, 21 * 3600.);
        PALADIN_CHECK(morningDirection.z < 0 && eveningDirection.z < 0);
        PALADIN_CHECK(
            (earlyMorning->x - view.cx) * morningDirection.x > 0
        );
        PALADIN_CHECK((lateEvening->x - view.cx) * eveningDirection.x > 0);

        // Camera/globe rotation must naturally move the sun into and out of
        // view. This guards against another permanently-invisible regression
        // while preserving the design rule that it is never screen-clamped.
        int visible = 0;
        int absent = 0;
        for (int i = 0; i < 32; ++i)
        {
            view.rotation = PlanetRotation::axis(0, 1, 0, 2 * pi * i / 32.);
            if (projectCelestialSun(view, width, height, 12 * 3600.))
            {
                ++visible;
            }
            else
            {
                ++absent;
            }
        }
        PALADIN_CHECK(visible > 0);
        PALADIN_CHECK(absent > 0);
    }
} // namespace Paladin::Test

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
        // not be composited into the distant background behind the globe.
        PALADIN_CHECK(!projectCelestialSun(view, width, height, 12 * 3600.));

        // At midnight the star is directly behind the globe. Its celestial
        // projection is centered and the renderer's analytic globe mask hides
        // the optical texture completely.
        const auto midnight = projectCelestialSun(view, width, height, 0.);
        PALADIN_CHECK(midnight);
        PALADIN_CHECK(std::abs(midnight->x - view.cx) < 1e-4);
        PALADIN_CHECK(std::abs(midnight->y - view.cy) < 1e-4);
        PALADIN_CHECK(
            std::hypot(midnight->x - view.cx, midnight->y - view.cy) <
            view.radius
        );

        const auto checkCrescentAlignment = [&](double seconds)
        {
            const auto projected =
                projectCelestialSun(view, width, height, seconds);
            PALADIN_CHECK(projected);
            const auto direction = celestialSunViewDirection(view, seconds);
            PALADIN_CHECK(direction.z < 0);

            // Screen-space direction to the rendered star must be parallel to
            // the X/Y part of the exact vector used by sunIncidence(). A sign
            // flip here is the regression that made the original feature look
            // disconnected from the lit hemisphere.
            const double sx = projected->x - view.cx;
            const double sy = view.cy - projected->y;
            const double screenLength = std::hypot(sx, sy);
            const double horizontal = std::hypot(direction.x, direction.y);
            PALADIN_CHECK(screenLength > 0 && horizontal > 0);
            PALADIN_CHECK(
                (sx * direction.x + sy * direction.y) /
                        (screenLength * horizontal) >
                    .999999
            );

            // The corresponding visible limb must actually be sunlit while
            // the center of the visible hemisphere is dark. That proves the
            // rendered star explains the crescent instead of merely sharing a
            // clock value with it.
            const WorldSurface::Point3 limbView{
                direction.x / horizontal,
                direction.y / horizontal,
                0
            };
            const auto limbPlanet = view.orientation().inverse().apply(limbView);
            const auto limbUv = WorldSurface::coordinates(limbPlanet);
            const double limbLight = PlanetAstronomy::sunIncidence(
                limbUv.u,
                limbUv.v,
                seconds
            );
            const auto centerPlanet =
                view.orientation().inverse().apply({0, 0, 1});
            const auto centerUv = WorldSurface::coordinates(centerPlanet);
            const double centerLight = PlanetAstronomy::sunIncidence(
                centerUv.u,
                centerUv.v,
                seconds
            );
            PALADIN_CHECK(limbLight > .65);
            PALADIN_CHECK(centerLight < 0);
        };

        checkCrescentAlignment(3 * 3600.);
        checkCrescentAlignment(21 * 3600.);

        // Near the far-side horizon the real perspective projection may put the
        // source well outside the viewport. It must disappear rather than be
        // clamped to an edge for composition.
        view.rotation = PlanetRotation::axis(0, 1, 0, 1.45);
        const auto grazing = celestialSunViewDirection(view, 0.);
        PALADIN_CHECK(grazing.z < -.035);
        PALADIN_CHECK(!projectCelestialSun(view, width, height, 0.));

        // Camera/globe rotation naturally moves the star into and out of view.
        // This catches both permanently-hidden and always-on-screen regressions.
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

        // The procedural optical source must read like a photographic light:
        // white-hot center, smooth falloff, and narrow diffraction spikes rather
        // than flat pixel-art circles/lines.
        const auto center = celestialSunSample(0, 0);
        PALADIN_CHECK(center.alpha == 255);
        PALADIN_CHECK(center.red == 255 && center.green >= 254 && center.blue >= 250);
        constexpr float sampleAngle = .40F;
        const auto nearHalo = celestialSunSample(
            .12F * std::cos(sampleAngle),
            .12F * std::sin(sampleAngle)
        );
        const auto middleHalo = celestialSunSample(
            .40F * std::cos(sampleAngle),
            .40F * std::sin(sampleAngle)
        );
        const auto farHalo = celestialSunSample(
            .85F * std::cos(sampleAngle),
            .85F * std::sin(sampleAngle)
        );
        PALADIN_CHECK(center.alpha > nearHalo.alpha);
        PALADIN_CHECK(nearHalo.alpha > middleHalo.alpha);
        PALADIN_CHECK(middleHalo.alpha > farHalo.alpha);

        const auto diffractionRay = celestialSunSample(
            .55F * std::cos(.018F),
            .55F * std::sin(.018F)
        );
        const auto betweenRays = celestialSunSample(
            .55F * std::cos(.35F),
            .55F * std::sin(.35F)
        );
        PALADIN_CHECK(diffractionRay.alpha > betweenRays.alpha * 3);
    }
} // namespace Paladin::Test

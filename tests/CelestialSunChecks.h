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

        PALADIN_CHECK(!projectCelestialSun(view, width, height, 12 * 3600.));

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

        view.rotation = PlanetRotation::axis(0, 1, 0, 1.45);
        const auto grazing = celestialSunViewDirection(view, 0.);
        PALADIN_CHECK(grazing.z < -.035);
        PALADIN_CHECK(!projectCelestialSun(view, width, height, 0.));

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

        // The core layer is deliberately translucent by itself. Additive
        // overlap with the corona produces the white-hot center, preventing an
        // opaque circular sprite edge from ever existing in source pixels.
        const auto coronaCenter = celestialSunCoronaSample(0, 0);
        const auto coreCenter = celestialSunCoreSample(0, 0);
        const auto coreNear = celestialSunCoreSample(.04F, 0);
        const auto coreMid = celestialSunCoreSample(.10F, 0);
        const auto coreOuter = celestialSunCoreSample(.40F, 0);
        PALADIN_CHECK(coreCenter.alpha < 200);
        PALADIN_CHECK(coreCenter.alpha > 120);
        PALADIN_CHECK(coronaCenter.alpha + coreCenter.alpha > 300);
        PALADIN_CHECK(
            coreCenter.red == 255 && coreCenter.green >= 254 &&
            coreCenter.blue >= 250
        );
        PALADIN_CHECK(coreCenter.alpha > coreNear.alpha);
        PALADIN_CHECK(coreNear.alpha > coreMid.alpha);
        PALADIN_CHECK(coreMid.alpha > coreOuter.alpha);
        PALADIN_CHECK(coreCenter.alpha - coreNear.alpha < 24);

        // A primary ray is visible but remains a broad optical cone, not a line
        // several times brighter than everything around it.
        const float radius = .40F;
        const auto glareLobe = celestialSunCoronaSample(
            radius * std::cos(.10F),
            radius * std::sin(.10F)
        );
        const auto betweenLobes = celestialSunCoronaSample(
            radius * std::cos(.35F),
            radius * std::sin(.35F)
        );
        PALADIN_CHECK(glareLobe.alpha > betweenLobes.alpha * 1.3F);
        PALADIN_CHECK(glareLobe.alpha < betweenLobes.alpha * 2.5F);

        const auto diffuseMiddle = celestialSunCoronaSample(
            .60F * std::cos(.35F),
            .60F * std::sin(.35F)
        );
        const auto diffuseOuter = celestialSunCoronaSample(
            .85F * std::cos(.35F),
            .85F * std::sin(.35F)
        );
        PALADIN_CHECK(diffuseMiddle.alpha > 10);
        PALADIN_CHECK(diffuseOuter.alpha > 0);
        PALADIN_CHECK(diffuseMiddle.alpha > diffuseOuter.alpha);

        const auto ghost = celestialSunLensGhostSample(.53F, 0);
        PALADIN_CHECK(ghost.alpha >= 10 && ghost.alpha <= 30);
        PALADIN_CHECK(ghost.red < ghost.blue);
    }
} // namespace Paladin::Test

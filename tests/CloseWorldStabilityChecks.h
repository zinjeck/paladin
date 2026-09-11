#pragma once

#include "TestFramework.h"
#include "interaction/GlobeCameraNavigation.h"
#include "rendering/GlobeView.h"
#include "rendering/LocalTangentWorldView.h"
#include "rendering/WorldPixelStability.h"

#include <array>
#include <cmath>

namespace Paladin::Test
{
    inline double closeWorldAngleDelta(double a, double b) noexcept
    {
        return std::atan2(std::sin(a - b), std::cos(a - b));
    }

    inline void runCloseWorldStabilityChecks()
    {
        WorldGrid grid(128, 64);

        // WASD/edge-scroll movement must translate the point under the camera
        // without continuously rotating the local tangent raster. Q/E owns roll.
        {
            Camera2D camera;
            camera.setPlanetRotation(
                GlobeView::orientationAt({0.63, 0.38}, -0.37),
                grid.width(),
                grid.height()
            );
            camera.setZoom(8.0);
            const auto before = GlobeView::from(camera, grid, 1590, 928);
            const double rollBefore = before.surfaceRollRadians();
            const double xBefore = camera.tileX();
            const double yBefore = camera.tileY();

            GlobeCameraNavigation::pan(
                camera,
                grid,
                1590,
                928,
                1.0,
                0.45,
                19.0
            );

            const auto after = GlobeView::from(camera, grid, 1590, 928);
            PALADIN_CHECK(
                std::abs(closeWorldAngleDelta(
                    after.surfaceRollRadians(),
                    rollBefore
                )) < 1e-10
            );
            PALADIN_CHECK(
                std::hypot(
                    camera.tileX() - xBefore,
                    camera.tileY() - yBefore
                ) > 1e-6
            );

            // Dedicated roll remains functional after the pan-stability rule.
            GlobeCameraNavigation::roll(camera, grid, 1590, 928, 0.21);
            const auto rolled = GlobeView::from(camera, grid, 1590, 928);
            PALADIN_CHECK(
                std::abs(closeWorldAngleDelta(
                    rolled.surfaceRollRadians(),
                    after.surfaceRollRadians()
                )) > 0.20
            );
        }

        // Snapping the terrain SOURCE camera to 1/16 tile must not perturb the
        // tangent angle. Otherwise every art-pixel step rerotates the full map.
        {
            Camera2D camera;
            camera.setPlanetRotation(
                GlobeView::orientationAt({0.51437, 0.43791}, -0.41),
                grid.width(),
                grid.height()
            );
            camera.setZoom(12.0);
            const auto stable = pixelStableWorldCamera(
                camera,
                grid,
                1590,
                928,
                true
            );
            const double actualRoll =
                GlobeView::from(camera, grid, 1590, 928).surfaceRollRadians();
            const double stableRoll =
                GlobeView::from(stable, grid, 1590, 928).surfaceRollRadians();
            PALADIN_CHECK(
                std::abs(closeWorldAngleDelta(actualRoll, stableRoll)) < 1e-10
            );
        }

        // The snapped source keeps INTERNAL texels stable. Its leftover camera
        // motion is returned as a whole physical-screen-pixel translation, so
        // the authoritative center remains visually centered to <= half a pixel.
        {
            Camera2D camera;
            camera.setPlanetRotation(
                GlobeView::orientationAt({0.51437, 0.43791}, -0.41),
                grid.width(),
                grid.height()
            );
            camera.setZoom(12.0);
            constexpr double tilePixels = 64.0;
            const auto stable = pixelStableWorldCamera(
                camera,
                grid,
                1590,
                928,
                true
            );
            const auto tangent = LocalTangentWorldView::from(
                stable,
                grid,
                1590,
                928,
                tilePixels
            );
            const auto residual = tangent.rigidOffsetToCenter(
                camera.tileX(),
                camera.tileY()
            );
            const auto presented = tangent.translated(residual.x, residual.y);
            const auto center = presented.projectTiles(
                camera.tileX(),
                camera.tileY()
            );
            PALADIN_CHECK(std::abs(center.x - 1590.0 * 0.5) <= 0.500001);
            PALADIN_CHECK(std::abs(center.y - 928.0 * 0.5) <= 0.500001);
            PALADIN_CHECK(residual.x == std::round(residual.x));
            PALADIN_CHECK(residual.y == std::round(residual.y));
        }

        // Regression for the supplied 1590x928 close-zoom screenshot: uniform
        // square-view overscan is insufficient on a rotated widescreen target.
        // Every viewport corner (plus a guard margin) must lie inside the final
        // rotated tangent rectangle, eliminating opposite black corner wedges.
        {
            Camera2D camera;
            camera.setPlanetRotation(
                GlobeView::orientationAt({0.55, 0.42}, -0.39),
                grid.width(),
                grid.height()
            );
            const auto tangent = LocalTangentWorldView::from(
                camera,
                grid,
                1590,
                928,
                64.0
            );
            constexpr double margin = 8.0;
            const double scale = tangent.overscanScale(margin);
            const double halfW = 1590.0 * 0.5;
            const double halfH = 928.0 * 0.5;
            const double destHalfW = halfW * scale;
            const double destHalfH = halfH * scale;
            const double c = std::cos(tangent.rollRadians());
            const double s = std::sin(tangent.rollRadians());

            for (const double x : std::array{-halfW - margin, halfW + margin})
            {
                for (const double y :
                     std::array{-halfH - margin, halfH + margin})
                {
                    // Inverse-rotate a screen corner into the destination
                    // rectangle's local axes.
                    const double localX = c * x + s * y;
                    const double localY = -s * x + c * y;
                    PALADIN_CHECK(std::abs(localX) <= destHalfW + 1e-8);
                    PALADIN_CHECK(std::abs(localY) <= destHalfH + 1e-8);
                }
            }

            const double oldSquareOnly =
                std::abs(c) + std::abs(s);
            PALADIN_CHECK(scale > oldSquareOnly);
        }
    }
} // namespace Paladin::Test

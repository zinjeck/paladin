#pragma once
#include "rendering/GlobeView.h"
#include "rendering/LocalTangentWorldView.h"
#include "rendering/TileRenderMetrics.h"
#include "rendering/WorldPixelStability.h"
#include "rendering/WorldPresentation.h"
#include "ui/UiTypes.h"
namespace Paladin
{
    // All views address the same canonical tile coordinates and longitude seam.
    struct WorldMapNavigation
    {
        static UiRectangle mapBounds(int width, int height)
        {
            const float w = std::min(260.F, float(width) * .27F), h = w * .5F;
            return {float(width) - w - 12, float(height) - h - 62, w, h};
        }
        static UiRectangle buttonBounds(int width, int height)
        {
            const auto b = mapBounds(width, height);
            return {b.x, b.y - 30.0F, 58.0F, 26.0F};
        }
        static UiRectangle politicalModeButtonBounds(int width, int height)
        {
            const auto b = buttonBounds(width, height);
            return {b.x, b.y, 26.0F, b.height};
        }
        static UiRectangle terrainModeButtonBounds(int width, int height)
        {
            const auto b = buttonBounds(width, height);
            return {b.x + 32.0F, b.y, 26.0F, b.height};
        }
        static WorldSurface::UV minimapPoint(UiRectangle b, double x, double y)
        {
            return {
                std::clamp((x - b.x) / b.width, 0., std::nextafter(1., 0.)),
                std::clamp((y - b.y) / b.height, 0., std::nextafter(1., 0.))
            };
        }
        static std::optional<WorldSurface::UV> pick(
            const Camera2D& c,
            const WorldGrid& g,
            int w,
            int h,
            double pixels,
            bool globe,
            double x,
            double y
        )
        {
            if (globe)
            {
                const auto globeView = GlobeView::from(c, g, w, h);
                const double effectivePixels =
                    g.width() > 0
                        ? globeView.radius * 6.28318530717958647692 / g.width()
                        : 0.0;
                const auto presentation =
                    worldPresentationState(effectivePixels);
                if (presentation.localWorldWeight >= 0.5F)
                {
                    // The close renderer rasterizes around a snapped 1/16-tile
                    // source camera, then translates that finished raster by the
                    // authoritative camera remainder. Picking must use the same
                    // final translated chart or clicks drift by several screen
                    // pixels at maximum zoom.
                    Camera2D renderCamera = pixelStableWorldCamera(
                        c,
                        g,
                        w,
                        h,
                        true
                    );
                    auto tangent = LocalTangentWorldView::from(
                        renderCamera,
                        g,
                        w,
                        h,
                        effectivePixels
                    );
                    const auto residual =
                        tangent.rigidOffsetToCenter(c.tileX(), c.tileY());
                    tangent = tangent.translated(residual.x, residual.y);
                    return tangent.pick(x, y);
                }
                return globeView.pick(x, y);
            }
            double u = (c.tileX() + (x - w * .5) / pixels) / g.width();
            double v = (c.tileY() + (y - h * .5) / pixels) / g.height();
            if (u < 0 || u >= 1 || v < 0 || v >= 1)
            {
                return std::nullopt;
            }
            return WorldSurface::UV{u, v};
        }
        static void focus(
            Camera2D& c,
            const WorldGrid& g,
            int w,
            int h,
            bool globe,
            WorldSurface::UV uv
        )
        {
            if (!globe)
            {
                c.setPosition(uv.u * g.width(), uv.v * g.height());
                return;
            }
            auto q = GlobeView::from(c, g, w, h).orientation();
            auto point = q.apply(WorldSurface::sphere(uv.u, uv.v));
            c.setPlanetRotation(
                PlanetRotation::between(point, {0, 0, 1}) * q,
                g.width(),
                g.height()
            );
        }
    };
} // namespace Paladin

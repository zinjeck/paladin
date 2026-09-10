#pragma once

#include "rendering/GlobeRenderer.h"
#include "world/PlanetAstronomy.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>

namespace Paladin
{
    struct CelestialSunScreenPosition
    {
        float x = 0;
        float y = 0;
        float scale = 1;
        float extent = 0;
    };

    // Project the visible star through the same celestial orientation used by
    // the fixed star shell. Nothing is clamped to the viewport: the sun can
    // leave the screen naturally as the globe camera turns.
    inline std::optional<CelestialSunScreenPosition> projectCelestialSun(
        const GlobeView& view,
        int width,
        int height,
        double secondsIntoDay
    )
    {
        if (width <= 0 || height <= 0)
        {
            return std::nullopt;
        }

        const auto solar = PlanetAstronomy::sunDirection(secondsIntoDay);
        const auto direction = view.orient({solar.x, solar.y, solar.z});
        if (!std::isfinite(direction.x) || !std::isfinite(direction.y) ||
            !std::isfinite(direction.z) || direction.z <= .05)
        {
            return std::nullopt;
        }

        const float minimumDimension = float(std::min(width, height));
        const float focal = minimumDimension * .65F;
        const float scale = std::clamp(minimumDimension / 1080.F, .75F, 1.60F);
        const float x = float(view.cx) + focal * float(direction.x / direction.z);
        const float y = float(view.cy) - focal * float(direction.y / direction.z);
        const float extent = 68.F * scale;

        if (x + extent < 0 || y + extent < 0 || x - extent >= width ||
            y - extent >= height)
        {
            return std::nullopt;
        }
        return CelestialSunScreenPosition{x, y, scale, extent};
    }

    inline void drawSunDiscOutsideGlobe(
        Renderer& renderer,
        float centerX,
        float centerY,
        float radius,
        float globeX,
        float globeY,
        float globeRadius,
        RenderColor color
    )
    {
        std::array<RenderRectangle, 256> spans{};
        std::size_t count = 0;
        const int bound = int(std::ceil(radius));
        for (int row = -bound; row <= bound && count < spans.size(); ++row)
        {
            const float dy = float(row);
            const float circle = radius * radius - dy * dy;
            if (circle <= 0)
            {
                continue;
            }

            const float half = std::sqrt(circle);
            const float y = std::floor(centerY + dy);
            float left = std::floor(centerX - half);
            float right = std::ceil(centerX + half);

            const float globeDy = y + .5F - globeY;
            const float globeCircle =
                globeRadius * globeRadius - globeDy * globeDy;
            if (globeCircle <= 0)
            {
                spans[count++] = {left, y, std::max(1.F, right - left), 1.F};
                continue;
            }

            const float globeHalf = std::sqrt(globeCircle);
            const float globeLeft = globeX - globeHalf;
            const float globeRight = globeX + globeHalf;
            if (right <= globeLeft || left >= globeRight)
            {
                spans[count++] = {left, y, std::max(1.F, right - left), 1.F};
                continue;
            }

            if (left < globeLeft && count < spans.size())
            {
                spans[count++] = {
                    left,
                    y,
                    std::max(0.F, globeLeft - left),
                    1.F
                };
            }
            if (right > globeRight && count < spans.size())
            {
                spans[count++] = {
                    globeRight,
                    y,
                    std::max(0.F, right - globeRight),
                    1.F
                };
            }
        }

        if (count)
        {
            renderer.fillRectangles(
                std::span<const RenderRectangle>(spans.data(), count),
                color
            );
        }
    }

    inline void drawSunRayOutsideGlobe(
        Renderer& renderer,
        float centerX,
        float centerY,
        float angle,
        float inner,
        float outer,
        float globeX,
        float globeY,
        float globeRadius,
        RenderColor color
    )
    {
        const float ux = std::cos(angle), uy = std::sin(angle);
        const float x0 = centerX + ux * inner;
        const float y0 = centerY + uy * inner;
        const float x1 = centerX + ux * outer;
        const float y1 = centerY + uy * outer;
        const float dx = x1 - x0, dy = y1 - y0;
        const float fx = x0 - globeX, fy = y0 - globeY;
        const float a = dx * dx + dy * dy;
        const float b = 2.F * (fx * dx + fy * dy);
        const float c = fx * fx + fy * fy - globeRadius * globeRadius;
        const float discriminant = b * b - 4.F * a * c;

        const auto drawRange = [&](float from, float to)
        {
            if (to - from <= 1e-4F)
            {
                return;
            }
            renderer.drawLine(
                x0 + dx * from,
                y0 + dy * from,
                x0 + dx * to,
                y0 + dy * to,
                color
            );
        };

        if (a <= 1e-6F || discriminant <= 0)
        {
            if (c >= 0)
            {
                drawRange(0, 1);
            }
            return;
        }

        const float root = std::sqrt(discriminant);
        float enter = (-b - root) / (2.F * a);
        float leave = (-b + root) / (2.F * a);
        if (enter > leave)
        {
            std::swap(enter, leave);
        }
        if (leave <= 0 || enter >= 1)
        {
            drawRange(0, 1);
            return;
        }
        drawRange(0, std::clamp(enter, 0.F, 1.F));
        drawRange(std::clamp(leave, 0.F, 1.F), 1.F);
    }

    inline void drawCelestialSun(
        Renderer& renderer,
        const GlobeView& view,
        double secondsIntoDay
    )
    {
        const auto projected = projectCelestialSun(
            view,
            renderer.outputWidth(),
            renderer.outputHeight(),
            secondsIntoDay
        );
        if (!projected)
        {
            return;
        }

        const float x = projected->x, y = projected->y;
        const float scale = projected->scale;
        // The globe renderer already draws a 2.4% atmosphere outside the
        // sphere. Keep the later celestial pass behind both the planet and that
        // limb without a stencil or framebuffer readback.
        const float occlusionRadius = float(view.radius * 1.03);
        const float centerDistance =
            std::hypot(x - float(view.cx), y - float(view.cy));
        if (centerDistance < occlusionRadius - 18.F * scale)
        {
            return;
        }

        struct Ray
        {
            float angle;
            float length;
            std::uint8_t alpha;
        };
        static constexpr std::array<Ray, 9> rays{{
            {0.11F, 55.F, 34},
            {0.79F, 40.F, 24},
            {1.48F, 64.F, 38},
            {2.17F, 35.F, 22},
            {2.91F, 52.F, 31},
            {3.66F, 43.F, 24},
            {4.43F, 61.F, 35},
            {5.19F, 37.F, 22},
            {5.88F, 48.F, 28}
        }};
        for (const auto& ray : rays)
        {
            drawSunRayOutsideGlobe(
                renderer,
                x,
                y,
                ray.angle,
                7.F * scale,
                ray.length * scale,
                float(view.cx),
                float(view.cy),
                occlusionRadius,
                {255, 232, 173, ray.alpha}
            );
        }

        // A compact layered corona borrows the reference photograph's
        // white-hot center and radial bloom, but deliberately compresses its
        // dynamic range so the globe remains the visual subject.
        drawSunDiscOutsideGlobe(
            renderer,
            x,
            y,
            34.F * scale,
            float(view.cx),
            float(view.cy),
            occlusionRadius,
            {255, 232, 173, 8}
        );
        drawSunDiscOutsideGlobe(
            renderer,
            x,
            y,
            24.F * scale,
            float(view.cx),
            float(view.cy),
            occlusionRadius,
            {255, 240, 196, 13}
        );
        drawSunDiscOutsideGlobe(
            renderer,
            x,
            y,
            15.F * scale,
            float(view.cx),
            float(view.cy),
            occlusionRadius,
            {255, 240, 196, 25}
        );
        drawSunDiscOutsideGlobe(
            renderer,
            x,
            y,
            9.F * scale,
            float(view.cx),
            float(view.cy),
            occlusionRadius,
            {244, 243, 232, 62}
        );
        drawSunDiscOutsideGlobe(
            renderer,
            x,
            y,
            5.5F * scale,
            float(view.cx),
            float(view.cy),
            occlusionRadius,
            {244, 243, 232, 225}
        );
        drawSunDiscOutsideGlobe(
            renderer,
            x,
            y,
            2.5F * scale,
            float(view.cx),
            float(view.cy),
            occlusionRadius,
            {244, 243, 232, 255}
        );
    }

    // Keep the existing globe renderer untouched so this feature remains easy
    // to reconcile with parallel renderer work. The celestial pass is bounded,
    // allocation-free per frame, and clipped as if it lived behind the globe.
    class CelestialGlobeRenderer : public GlobeRenderer
    {
    public:
        using GlobeRenderer::GlobeRenderer;

        void render(
            Renderer& renderer,
            const World& world,
            const Camera2D& camera,
            const SceneSpriteLibrary& art,
            std::span<const TileOverlayRenderItem> overlays,
            std::span<const TileOutlineRenderItem> outlines
        )
        {
            GlobeRenderer::render(renderer, world, camera, art, overlays, outlines);
            const auto view = GlobeView::from(
                camera,
                world.grid(),
                renderer.outputWidth(),
                renderer.outputHeight()
            );
            drawCelestialSun(renderer, view, world.time().secondsIntoDay());
        }
    };
} // namespace Paladin

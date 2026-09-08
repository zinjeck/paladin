#pragma once
#include "rendering/CityPresentation.h"
#include "rendering/GlobeLighting.h"
#include "rendering/SceneDetail.h"
#include "rendering/SceneSpriteLibrary.h"
#include "world/settlements/SettlementMap.h"
#include <array>
#include <cmath>
#include <limits>

namespace Paladin
{
    // Continuous illumination is deliberately separate from authored sprite
    // colors. A bounded, quarter-resolution light field supports the SDL
    // software backend too.
    class CityLighting
    {
        struct Building
        {
            double x, y, w, h, height;
            std::string type;
        };
        struct Lamp
        {
            double x, y, radius, intensity;
            RenderColor color;
        };
        std::vector<Building> buildings_;
        std::uint64_t instance_ = 0, version_ = ~std::uint64_t(0);
        std::unique_ptr<Texture> light_, glow_;
        std::vector<RenderColor> pixels_, emission_;
        int width_ = 0, height_ = 0;
        std::array<double, 7> lastKey_{};
        bool cached_ = false;

    public:
        void reset()
        {
            instance_ = 0;
            cached_ = false;
        }
        void render(
            Renderer& renderer,
            const SceneProjection& projection,
            const SettlementMap& map,
            const SceneSpriteLibrary& sprites,
            const CityPresentation& policy,
            double hour,
            double sunIncidence = std::numeric_limits<double>::quiet_NaN()
        )
        {
            if (!policy.daylightEnabled)
            {
                return;
            }
            // Live city views receive the geographic sun at their settlement.
            // Standalone art previews use an equatorial local clock.
            const double day = solarIllumination(
                std::isfinite(sunIncidence) ? sunIncidence
                                            : globeSunDot(.5, .5, hour * 3600.)
            );
            if (day >= 1)
            {
                return;
            }
            const double effectTime =
                projection.tilePixels >= AnimationDetailPixels
                    ? std::floor(sprites.time() * 10) / 10
                    : 0;
            const std::array<double, 7> key{
                projection.cameraX,
                projection.cameraY,
                projection.tilePixels,
                double(renderer.outputWidth()),
                double(renderer.outputHeight()),
                day,
                policy.localLightsEnabled ? effectTime : -1
            };
            if (cached_ && key == lastKey_ && instance_ == map.instanceId() &&
                version_ == map.objectState().navigationVersion() && light_ &&
                glow_)
            {
                renderer.compositeLighting(*light_, *glow_);
                return;
            }
            if (instance_ != map.instanceId() ||
                version_ != map.objectState().navigationVersion())
            {
                instance_ = map.instanceId();
                version_ = map.objectState().navigationVersion();
                buildings_.clear();
                for (const auto& o : map.objectState().completedObjects())
                {
                    const auto& style = sprites.objectStyle(o.objectTypeId);
                    if (style.mode == "enclosed")
                    {
                        buildings_.push_back(
                            {double(o.footprint.topLeft.x),
                             double(o.footprint.topLeft.y),
                             double(o.footprint.width),
                             double(o.footprint.height),
                             style.height,
                             o.objectTypeId}
                        );
                    }
                }
            }
            const int w = std::max(1, (renderer.outputWidth() + 3) / 4),
                      h = std::max(1, (renderer.outputHeight() + 3) / 4);
            if (w != width_ || h != height_)
            {
                width_ = w;
                height_ = h;
                light_.reset();
                glow_.reset();
            }
            const auto byte = [](double x)
            { return std::uint8_t(std::clamp(x, 0.0, 255.0)); };
            const RenderColor ambient{
                byte(255 * (.12 + .88 * day)),
                byte(255 * (.16 + .84 * day)),
                byte(255 * (.34 + .66 * day)),
                255
            };
            pixels_.assign(std::size_t(w) * h, ambient);
            emission_.assign(pixels_.size(), {0, 0, 0, 255});
            std::vector<RenderRectangle> obstacles;
            std::vector<Lamp> lamps;
            for (const auto& b : buildings_)
            {
                const auto bounds = projection.bounds(
                    {b.x, b.y, b.height, b.w, b.h + b.height, 0, 0}
                );
                const auto lamp = projection.bounds(
                    {b.x + b.w - .35, b.y + b.h - .25, 0, 0, 0, 0, 0}
                );
                const double radius = projection.tilePixels * 16;
                if (lamp.x + radius < 0 || lamp.y + radius < 0 ||
                    lamp.x - radius > projection.screenWidth ||
                    lamp.y - radius > projection.screenHeight)
                {
                    continue;
                }
                if (obstacles.size() < 128)
                {
                    obstacles.push_back(bounds);
                }
                for (const auto& spec : sprites.lights())
                {
                    if (spec.object == b.type && lamps.size() < 32)
                    {
                        const auto& blueprint = sprites.objectStyle(b.type);
                        // Facade lights follow the fitted wall: horizontal
                        // position scales, distance from its front stays fixed.
                        const auto at = projection.bounds(
                            {b.x + spec.x * b.w / blueprint.moduleWidth,
                             b.y + spec.y + b.h - blueprint.moduleDepth,
                             0,
                             0,
                             0,
                             0,
                             0}
                        );
                        const double reach =
                            spec.radius * projection.tilePixels;
                        if (at.x + reach < 0 || at.y + reach < 0 ||
                            at.x - reach > projection.screenWidth ||
                            at.y - reach > projection.screenHeight)
                        {
                            continue;
                        }
                        lamps.push_back(
                            {at.x / 4.0,
                             at.y / 4.0,
                             spec.radius * projection.tilePixels / 4,
                             spec.intensity * (.93 +
                                               .045 * std::sin(
                                                          effectTime * 5.1 +
                                                          b.x * 2.3 + spec.x
                                                      ) +
                                               .025 * std::sin(
                                                          effectTime * 8.7 +
                                                          b.y * 1.7 + spec.y
                                                      )),
                             spec.color}
                        );
                    }
                }
            }
            if (policy.localLightsEnabled)
            {
                for (const auto& lamp : lamps)
                {
                    const double cx = lamp.x, cy = lamp.y,
                                 radius = std::max(2.0, lamp.radius);
                    std::array<double, 128> reach;
                    reach.fill(radius);
                    // Angular shadow distances: opaque building silhouettes
                    // block neighboring lamps.
                    if (projection.tilePixels >= AnimationDetailPixels)
                    {
                        for (std::size_t a = 0; a < reach.size(); ++a)
                        {
                            const double angle =
                                (a + .5) * 6.283185307179586 / reach.size();
                            const double dx = std::cos(angle),
                                         dy = std::sin(angle);
                            for (const auto& b : obstacles)
                            {
                                const double x0 = b.x / 4, y0 = b.y / 4,
                                             x1 = (b.x + b.width) / 4,
                                             y1 = (b.y + b.height) / 4;
                                if (cx >= x0 && cx <= x1 && cy >= y0 &&
                                    cy <= y1)
                                {
                                    continue; // own facade
                                }
                                double near = 0, far = radius;
                                for (int axis = 0; axis < 2; ++axis)
                                {
                                    const double d = axis ? dy : dx,
                                                 origin = axis ? cy : cx,
                                                 lo = axis ? y0 : x0,
                                                 hi = axis ? y1 : x1;
                                    if (std::abs(d) < 1e-8)
                                    {
                                        if (origin < lo || origin > hi)
                                        {
                                            far = -1;
                                        }
                                    }
                                    else
                                    {
                                        double t0 = (lo - origin) / d,
                                               t1 = (hi - origin) / d;
                                        if (t0 > t1)
                                        {
                                            std::swap(t0, t1);
                                        }
                                        near = std::max(near, t0);
                                        far = std::min(far, t1);
                                    }
                                }
                                if (far >= near)
                                {
                                    reach[a] = std::min(reach[a], near);
                                }
                            }
                        }
                    }
                    const int left = std::max(0, int(cx - radius)),
                              right = std::min(w, int(std::ceil(cx + radius)));
                    const int top = std::max(0, int(cy - radius)),
                              bottom = std::min(h, int(std::ceil(cy + radius)));
                    for (int y = top; y < bottom; ++y)
                    {
                        for (int x = left; x < right; ++x)
                        {
                            const double dx = x + .5 - cx, dy = y + .5 - cy,
                                         d = std::hypot(dx, dy);
                            if (d >= radius)
                            {
                                continue;
                            }
                            double angle = std::atan2(dy, dx);
                            if (angle < 0)
                            {
                                angle += 6.283185307179586;
                            }
                            const int a = std::min(
                                127,
                                int(angle * 128 / 6.283185307179586)
                            );
                            if (d > reach[a] + .75)
                            {
                                continue;
                            }
                            const double fall = std::pow(1 - d / radius, 2) *
                                                (1 - day) * lamp.intensity;
                            auto& p = pixels_[std::size_t(y) * w + x];
                            p.red = byte(p.red + lamp.color.red * fall * 1.2);
                            p.green =
                                byte(p.green + lamp.color.green * fall * 1.2);
                            p.blue =
                                byte(p.blue + lamp.color.blue * fall * 1.2);
                            // Soft bloom plus a small luminous source; no
                            // full-scene orange wash.
                            const double bloom =
                                (std::exp(-d * d / (radius * radius * .012)) *
                                     .22 +
                                 std::exp(-d * d / 1.2) * .65) *
                                (1 - day) * lamp.intensity;
                            auto& e = emission_[std::size_t(y) * w + x];
                            e.red = byte(e.red + lamp.color.red * bloom);
                            e.green = byte(e.green + lamp.color.green * bloom);
                            e.blue = byte(e.blue + lamp.color.blue * bloom);
                        }
                    }
                }
            }
            if (!light_)
            {
                light_ = renderer.createTextureFromPixels(w, h, pixels_);
            }
            else if (!renderer.updateTexturePixels(*light_, pixels_))
            {
                return;
            }
            if (!glow_)
            {
                glow_ = renderer.createTextureFromPixels(w, h, emission_);
            }
            else if (!renderer.updateTexturePixels(*glow_, emission_))
            {
                return;
            }
            if (light_ && glow_)
            {
                lastKey_ = key;
                cached_ = true;
                renderer.compositeLighting(*light_, *glow_);
            }
        }
    };
} // namespace Paladin

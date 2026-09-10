#pragma once

#include "rendering/GlobeView.h"
#include "rendering/Renderer.h"
#include "rendering/Texture.h"
#include "world/PlanetAstronomy.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace Paladin
{
    struct CelestialSunScreenPosition
    {
        float x = 0;
        float y = 0;
        float scale = 1;
        float extent = 0;
    };

    inline WorldSurface::Point3 celestialSunViewDirection(
        const GlobeView& view,
        double secondsIntoDay
    )
    {
        const auto solar = PlanetAstronomy::sunDirection(secondsIntoDay);
        return view.orient({solar.x, solar.y, solar.z});
    }

    // GlobeView uses +Z for the hemisphere facing the camera. The visible sun
    // is a distant background source, so it is projectable only while the
    // planet-to-sun direction lies in the far half-space (negative view Z).
    // The X/Y signs are preserved exactly. That makes the projected star sit on
    // the same side of the globe as the sunlit crescent produced by the same
    // PlanetAstronomy vector.
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

        const auto direction = celestialSunViewDirection(view, secondsIntoDay);
        if (!std::isfinite(direction.x) || !std::isfinite(direction.y) ||
            !std::isfinite(direction.z) || direction.z >= -.035)
        {
            return std::nullopt;
        }

        const float minimumDimension = float(std::min(width, height));
        const float focal = minimumDimension * .65F;
        const float scale = std::clamp(minimumDimension / 1080.F, .72F, 1.80F);
        const float depth = float(-direction.z);
        const float x = float(view.cx) + focal * float(direction.x) / depth;
        const float y = float(view.cy) - focal * float(direction.y) / depth;
        // Extent is the photographic corona, not the white solar disc. The
        // actual disc is intentionally tiny relative to the globe.
        const float extent = 190.F * scale;

        if (x + extent < 0 || y + extent < 0 || x - extent >= width ||
            y - extent >= height)
        {
            return std::nullopt;
        }
        return CelestialSunScreenPosition{x, y, scale, extent};
    }

    namespace CelestialSunDetail
    {
        inline float smooth01(float value)
        {
            value = std::clamp(value, 0.F, 1.F);
            return value * value * (3.F - 2.F * value);
        }

        struct Ray
        {
            float angle;
            float width;
            float length;
            float strength;
        };

        // Fixed optical spikes: deterministic from frame to frame, slightly
        // asymmetric, and narrow enough to read as camera diffraction rather
        // than hand-drawn game rays.
        inline constexpr std::array<Ray, 12> Rays{{
            {0.018F, .030F, .98F, .22F},
            {3.158F, .027F, .91F, .18F},
            {1.626F, .026F, .87F, .17F},
            {4.739F, .024F, .82F, .15F},
            {.770F, .021F, .74F, .13F},
            {3.934F, .020F, .69F, .11F},
            {2.304F, .018F, .63F, .10F},
            {5.474F, .017F, .59F, .09F},
            {1.135F, .014F, .54F, .075F},
            {4.285F, .013F, .49F, .065F},
            {2.765F, .012F, .45F, .055F},
            {5.930F, .011F, .41F, .050F}
        }};
    } // namespace CelestialSunDetail

    // A smooth, native-resolution optical sample. This is runtime emitted light,
    // not authored sprite art, so continuous color/alpha is intentional and
    // separate from Paladin's 64-color source-art palette invariant.
    inline RenderColor celestialSunSample(float normalizedX, float normalizedY)
    {
        using namespace CelestialSunDetail;
        const float radius = std::hypot(normalizedX, normalizedY);
        if (!std::isfinite(radius) || radius >= 1.F)
        {
            return {0, 0, 0, 0};
        }

        const float core = smooth01((.060F - radius) / .018F);
        const float innerBloom = .96F * std::exp(-radius * 16.F);
        const float middleBloom = .23F * std::exp(-radius * 5.0F);
        const float outerHalo = .052F * std::exp(-radius * 2.7F);

        const float angle = std::atan2(normalizedY, normalizedX);
        const float rayGate = smooth01((radius - .025F) / .055F);
        const float edgeFade = smooth01((1.F - radius) / .14F);
        float rayField = 0.F;
        for (const auto& ray : Rays)
        {
            const float delta = std::atan2(
                std::sin(angle - ray.angle),
                std::cos(angle - ray.angle)
            );
            const float angular = std::exp(
                -.5F * (delta / ray.width) * (delta / ray.width)
            );
            const float radial = std::exp(
                -std::pow(radius / ray.length, 1.35F)
            );
            rayField += ray.strength * angular * radial;
        }
        rayField *= rayGate * edgeFade;

        // Very faint sensor-like streaks keep the source photographic without
        // turning it into a lens-flare UI effect.
        const float opticalStreak =
            .026F * rayGate * edgeFade *
            (std::exp(-std::abs(normalizedY) * 96.F) *
                 std::exp(-radius * 2.8F) +
             .68F * std::exp(-std::abs(normalizedX) * 112.F) *
                 std::exp(-radius * 3.1F));

        const float radiance = std::clamp(
            core + innerBloom + middleBloom + outerHalo + rayField +
                opticalStreak,
            0.F,
            1.F
        );
        if (radiance < .00035F)
        {
            return {0, 0, 0, 0};
        }

        // White-hot center grading gently toward a warm outer corona.
        const float white = std::exp(-radius * 8.F);
        const auto green = static_cast<std::uint8_t>(std::clamp(
            std::lround(236.F + 19.F * white),
            0L,
            255L
        ));
        const auto blue = static_cast<std::uint8_t>(std::clamp(
            std::lround(184.F + 71.F * white),
            0L,
            255L
        ));
        const auto alpha = static_cast<std::uint8_t>(std::clamp(
            std::lround(255.F * radiance),
            0L,
            255L
        ));
        return {255, green, blue, alpha};
    }

    class CelestialSunRenderer
    {
    public:
        bool prepare(Renderer& renderer) const
        {
            return ensureTexture(renderer);
        }

        void reset() const
        {
            texture_.reset();
            textureOwner_ = nullptr;
        }

        void render(
            Renderer& renderer,
            const GlobeView& view,
            double secondsIntoDay,
            float opacity = 1.F
        ) const
        {
            opacity = std::clamp(opacity, 0.F, 1.F);
            if (opacity <= .001F || !ensureTexture(renderer))
            {
                return;
            }

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

            const float halfSize = projected->extent;
            const float diameter = halfSize * 2.F;
            const float left = projected->x - halfSize;
            const float top = projected->y - halfSize;
            const float right = projected->x + halfSize;
            const float globeX = float(view.cx);
            const float globeY = float(view.cy);
            // GlobeRenderer's atmosphere reaches a little beyond the terrain
            // sphere. Conservatively hide the optical source behind that limb.
            const float globeRadius = float(view.radius * 1.03);

            constexpr std::size_t StripCount = 256;
            constexpr std::size_t MaxQuads = StripCount * 2;
            std::array<MeshVertex, MaxQuads * 4> vertices{};
            std::array<int, MaxQuads * 6> indices{};
            std::size_t quadCount = 0;

            const auto alpha = static_cast<std::uint8_t>(std::clamp(
                std::lround(255.F * opacity),
                0L,
                255L
            ));
            const RenderColor modulation{255, 255, 255, alpha};

            const auto addQuad = [&](float x0,
                                     float x1,
                                     float y0,
                                     float y1,
                                     float u0,
                                     float u1,
                                     float v0,
                                     float v1)
            {
                if (x1 - x0 <= .01F || y1 - y0 <= .01F ||
                    quadCount >= MaxQuads)
                {
                    return;
                }
                const int vertex = int(quadCount * 4);
                const std::size_t vi = quadCount * 4;
                vertices[vi + 0] = {x0, y0, u0, v0, modulation};
                vertices[vi + 1] = {x1, y0, u1, v0, modulation};
                vertices[vi + 2] = {x1, y1, u1, v1, modulation};
                vertices[vi + 3] = {x0, y1, u0, v1, modulation};
                const std::size_t ii = quadCount * 6;
                indices[ii + 0] = vertex;
                indices[ii + 1] = vertex + 1;
                indices[ii + 2] = vertex + 2;
                indices[ii + 3] = vertex;
                indices[ii + 4] = vertex + 2;
                indices[ii + 5] = vertex + 3;
                ++quadCount;
            };

            for (std::size_t strip = 0; strip < StripCount; ++strip)
            {
                const float v0 = float(strip) / float(StripCount);
                const float v1 = float(strip + 1) / float(StripCount);
                const float y0 = top + diameter * v0;
                const float y1 = top + diameter * v1;

                // Use the Y within the strip nearest the globe center. This
                // slightly over-occludes each ~1-2 px native strip rather than
                // leaking a bright solar pixel through the planet silhouette.
                const float maskY = std::clamp(globeY, y0, y1);
                const float dy = maskY - globeY;
                const float circle = globeRadius * globeRadius - dy * dy;
                if (circle <= 0.F)
                {
                    addQuad(left, right, y0, y1, 0.F, 1.F, v0, v1);
                    continue;
                }

                const float globeHalf = std::sqrt(circle);
                const float globeLeft = globeX - globeHalf;
                const float globeRight = globeX + globeHalf;
                if (right <= globeLeft || left >= globeRight)
                {
                    addQuad(left, right, y0, y1, 0.F, 1.F, v0, v1);
                    continue;
                }

                if (left < globeLeft)
                {
                    const float x1 = std::min(right, globeLeft);
                    addQuad(
                        left,
                        x1,
                        y0,
                        y1,
                        0.F,
                        (x1 - left) / diameter,
                        v0,
                        v1
                    );
                }
                if (right > globeRight)
                {
                    const float x0 = std::max(left, globeRight);
                    addQuad(
                        x0,
                        right,
                        y0,
                        y1,
                        (x0 - left) / diameter,
                        1.F,
                        v0,
                        v1
                    );
                }
            }

            if (!quadCount)
            {
                return;
            }

            renderer.drawMesh(
                *texture_,
                std::span<const MeshVertex>(vertices.data(), quadCount * 4),
                std::span<const int>(indices.data(), quadCount * 6)
            );
        }

    private:
        bool ensureTexture(Renderer& renderer) const
        {
            if (texture_ && textureOwner_ == &renderer)
            {
                return true;
            }

            texture_.reset();
            textureOwner_ = &renderer;
            constexpr int Side = 512;
            static const std::vector<RenderColor> pixels = []
            {
                std::vector<RenderColor> result(
                    std::size_t(Side) * Side,
                    {0, 0, 0, 0}
                );
                for (int y = 0; y < Side; ++y)
                {
                    const float ny =
                        (float(y) + .5F) / float(Side) * 2.F - 1.F;
                    for (int x = 0; x < Side; ++x)
                    {
                        const float nx =
                            (float(x) + .5F) / float(Side) * 2.F - 1.F;
                        result[std::size_t(y) * Side + x] =
                            celestialSunSample(nx, ny);
                    }
                }
                return result;
            }();

            texture_ = renderer.createTextureFromPixels(Side, Side, pixels);
            if (!texture_)
            {
                return false;
            }
            renderer.setTextureFiltering(*texture_, true);
            return true;
        }

        mutable std::unique_ptr<Texture> texture_;
        mutable const Renderer* textureOwner_ = nullptr;
    };
} // namespace Paladin

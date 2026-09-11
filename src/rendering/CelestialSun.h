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

    // GlobeView uses +Z for the hemisphere facing the camera. A sun visible in
    // the starfield is therefore on the far side of the world, with negative
    // view-space Z. X/Y signs are preserved exactly so the apparent source and
    // the illuminated limb stay locked to the same PlanetAstronomy vector.
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
        const float scale = std::clamp(minimumDimension / 1080.F, .70F, 2.15F);
        const float depth = float(-direction.z);
        const float x = float(view.cx) + focal * float(direction.x) / depth;
        const float y = float(view.cy) - focal * float(direction.y) / depth;

        // This is the very faint outer glare radius used only for culling. The
        // actual overexposed source is tiny; apparent size comes from bloom.
        const float extent = 310.F * scale;
        if (x + extent < 0 || y + extent < 0 || x - extent >= width ||
            y - extent >= height)
        {
            return std::nullopt;
        }

        return CelestialSunScreenPosition{x, y, scale, extent};
    }

    namespace CelestialSunOptics
    {
        inline float smooth01(float value)
        {
            value = std::clamp(value, 0.F, 1.F);
            return value * value * (3.F - 2.F * value);
        }

        inline std::uint8_t channel(float value)
        {
            return static_cast<std::uint8_t>(std::clamp(
                std::lround(value),
                0L,
                255L
            ));
        }

        struct GlareLobe
        {
            float angle;
            float width;
            float length;
            float strength;
        };

        // Broad deterministic glare cones. They are texture fields, never line
        // primitives. Width expands with distance so each ray diffuses into the
        // corona instead of reading as a spoke drawn from a circle.
        inline constexpr std::array<GlareLobe, 12> GlareLobes{{
            {.10F, .045F, .98F, .18F},
            {3.25F, .050F, .92F, .15F},
            {1.47F, .048F, .88F, .16F},
            {4.67F, .052F, .83F, .14F},
            {.72F, .060F, .76F, .12F},
            {3.86F, .064F, .71F, .10F},
            {2.24F, .058F, .66F, .11F},
            {5.39F, .062F, .61F, .095F},
            {1.02F, .072F, .57F, .080F},
            {4.12F, .075F, .52F, .070F},
            {2.74F, .068F, .48F, .065F},
            {5.88F, .080F, .44F, .055F}
        }};

        inline float glareLobe(
            float x,
            float y,
            const GlareLobe& lobe
        )
        {
            const float c = std::cos(lobe.angle);
            const float s = std::sin(lobe.angle);
            const float longitudinal = x * c + y * s;
            if (longitudinal <= 0.F)
            {
                return 0.F;
            }

            const float lateral = std::abs(-x * s + y * c);
            const float along = std::clamp(
                longitudinal / std::max(.001F, lobe.length),
                0.F,
                1.F
            );
            const float width = lobe.width * (.45F + 1.15F * along);
            const float transverse = std::exp(
                -.5F * (lateral / width) * (lateral / width)
            );
            const float fade = std::exp(-std::pow(
                longitudinal / std::max(.001F, lobe.length),
                1.15F
            ));
            return lobe.strength * transverse * fade;
        }
    } // namespace CelestialSunOptics

    // Main optical field. It contains no hard solar disc and no line-rendered
    // rays. The image is continuous radial bloom plus broad anisotropic glare.
    inline RenderColor celestialSunCoronaSample(float x, float y)
    {
        using namespace CelestialSunOptics;
        const float radius = std::hypot(x, y);
        if (!std::isfinite(radius) || radius >= 1.F)
        {
            return {0, 0, 0, 0};
        }

        const float angle = std::atan2(y, x);
        const float irregularity = std::clamp(
            1.F + .035F * std::sin(7.F * angle + .3F) +
                .020F * std::sin(15.F * angle + 1.1F),
            .90F,
            1.10F
        );

        const float sourceBloom =
            .18F * std::exp(-std::pow(radius / .065F, 2.F));
        const float innerCorona =
            .42F * std::exp(-std::pow(radius / .22F, 1.35F));
        const float middleCorona =
            .16F * std::exp(-std::pow(radius / .46F, 1.18F));
        const float outerHalo =
            .045F * std::exp(-std::pow(radius / .80F, 1.55F));

        float glare = 0.F;
        for (const auto& lobe : GlareLobes)
        {
            glare += glareLobe(x, y, lobe);
        }
        glare *= smooth01((radius - .045F) / .09F) *
                 smooth01((1.F - radius) / .12F);

        const float radiance = std::clamp(
            (sourceBloom + innerCorona + middleCorona + outerHalo) *
                    irregularity +
                glare,
            0.F,
            .92F
        );
        if (radiance < .0015F)
        {
            return {0, 0, 0, 0};
        }

        const float hot = std::exp(-radius * 4.5F);
        return {
            255,
            channel(230.F + 25.F * hot),
            channel(190.F + 65.F * hot),
            channel(255.F * radiance)
        };
    }

    // Tiny source contribution. It is intentionally not an opaque white disc.
    // Additive composition with the corona clips only the innermost few pixels
    // to white, reproducing camera overexposure without exposing a circle edge.
    inline RenderColor celestialSunCoreSample(float x, float y)
    {
        using namespace CelestialSunOptics;
        const float radius = std::hypot(x, y);
        if (!std::isfinite(radius) || radius >= 1.F)
        {
            return {0, 0, 0, 0};
        }

        const float hot = .44F * std::exp(-std::pow(radius / .10F, 2.F));
        const float bloom =
            .10F * std::exp(-std::pow(radius / .55F, 1.70F));
        float radiance = (hot + bloom) *
                         smooth01((1.F - radius) / .18F);
        radiance = std::clamp(radiance, 0.F, .54F);

        const float white = std::exp(-std::pow(radius / .45F, 1.6F));
        return {
            255,
            channel(248.F + 7.F * white),
            channel(232.F + 23.F * white),
            channel(255.F * radiance)
        };
    }

    // Faint secondary camera artifact. It remains broad and translucent so it
    // never competes with the actual source.
    inline RenderColor celestialSunLensGhostSample(float x, float y)
    {
        using namespace CelestialSunOptics;
        const float radius = std::hypot(x, y);
        if (!std::isfinite(radius) || radius >= 1.F)
        {
            return {0, 0, 0, 0};
        }

        const float ring = std::exp(-std::pow((radius - .53F) / .17F, 2.F));
        const float haze = .35F * std::exp(-std::pow(radius / .72F, 2.F));
        const float edge = smooth01((1.F - radius) / .18F);
        const float radiance = std::clamp(
            (.080F * ring + .025F * haze) * edge,
            0.F,
            .10F
        );
        return {205, 222, 255, channel(255.F * radiance)};
    }

    class CelestialSunRenderer
    {
    public:
        bool prepare(Renderer& renderer) const
        {
            return ensureTextures(renderer);
        }

        void reset() const
        {
            coronaTexture_.reset();
            coreTexture_.reset();
            lensGhostTexture_.reset();
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
            if (opacity <= .001F || !ensureTextures(renderer))
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

            const float globeX = float(view.cx);
            const float globeY = float(view.cy);
            const float globeRadius = float(view.radius * 1.03);

            drawOccludedLayer(
                renderer,
                *coronaTexture_,
                projected->x,
                projected->y,
                285.F * projected->scale,
                globeX,
                globeY,
                globeRadius,
                opacity
            );
            drawOccludedLayer(
                renderer,
                *coreTexture_,
                projected->x,
                projected->y,
                18.F * projected->scale,
                globeX,
                globeY,
                globeRadius,
                opacity
            );

            renderLensGhosts(
                renderer,
                *projected,
                globeX,
                globeY,
                globeRadius,
                opacity
            );
        }

    private:
        static float sourceVisibility(
            const CelestialSunScreenPosition& projected,
            float globeX,
            float globeY,
            float globeRadius
        )
        {
            const float distance = std::hypot(
                projected.x - globeX,
                projected.y - globeY
            );
            const float sourceRadius = 7.F * projected.scale;
            const float lower = globeRadius - sourceRadius;
            const float upper = globeRadius + sourceRadius;
            return CelestialSunOptics::smooth01(
                (distance - lower) / std::max(1.F, upper - lower)
            );
        }

        void renderLensGhosts(
            Renderer& renderer,
            const CelestialSunScreenPosition& projected,
            float globeX,
            float globeY,
            float globeRadius,
            float opacity
        ) const
        {
            const float visible = sourceVisibility(
                projected,
                globeX,
                globeY,
                globeRadius
            );
            if (visible <= .01F || !lensGhostTexture_)
            {
                return;
            }

            const float centerX = renderer.outputWidth() * .5F;
            const float centerY = renderer.outputHeight() * .5F;
            const float axisX = centerX - projected.x;
            const float axisY = centerY - projected.y;

            struct Ghost
            {
                float along;
                float radius;
                float strength;
            };
            static constexpr std::array<Ghost, 3> ghosts{{
                {.43F, 24.F, .090F},
                {.74F, 42.F, .052F},
                {1.08F, 18.F, .034F}
            }};

            for (const auto& ghost : ghosts)
            {
                const float radius = ghost.radius * projected.scale;
                const float strength = opacity * visible * ghost.strength;
                if (strength <= .002F)
                {
                    continue;
                }
                const auto alpha = static_cast<std::uint8_t>(std::clamp(
                    std::lround(255.F * strength),
                    0L,
                    255L
                ));
                const float x = projected.x + axisX * ghost.along - radius;
                const float y = projected.y + axisY * ghost.along - radius;
                renderer.drawTexture(
                    *lensGhostTexture_,
                    0,
                    0,
                    float(lensGhostTexture_->width()),
                    float(lensGhostTexture_->height()),
                    x,
                    y,
                    radius * 2.F,
                    radius * 2.F,
                    alpha
                );
            }
        }

        void drawOccludedLayer(
            Renderer& renderer,
            const Texture& texture,
            float centerX,
            float centerY,
            float halfSize,
            float globeX,
            float globeY,
            float globeRadius,
            float opacity
        ) const
        {
            if (halfSize <= .01F)
            {
                return;
            }

            const float diameter = halfSize * 2.F;
            const float left = centerX - halfSize;
            const float top = centerY - halfSize;
            const float right = centerX + halfSize;

            constexpr std::size_t MaxStrips = 512;
            constexpr std::size_t MaxQuads = MaxStrips * 2;
            std::array<MeshVertex, MaxQuads * 4> vertices{};
            std::array<int, MaxQuads * 6> indices{};
            std::size_t quadCount = 0;

            const int stripCount = std::clamp(
                int(std::ceil(diameter)),
                64,
                int(MaxStrips)
            );
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

            for (int strip = 0; strip < stripCount; ++strip)
            {
                const float v0 = float(strip) / float(stripCount);
                const float v1 = float(strip + 1) / float(stripCount);
                const float y0 = top + diameter * v0;
                const float y1 = top + diameter * v1;

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
                texture,
                std::span<const MeshVertex>(vertices.data(), quadCount * 4),
                std::span<const int>(indices.data(), quadCount * 6)
            );
        }

        bool ensureTextures(Renderer& renderer) const
        {
            if (coronaTexture_ && coreTexture_ && lensGhostTexture_ &&
                textureOwner_ == &renderer)
            {
                return true;
            }

            reset();
            textureOwner_ = &renderer;

            const auto create = [&](int side, auto&& sampler)
                -> std::unique_ptr<Texture>
            {
                std::vector<RenderColor> pixels(
                    std::size_t(side) * side,
                    {0, 0, 0, 0}
                );
                for (int y = 0; y < side; ++y)
                {
                    const float ny =
                        (float(y) + .5F) / float(side) * 2.F - 1.F;
                    for (int x = 0; x < side; ++x)
                    {
                        const float nx =
                            (float(x) + .5F) / float(side) * 2.F - 1.F;
                        pixels[std::size_t(y) * side + x] = sampler(nx, ny);
                    }
                }

                auto texture = renderer.createTextureFromPixels(side, side, pixels);
                if (!texture)
                {
                    return {};
                }
                renderer.setTextureFiltering(*texture, true);
                texture->setAdditiveBlending(true);
                return texture;
            };

            coronaTexture_ = create(1024, celestialSunCoronaSample);
            coreTexture_ = create(384, celestialSunCoreSample);
            lensGhostTexture_ = create(256, celestialSunLensGhostSample);
            if (!coronaTexture_ || !coreTexture_ || !lensGhostTexture_)
            {
                reset();
                return false;
            }
            return true;
        }

        mutable std::unique_ptr<Texture> coronaTexture_;
        mutable std::unique_ptr<Texture> coreTexture_;
        mutable std::unique_ptr<Texture> lensGhostTexture_;
        mutable const Renderer* textureOwner_ = nullptr;
    };
} // namespace Paladin

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
        const float extent = 430.F * scale;
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

        // Unequal, non-periodic aperture/scatter lobes. These are continuous
        // optical intensity profiles, not line primitives. The long axes end at
        // different distances and widen softly; some develop a violet fringe.
        inline constexpr std::array<GlareLobe, 24> GlareLobes{{
            {.10F,.014F,.96F,.78F}, {3.25F,.011F,.84F,.66F},
            {1.47F,.009F,.78F,.69F}, {4.67F,.013F,.94F,.81F},
            {.72F,.013F,.68F,.58F}, {3.86F,.009F,.80F,.60F},
            {2.24F,.017F,.91F,.68F}, {5.39F,.008F,.62F,.56F},
            {1.02F,.018F,.46F,.37F}, {4.12F,.020F,.55F,.42F},
            {2.74F,.011F,.60F,.46F}, {5.88F,.019F,.78F,.56F},
            {.32F,.025F,.34F,.22F}, {3.50F,.023F,.30F,.27F},
            {1.72F,.018F,.51F,.28F}, {4.42F,.023F,.40F,.23F},
            {.91F,.008F,.29F,.28F}, {3.65F,.027F,.26F,.19F},
            {2.48F,.024F,.37F,.25F}, {5.62F,.017F,.43F,.23F},
            {1.26F,.014F,.32F,.25F}, {4.93F,.022F,.29F,.26F},
            {2.97F,.021F,.25F,.21F}, {6.12F,.017F,.39F,.25F}
        }};

        inline float smoothC2(float t)
        {
            t = std::clamp(t, 0.F, 1.F);
            return t*t*t*(10.F+t*(-15.F+6.F*t));
        }

        inline float glareLobe(float x, float y, const GlareLobe& lobe)
        {
            const float c=std::cos(lobe.angle), s=std::sin(lobe.angle);
            const float along=x*c+y*s;
            if (along <= 0 || along >= lobe.length) return 0;
            const float q=along/lobe.length;
            const float width=.0018F+lobe.width*(.22F+q*.9F);
            const float lateral=(-x*s+y*c)/width;
            return lobe.strength * std::exp(-.5F*lateral*lateral) *
                   std::exp(-2.4F*q) * (1.F-smoothC2((q-.48F)/.52F));
        }
    } // namespace CelestialSunOptics

    // Optical glare is a camera-response approximation, not a full physical
    // light-transport simulation. Crucially ALL energy reaches zero smoothly
    // before the finite texture boundary, not just the rays. The previous code
    // cut nonzero radial corona energy at radius=1, exposing a pasted disk.
    inline RenderColor celestialSunCoronaSample(float x, float y)
    {
        using namespace CelestialSunOptics;
        const float radius=std::hypot(x,y);
        if (!std::isfinite(radius) || radius >= 1) return {0,0,0,0};
        const float halo = .80F*std::exp(-std::pow(radius/.045F,2.F)) +
                           .58F*std::exp(-std::pow(radius/.135F,1.45F)) +
                           .13F*std::exp(-std::pow(radius/.30F,1.7F)) +
                           .010F*std::exp(-std::pow(radius/.52F,2.F));
        const float hot=std::exp(-radius*13.F);
        float red=halo, green=halo*(.92F+.08F*hot), blue=halo*(.79F+.21F*hot);
        const float emergence=smoothC2(radius/.020F);
        for (std::size_t i=0;i<GlareLobes.size();++i)
        {
            const auto& lobe=GlareLobes[i];
            const float along=x*std::cos(lobe.angle)+y*std::sin(lobe.angle);
            const float end=(i%3 != 1) ? smoothC2((along/lobe.length-.20F)/.50F) : 0.F;
            const float energy=glareLobe(x,y,lobe)*emergence;
            red+=energy*(.96F+.04F*hot);
            green+=energy*(.97F-.44F*end);
            blue+=energy*(.93F+.07F*end);
            // A restrained displaced blue/violet wing gives selected ray ends
            // color dispersion without tinting the entire sun purple.
            auto fringe=lobe;
            fringe.angle+=.008F;
            fringe.width*=1.25F;
            const float chroma=.22F*end*glareLobe(x,y,fringe)*emergence;
            red+=chroma*.65F; green+=chroma*.23F; blue+=chroma;
        }
        const float envelope=1.F-smoothC2((radius-.65F)/.35F);
        red*=envelope; green*=envelope; blue*=envelope;
        const float energy=std::max({red,green,blue});
        if (energy <= 0) return {0,0,0,0};
        // Encode additive intensity without throwing away chromatic ratios.
        // Black, zero-alpha guard texels prevent filtering a visible disk edge.
        const auto alpha=channel(255.F*std::min(1.F,energy));
        if (!alpha) return {0,0,0,0};
        return {channel(255.F*red/energy),channel(255.F*green/energy),
                channel(255.F*blue/energy),alpha};
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
        std::uint64_t textureBuilds() const noexcept { return textureBuilds_; }

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

            // Glare requires a visible light source. A fully occulted source
            // must not leave the outer half of its halo hanging around a planet.
            opacity *= sourceVisibility(*projected, globeX, globeY, globeRadius);
            if (opacity <= .001F) return;
            drawOccludedLayer(
                renderer,
                *coronaTexture_,
                projected->x,
                projected->y,
                410.F * projected->scale,
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

            // Integer physical scan rows avoid fractional-strip gaps in SDL's
            // software triangle path. Offscreen regions never enter the mesh.
            const int firstRow=std::max(0,int(std::ceil(top)));
            const int lastRow=std::min(renderer.outputHeight(),int(std::floor(top+diameter)));
            if (lastRow<=firstRow) return;
            const int rowStep=std::max(1,(lastRow-firstRow+int(MaxStrips)-1)/int(MaxStrips));
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

            // Unoccluded glare is one smooth quad, not hundreds of strips.
            const float nearestX=std::clamp(globeX,left,right);
            const float nearestY=std::clamp(globeY,top,top+diameter);
            const bool separate=std::hypot(nearestX-globeX,nearestY-globeY)>=globeRadius;
            if (separate)
            {
                addQuad(left,right,top,top+diameter,0,1,0,1);
            }
            for (int row=firstRow; !separate && row<lastRow; row+=rowStep)
            {
                const float y0=float(row), y1=float(std::min(row+rowStep,lastRow));
                const float v0=(y0-top)/diameter, v1=(y1-top)/diameter;
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
            ++textureBuilds_;
            return true;
        }

        mutable std::uint64_t textureBuilds_ = 0;
        mutable std::unique_ptr<Texture> coronaTexture_;
        mutable std::unique_ptr<Texture> coreTexture_;
        mutable std::unique_ptr<Texture> lensGhostTexture_;
        mutable const Renderer* textureOwner_ = nullptr;
    };
} // namespace Paladin

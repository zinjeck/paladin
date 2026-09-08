#pragma once
#include "rendering/ScenePresentation.h"
#include "rendering/TerrainMaterialField.h"
#include <array>
namespace Paladin
{
    // Weather is a translucent lighting layer, separate from authored sprites.
    class CityClouds
    {
        mutable std::unique_ptr<Texture> body_, shadow_;

    public:
        void render(
            Renderer& r,
            const SceneProjection& p,
            double seconds,
            double day,
            bool sky,
            int mapWidth,
            int mapHeight
        ) const
        {
            const double visibility =
                sky ? std::clamp((10. - p.tilePixels) / 5., 0., 1.) : 1.;
            if (visibility <= 0)
            {
                return;
            }
            if (!body_)
            {
                constexpr int w = 64, h = 32;
                std::vector<RenderColor> body(w * h), shadow(w * h);
                for (int y = 0; y < h; ++y)
                {
                    for (int x = 0; x < w; ++x)
                    {
                        // Stepped wind-shaped clusters, sampled on a coarse
                        // art grid instead of smooth ellipse lobes.
                        const double nx = x / double(w), ny = y / double(h);
                        const double center =
                            .5 + .12 * (landscapeField(x * .13, 0, 361) - .5);
                        const double taper =
                            std::clamp(std::min(nx, 1 - nx) * 5, 0., 1.);
                        const double halfHeight =
                            (.14 + .20 * landscapeField(x * .18, 0, 731)) *
                            taper;
                        const double coverField =
                            landscapeField(x * .16, y * .23, 922) * .65 +
                            landscapeField(x * .4, y * .4, 179) * .35;
                        const double edge = halfHeight - std::abs(ny - center);
                        const double cover =
                            edge <= .02 || coverField < .34
                                ? 0
                                : (edge > .10 && coverField > .65 ? 1. : .65);
                        auto alpha = std::uint8_t(255 * cover);
                        body[y * w + x] = {215, 224, 227, alpha};
                        shadow[y * w + x] = {25, 62, 66, alpha};
                    }
                }
                body_ = r.createTextureFromPixels(w, h, body);
                shadow_ = r.createTextureFromPixels(w, h, shadow);
            }
            if (!body_ || !shadow_)
            {
                return;
            }
            const double driftX =
                             std::fmod(seconds * .35, double(mapWidth) + 240),
                         driftY =
                             std::fmod(seconds * .07, double(mapHeight) + 170);
            const double halfW = p.screenWidth * .5 / p.tilePixels,
                         halfH = p.screenHeight * .5 / p.tilePixels;
            int x0 = int(std::floor((p.cameraX - halfW - driftX - 110) / 120)),
                x1 = int(std::ceil((p.cameraX + halfW - driftX + 110) / 120));
            int y0 = int(std::floor((p.cameraY - halfH - driftY - 60) / 85)),
                y1 = int(std::ceil((p.cameraY + halfH - driftY + 60) / 85));
            x0 = std::max(x0, int(std::floor((-driftX - 130) / 120)));
            x1 = std::min(x1, int(std::ceil((mapWidth - driftX + 130) / 120)));
            y0 = std::max(y0, int(std::floor((-driftY - 90) / 85)));
            y1 = std::min(y1, int(std::ceil((mapHeight - driftY + 90) / 85)));
            for (int y = y0; y <= y1; ++y)
            {
                for (int x = x0; x <= x1; ++x)
                {
                    auto hash = landscapeHash(x, y, 721);
                    if (hash % 5 == 0)
                    {
                        continue;
                    }
                    const double width = 65 + (hash % 40),
                                 height =
                                     width * (.40 + ((hash >> 8) % 12) / 100.);
                    const double cx = x * 120 + ((hash >> 12) % 50) + driftX,
                                 cy = y * 85 + ((hash >> 18) % 35) + driftY;
                    const auto b = p.bounds(
                        {cx + (sky ? 0 : 22),
                         cy + (sky ? 0 : 14),
                         0,
                         width,
                         height,
                         .5,
                         .5}
                    );
                    if (!p.visible(b))
                    {
                        continue;
                    }
                    const auto alpha = std::uint8_t(
                        (sky ? 24 : 24) * visibility *
                        (sky ? (.3 + .7 * day) : (.4 + .6 * day))
                    );
                    const float mx = float(
                                    p.screenWidth * .5 -
                                    p.cameraX * p.tilePixels
                                ),
                                my = float(
                                    p.screenHeight * .5 -
                                    p.cameraY * p.tilePixels
                                );
                    const float x0 = std::max({0.F, b.x, mx}),
                                y0 = std::max({0.F, b.y, my});
                    const float x1 = std::min(
                                    {float(p.screenWidth),
                                     b.x + b.width,
                                     mx + float(mapWidth * p.tilePixels)}
                                ),
                                y1 = std::min(
                                    {float(p.screenHeight),
                                     b.y + b.height,
                                     my + float(mapHeight * p.tilePixels)}
                                );
                    if (x1 <= x0 || y1 <= y0)
                    {
                        continue;
                    }
                    r.drawTexture(
                        sky ? *body_ : *shadow_,
                        (x0 - b.x) / b.width * 64,
                        (y0 - b.y) / b.height * 32,
                        (x1 - x0) / b.width * 64,
                        (y1 - y0) / b.height * 32,
                        x0,
                        y0,
                        x1 - x0,
                        y1 - y0,
                        alpha
                    );
                }
            }
        }
    };
} // namespace Paladin

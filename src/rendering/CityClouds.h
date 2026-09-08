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
            bool sky
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
                constexpr int w = 128, h = 64;
                std::vector<RenderColor> body(w * h), shadow(w * h);
                for (int y = 0; y < h; ++y)
                {
                    for (int x = 0; x < w; ++x)
                    {
                        double cover = 0;
                        for (auto lobe : std::array<std::array<double, 4>, 5>{
                                 {{{.25, .57, .22, .26}},
                                  {{.43, .40, .24, .33}},
                                  {{.62, .53, .28, .30}},
                                  {{.78, .58, .17, .23}},
                                  {{.48, .64, .30, .23}}}
                             })
                        {
                            double dx = (x / double(w) - lobe[0]) / lobe[2],
                                   dy = (y / double(h) - lobe[1]) / lobe[3];
                            cover = std::max(
                                cover,
                                std::clamp(
                                    (1 - dx * dx - dy * dy) * 1.5,
                                    0.,
                                    1.
                                )
                            );
                        }
                        cover = cover * cover * (3 - 2 * cover);
                        cover *=
                            .80 + .20 * landscapeField(x * .12, y * .12, 922);
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
            const double driftX = seconds * .35, driftY = seconds * .07;
            const double halfW = p.screenWidth * .5 / p.tilePixels,
                         halfH = p.screenHeight * .5 / p.tilePixels;
            int x0 = int(std::floor((p.cameraX - halfW - driftX - 110) / 120)),
                x1 = int(std::ceil((p.cameraX + halfW - driftX + 110) / 120));
            int y0 = int(std::floor((p.cameraY - halfH - driftY - 60) / 85)),
                y1 = int(std::ceil((p.cameraY + halfH - driftY + 60) / 85));
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
                        {cx + (sky ? 0 : 5),
                         cy + (sky ? 0 : 3),
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
                    r.drawTexture(
                        sky ? *body_ : *shadow_,
                        0,
                        0,
                        128,
                        64,
                        b.x,
                        b.y,
                        b.width,
                        b.height,
                        alpha
                    );
                }
            }
        }
    };
} // namespace Paladin

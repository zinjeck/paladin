#pragma once
#include "rendering/NaturalSurfaceShape.h"
#include "rendering/SceneDetail.h"
#include "rendering/SceneSpriteLibrary.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include "world/settlements/objects/jobs/LoggingGroundsJob.h"

namespace Paladin
{
    inline void groundPatch(
        SceneDrawQueue& q,
        const SceneProjection& p,
        const SceneSpriteLibrary& sprites,
        const SceneSprite& art,
        double x,
        double y,
        double w,
        double h,
        std::uint64_t id,
        int alpha,
        int layer = -3
    )
    {
        const auto b = p.bounds({x, y, 0, w, h, 0, 0});
        if (!p.visible(b) || alpha <= 0)
        {
            return;
        }
        const auto f = sprites.frame(art, false);
        const double u = (x - std::floor(x)), v = (y - std::floor(y));
        q.submit(
            {b,
             {},
             y,
             id,
             layer,
             0,
             art.texture.get(),
             {f.x + float(u * f.width),
              f.y + float(v * f.height),
              float(std::min(w, 1 - u) * f.width),
              float(std::min(h, 1 - v) * f.height)},
             std::uint8_t(alpha)}
        );
    }
    bool naturalRoad(
        SceneDrawQueue& q,
        const SceneProjection& p,
        const SettlementMap& map,
        const SceneSpriteLibrary& sprites,
        const CompletedSettlementObject& object,
        std::uint64_t id
    );
    void buildingGround(
        SceneDrawQueue& q,
        const SceneProjection& p,
        const SceneSpriteLibrary& sprites,
        const SettlementMap& map,
        const SettlementObjectFootprint& f,
        std::uint64_t id
    );
    inline void homeChimney(
        SceneDrawQueue& q,
        const SceneProjection& p,
        const SceneSpriteLibrary& sprites,
        const SettlementMap& map,
        const CompletedSettlementObject& object,
        std::uint64_t id
    )
    {
        if (p.tilePixels < AnimationDetailPixels || !sprites.find("house.wall"))
        {
            return;
        }
        const auto& f = object.footprint;
        const double x = f.topLeft.x + f.width * .73, y = f.topLeft.y + .72;
        const double depth = f.topLeft.y + f.height;
        const auto block = [&](double xx,
                               double yy,
                               double w,
                               double h,
                               RenderColor c,
                               int part)
        {
            q.submit(
                {p.bounds({xx, yy, 0, w, h, 0, 0}), c, depth, id, 0, part}
            );
        };
        sprites.surface(
            q,
            p,
            "house.wall",
            {x, y - .58, 0, .3, .58, 0, 0},
            {},
            depth,
            id,
            30,
            false
        );
        block(x + .23, y - .58, .07, .58, {135, 77, 80, 255}, 31);
        block(x - .04, y - .63, .38, .12, {169, 148, 120, 255}, 32);
        block(x + .03, y - .61, .24, .06, {57, 43, 60, 255}, 33);
        if (!map.heating.heated(object.id))
        {
            return;
        }
        for (int i = 0; i < 5; ++i)
        {
            const double phase = std::fmod(
                sprites.time() * .23 + i * .2 + double(id % 17) * .01,
                1.
            );
            const double rise = std::floor(phase * 22) / 16.;
            const double drift =
                std::round(
                    (phase * .32 +
                     .06 * std::sin(phase * 8 + sprites.time() * .7)) *
                    16
                ) /
                16.;
            const double size = (2 + int(phase * 3)) / 16.;
            const auto alpha = std::uint8_t(95 * (1 - phase));
            block(
                x + .1 + drift,
                y - .68 - rise,
                size,
                size,
                {173, 160, 185, alpha},
                34 + i
            );
            block(
                x + .1 + drift - .0625,
                y - .68 - rise + .0625,
                size + .125,
                std::max(.0625, size - .125),
                {173, 160, 185, alpha},
                34 + i
            );
        }
    }
    inline void loggingDetails(
        SceneDrawQueue& q,
        const SceneProjection& p,
        const SceneSpriteLibrary& sprites,
        const CompletedSettlementObject& object,
        const SettlementCitizenState* citizens,
        std::uint64_t id,
        bool shadows
    )
    {
        const auto& f = object.footprint;
        std::unordered_set<std::uint64_t> occupied;
        if (p.tilePixels < StaticDetailPixels)
        {
            sprites.submit(
                q,
                p,
                "tree",
                f.topLeft.x + f.width * .5,
                f.topLeft.y + f.height * .5,
                id
            );
            return;
        }
        if (citizens && p.tilePixels >= AnimationDetailPixels)
        {
            for (const auto& c : citizens->citizens())
            {
                if (c.task.object != object.id ||
                    c.task.kind != CitizenTaskKind::Work || !c.path.empty() ||
                    c.health <= 0)
                {
                    continue;
                }
                occupied.insert(
                    (std::uint64_t(c.task.workTile.y) << 32) |
                    std::uint32_t(c.task.workTile.x)
                );
                const double swing =
                    std::round(
                        std::sin(sprites.time() * 7 + c.id.value()) * 3
                    ) /
                    16.;
                const double cx = c.visualX() + .72, cy = c.visualY() + .35;
                q.submit(
                    {p.bounds({cx, cy + swing, 0, .0625, .32, 0, 1}),
                     {122, 80, 56, 255},
                     c.visualY() + .51,
                     id,
                     0,
                     40}
                );
                q.submit(
                    {p.bounds(
                         {cx - .0625, cy + swing - .3, 0, .19, .125, 0, 0}
                     ),
                     {154, 167, 175, 255},
                     c.visualY() + .51,
                     id,
                     0,
                     41}
                );
            }
        }
        int count = 0;
        const double halfW = p.screenWidth * .5 / p.tilePixels,
                     halfH = p.screenHeight * .5 / p.tilePixels;
        const int firstRow = std::max(
            0,
            int(std::floor((p.cameraY - halfH - 3 - f.topLeft.y) / 3))
        );
        const int lastRow = std::min(
            (f.height + 2) / 3,
            int(std::ceil((p.cameraY + halfH + 3 - f.topLeft.y) / 3))
        );
        const int firstCol = std::max(
            0,
            int(std::floor((p.cameraX - halfW - 2 - f.topLeft.x) / 3))
        );
        const int lastCol = std::min(
            (f.width + 2) / 3,
            int(std::ceil((p.cameraX + halfW + 2 - f.topLeft.x) / 3))
        );
        for (int row = firstRow; row < lastRow; ++row)
        {
            for (int col = firstCol; col < lastCol && count < 2048; ++col)
            {
                const int tx = f.topLeft.x + std::min(f.width - 1, col * 3 + 1),
                          ty =
                              f.topLeft.y + std::min(f.height - 1, row * 3 + 2);
                const double x = tx + .5, y = ty - .15;
                if (!p.visible(p.bounds({x, y, 0, 2, 2, .5, 1})))
                {
                    continue;
                }
                ++count;
                const auto seed = id ^ std::uint64_t(tx * 73856093u) ^
                                  std::uint64_t(ty * 19349663u);
                const bool working = occupied.contains(
                    (std::uint64_t(ty) << 32) | std::uint32_t(tx)
                );
                const double growth =
                    loggingTreeGrowth(sprites.time(), seed, working);
                if (shadows)
                {
                    for (int band = 0; band < 5; ++band)
                    {
                        const double width = (band == 0 || band == 4 ? .45
                                              : band == 2            ? 1.
                                                                     : .8) *
                                             .8 * std::max(.3, growth);
                        q.submit(
                            {p.bounds(
                                 {x + .08,
                                  y - .1 + band * .04,
                                  0,
                                  width,
                                  .04,
                                  .5,
                                  0}
                             ),
                             {32,
                              44,
                              67,
                              std::uint8_t(band == 0 || band == 4 ? 30 : 55)},
                             y,
                             id,
                             -1}
                        );
                    }
                }
                if (growth <= .13)
                {
                    if (const auto* stump = sprites.find("tree.trunk.1"))
                    {
                        const auto frame = sprites.frame(*stump);
                        const float rows = std::min(5.F, frame.height);
                        q.submit(
                            {p.bounds(
                                 {x,
                                  y,
                                  0,
                                  .44,
                                  .56 * rows / frame.height,
                                  .5,
                                  1}
                             ),
                             {},
                             y,
                             seed,
                             0,
                             0,
                             stump->texture.get(),
                             {frame.x,
                              frame.y + frame.height - rows,
                              frame.width,
                              rows}}
                        );
                    }
                }
                else
                {
                    sprites.submitTree(q, p, x, y, seed, .8 * growth);
                }
            }
        }
        sprites.submit(
            q,
            p,
            "logging_grounds.stack",
            f.topLeft.x + .5,
            f.topLeft.y + f.height - .25,
            id
        );
    }
} // namespace Paladin

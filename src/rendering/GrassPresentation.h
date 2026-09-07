#pragma once
#include "rendering/SceneDetail.h"
#include "rendering/SceneSpriteLibrary.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include <cmath>
#include <unordered_map>

namespace Paladin
{
    class GrassPresentation
    {
        mutable std::vector<unsigned char> blocked_;
        mutable std::uint64_t instance_ = 0, version_ = ~std::uint64_t(0);
        mutable int left_ = 0, top_ = 0, right_ = 0, bottom_ = 0;
        struct Reaction
        {
            double amount = 0, direction = 1;
            std::uint64_t seen = 0;
        };
        mutable std::unordered_map<std::uint32_t, Reaction> reactions_;
        mutable double lastTime_ = 0;
        mutable std::uint64_t frame_ = 0;

    public:
        void submit(
            SceneDrawQueue& queue,
            const SceneProjection& p,
            const SettlementMap& map,
            const SceneSpriteLibrary& sprites,
            const SettlementCitizenState* citizens = nullptr,
            double interpolation = 1
        ) const
        {
            if (p.tilePixels < 20 || !sprites.find("grass.tuft"))
            {
                return;
            }
            const auto& grid = map.grid();
            const double now = sprites.time();
            ++frame_;
            if (instance_ != map.instanceId())
            {
                reactions_.clear();
                lastTime_ = now;
            }
            const double dt = std::clamp(now - lastTime_, 0., .1);
            lastTime_ = now;
            const auto cell = [](int x, int y)
            {
                return (std::uint64_t(std::uint32_t(x)) << 32) |
                       std::uint32_t(y);
            };
            std::unordered_map<
                std::uint64_t,
                std::vector<std::pair<double, double>>>
                walkers;
            if (citizens)
            {
                for (const auto& c : citizens->citizens())
                {
                    if (c.insideHome || c.pathIndex >= c.path.size())
                    {
                        continue;
                    }
                    const double x = c.renderX(c.visualX(), interpolation) + .5,
                                 y = c.renderY(c.visualY(), interpolation) + .5;
                    if (std::abs(x - p.cameraX) >
                            p.screenWidth * .5 / p.tilePixels + 2 ||
                        std::abs(y - p.cameraY) >
                            p.screenHeight * .5 / p.tilePixels + 2)
                    {
                        continue;
                    }
                    walkers[cell(int(std::floor(x)), int(std::floor(y)))]
                        .push_back({x, y});
                }
            }
            std::erase_if(
                reactions_,
                [&](const auto& entry)
                { return frame_ - entry.second.seen > 2; }
            );
            const int x0 = std::max(
                0,
                int(std::floor(
                    p.cameraX - p.screenWidth * .5 / p.tilePixels - 2
                )) / 2 *
                    2
            );
            const int y0 = std::max(
                0,
                int(std::floor(
                    p.cameraY - p.screenHeight * .5 / p.tilePixels - 2
                )) / 2 *
                    2
            );
            const int x1 = std::min(
                grid.width(),
                int(
                    std::ceil(p.cameraX + p.screenWidth * .5 / p.tilePixels + 2)
                )
            );
            const int y1 = std::min(
                grid.height(),
                int(std::ceil(
                    p.cameraY + p.screenHeight * .5 / p.tilePixels + 2
                ))
            );
            const int width = x1 - x0, height = y1 - y0;
            if (width <= 0 || height <= 0)
            {
                return;
            }
            if (instance_ != map.instanceId() ||
                version_ != map.objectState().navigationVersion() ||
                left_ != x0 || top_ != y0 || right_ != x1 || bottom_ != y1)
            {
                instance_ = map.instanceId();
                version_ = map.objectState().navigationVersion();
                left_ = x0;
                top_ = y0;
                right_ = x1;
                bottom_ = y1;
                blocked_.assign(std::size_t(width) * height, 0);
                const auto mark = [&](const auto& f)
                {
                    for (int by = std::max(y0, f.topLeft.y);
                         by < std::min(y1, f.topLeft.y + f.height);
                         ++by)
                    {
                        for (int bx = std::max(x0, f.topLeft.x);
                             bx < std::min(x1, f.topLeft.x + f.width);
                             ++bx)
                        {
                            blocked_[std::size_t(by - y0) * width + bx - x0] =
                                1;
                        }
                    }
                };
                for (const auto& o : map.objectState().completedObjects())
                {
                    mark(o.footprint);
                }
                for (const auto& o : map.objectState().constructionSites())
                {
                    mark(o.footprint);
                }
            }
            int submitted = 0;
            for (int y = y0; y < y1 && submitted < 1500; y += 2)
            {
                for (int x = x0; x < x1 && submitted < 1500; x += 2)
                {
                    std::uint32_t hash = std::uint32_t(x) * 0x9e3779b9u ^
                                         std::uint32_t(y) * 0x85ebca6bu;
                    hash ^= hash >> 16;
                    hash *= 0x7feb352du;
                    hash ^= hash >> 15;
                    if (hash % 3 != 0)
                    {
                        continue;
                    }
                    const SettlementTilePosition at{
                        x + int((hash >> 8) & 1),
                        y + int((hash >> 9) & 1)
                    };
                    const auto* tile = grid.tile(at);
                    if (!tile || tile->terrain != TerrainType::Land ||
                        (tile->biome != BiomeType::Plain &&
                         tile->biome != BiomeType::Forest &&
                         tile->biome != BiomeType::Jungle))
                    {
                        continue;
                    }
                    if (at.x >= x1 || at.y >= y1 ||
                        blocked_[std::size_t(at.y - y0) * width + at.x - x0])
                    {
                        continue;
                    }
                    const double gx = at.x + .3 + ((hash >> 12) & 15) / 32.,
                                 gy = at.y + .65 + ((hash >> 16) & 7) / 32.;
                    double contact = 0, direction = 1;
                    for (int yy = int(std::floor(gy)) - 1;
                         yy <= int(std::floor(gy)) + 1;
                         ++yy)
                    {
                        for (int xx = int(std::floor(gx)) - 1;
                             xx <= int(std::floor(gx)) + 1;
                             ++xx)
                        {
                            if (const auto it = walkers.find(cell(xx, yy));
                                it != walkers.end())
                            {
                                for (const auto [wx, wy] : it->second)
                                {
                                    const double d =
                                        std::hypot(wx - gx, wy - gy);
                                    if (d < .8 && 1 - d / .8 > contact)
                                    {
                                        contact = 1 - d / .8;
                                        direction = gx >= wx ? 1 : -1;
                                    }
                                }
                            }
                        }
                    }
                    auto& reaction = reactions_[hash];
                    reaction.seen = frame_;
                    if (dt > 0)
                    {
                        reaction.amount +=
                            (contact - reaction.amount) *
                            (1 - std::exp(
                                     -dt * (contact > reaction.amount ? 18 : 5)
                                 ));
                        if (contact > 0)
                        {
                            reaction.direction = direction;
                        }
                    }
                    const auto begin = queue.size();
                    sprites.submit(
                        queue,
                        p,
                        "grass.tuft",
                        at.x + .3 + ((hash >> 12) & 15) / 32.,
                        at.y + .65 + ((hash >> 16) & 7) / 32.,
                        (std::uint64_t(hash) << 3) | 6
                    );
                    queue.bendFrom(
                        begin,
                        reaction.direction * reaction.amount * .14 *
                            p.tilePixels,
                        1 - reaction.amount * .45,
                        p.screenHeight * .5 + (gy - p.cameraY) * p.tilePixels
                    );
                    ++submitted;
                }
            }
        }
    };
} // namespace Paladin

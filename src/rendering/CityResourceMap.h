#pragma once

#include "rendering/CityMineralOverview.h"
#include "rendering/SceneSpriteLibrary.h"
#include "world/settlements/SettlementMap.h"
#include <map>
#include <tuple>

namespace Paladin
{
    // Resource tiles stay tile highlights; only herds become single symbols.
    // Small reusable pages bound uploads and cache memory while panning.
    class CityResourceMap
    {
        struct Page
        {
            std::unique_ptr<Texture> texture;
            std::uint64_t revision = ~std::uint64_t{}, touched = 0;
            std::uint64_t naturalVersion = ~std::uint64_t{},
                          naturalRevision = 0;
            std::size_t mineralCursor = 0;
        };
        std::map<std::tuple<int, int, int>, Page> pages_;
        std::uint64_t instance_ = 0, frame_ = 0;
        CityMineralOverview minerals_;
        static RenderColor colorAt(const SettlementMap& map, int x, int y)
        {
            const auto* tile = map.grid().tile({x, y});
            if (!tile || tile->terrain == TerrainType::Water)
            {
                return {0, 0, 0, 0};
            }
            switch (tile->mineral != MineralDeposit::None &&
                            map.mining.remainingAt({x, y}, tile->mineral) > 0
                        ? tile->mineral
                        : MineralDeposit::None)
            {
            case MineralDeposit::Coal:
                return {116, 129, 144, 215};
            case MineralDeposit::Iron:
                return {198, 113, 73, 220};
            case MineralDeposit::Gold:
                return {235, 196, 107, 240};
            default:
                break;
            }
            switch (map.naturalFeatures().at({x, y}).kind)
            {
            case NaturalFeatureKind::Tree:
                return {79, 140, 122, 170};
            case NaturalFeatureKind::Rock:
                return {189, 194, 199, 210};
            case NaturalFeatureKind::Wheat:
                return {235, 196, 107, 220};
            default:
                return {0, 0, 0, 0};
            }
        }

    public:
        void reset()
        {
            pages_.clear();
            minerals_.reset();
            instance_ = 0;
        }
        void render(
            Renderer& renderer,
            const SettlementMap& map,
            const SceneProjection& projection,
            const SceneSpriteLibrary& art
        )
        {
            if (instance_ != map.instanceId())
            {
                reset();
                instance_ = map.instanceId();
            }
            minerals_.begin(map);
            ++frame_;
            renderer.fillRectangle(
                0,
                0,
                float(renderer.outputWidth()),
                float(renderer.outputHeight()),
                {8, 15, 27, 188}
            );
            const double pixels = projection.tilePixels;
            if (pixels <= 0)
            {
                return;
            }
            int stride = 1;
            while ((renderer.outputWidth() / (32. * stride * pixels) + 2) *
                       (renderer.outputHeight() / (32. * stride * pixels) + 2) >
                   80)
            {
                stride *= 2;
            }
            const int span = 32 * stride;
            const int left = std::max(
                0,
                int(std::floor(
                    (projection.cameraX -
                     projection.screenWidth / pixels * .5) /
                    span
                ))
            );
            const int top = std::max(
                0,
                int(std::floor(
                    (projection.cameraY -
                     projection.screenHeight / pixels * .5) /
                    span
                ))
            );
            const int right = std::min(
                (map.grid().width() + span - 1) / span,
                int(std::ceil(
                    (projection.cameraX +
                     projection.screenWidth / pixels * .5) /
                    span
                ))
            );
            const int bottom = std::min(
                (map.grid().height() + span - 1) / span,
                int(std::ceil(
                    (projection.cameraY +
                     projection.screenHeight / pixels * .5) /
                    span
                ))
            );
            int uploads = 4;
            for (int py = top; py < bottom; ++py)
            {
                for (int px = left; px < right; ++px)
                {
                    const auto key = std::tuple{px, py, stride};
                    auto it = pages_.find(key);
                    if (it == pages_.end() && uploads > 0)
                    {
                        if (pages_.size() >= 128)
                        {
                            const auto oldest = std::min_element(
                                pages_.begin(),
                                pages_.end(),
                                [](const auto& a, const auto& b)
                                { return a.second.touched < b.second.touched; }
                            );
                            pages_.erase(oldest);
                        }
                        it = pages_.try_emplace(key).first;
                    }
                    if (it == pages_.end())
                    {
                        continue;
                    }
                    auto& page = it->second;
                    page.touched = frame_;
                    const int firstX = px * stride, firstY = py * stride;
                    const int columns = std::min(
                        stride,
                        (map.grid().width() + 31) / 32 - firstX
                    );
                    const int rows = std::min(
                        stride,
                        (map.grid().height() + 31) / 32 - firstY
                    );
                    // Prioritize visible ore chunks without a map-wide audit
                    // at distant zoom. The overview enforces one shared tile
                    // budget across these requests and its background cursor.
                    for (int i = 0; i < 4 && columns > 0 && rows > 0; ++i)
                    {
                        const auto cell = page.mineralCursor++ %
                                          (std::size_t(columns) * rows);
                        static_cast<void>(minerals_.touch(
                            map,
                            firstX + int(cell % columns),
                            firstY + int(cell / columns)
                        ));
                    }
                    if (page.naturalVersion != map.naturalFeatures().version())
                    {
                        page.naturalVersion = map.naturalFeatures().version();
                        page.naturalRevision = 0;
                        for (int y = 0; y < rows; ++y)
                        {
                            for (int x = 0; x < columns; ++x)
                            {
                                page.naturalRevision =
                                    page.naturalRevision * 1099511628211ULL +
                                    map.naturalFeatures().chunkVersion(
                                        firstX + x,
                                        firstY + y
                                    );
                            }
                        }
                    }
                    const auto revision =
                        page.naturalRevision +
                        std::uint64_t(
                            minerals_.revisionAt(px * span, py * span, span)
                        ) * 2654435761ULL;
                    if (page.revision != revision && uploads > 0)
                    {
                        --uploads;
                        std::array<RenderColor, 1024> colors{};
                        for (int y = 0; y < 32; ++y)
                        {
                            for (int x = 0; x < 32; ++x)
                            {
                                RenderColor color{0, 0, 0, 0};
                                // Distant pages sample at most sixteen points
                                // per pixel, keeping zoom-out work bounded.
                                const int step = std::max(1, stride / 4);
                                for (int sy = 0; sy < stride; sy += step)
                                {
                                    for (int sx = 0; sx < stride; sx += step)
                                    {
                                        const auto sample = colorAt(
                                            map,
                                            px * span + x * stride + sx,
                                            py * span + y * stride + sy
                                        );
                                        if (sample.alpha > color.alpha)
                                        {
                                            color = sample;
                                        }
                                    }
                                }
                                if (stride >= 4)
                                {
                                    const auto mask = minerals_.maskAt(
                                        px * span + x * stride,
                                        py * span + y * stride,
                                        stride
                                    );
                                    // Mineral presence is fully aggregated,
                                    // never sampled away. Rare gold takes
                                    // precedence only when deposits share one
                                    // subpixel overview cell.
                                    if (mask & 4)
                                    {
                                        color = {235, 196, 107, 240};
                                    }
                                    else if (mask & 2)
                                    {
                                        color = {198, 113, 73, 220};
                                    }
                                    else if (mask & 1)
                                    {
                                        color = {116, 129, 144, 215};
                                    }
                                }
                                colors[std::size_t(y) * 32 + x] = color;
                            }
                        }
                        if (!page.texture)
                        {
                            page.texture = renderer.createTextureFromPixels(
                                32,
                                32,
                                colors
                            );
                            if (page.texture)
                            {
                                renderer.setTextureFiltering(
                                    *page.texture,
                                    false
                                );
                            }
                        }
                        else
                        {
                            static_cast<void>(renderer.updateTexturePixels(
                                *page.texture,
                                colors
                            ));
                        }
                        page.revision = revision;
                    }
                    if (!page.texture)
                    {
                        continue;
                    }
                    const auto b = projection.bounds(
                        {double(px * span),
                         double(py * span),
                         0,
                         double(span),
                         double(span),
                         0,
                         0}
                    );
                    renderer.drawTexture(
                        *page.texture,
                        0,
                        0,
                        32,
                        32,
                        b.x,
                        b.y,
                        b.width,
                        b.height
                    );
                }
            }
            minerals_.advance(map);
            struct Herd
            {
                double x = 0, y = 0;
                int count = 0;
            };
            std::map<std::tuple<int, int, std::string_view>, Herd> herds;
            for (const auto& animal : map.animals.all())
            {
                if (animal.health <= 0 || animal.pasture)
                {
                    continue;
                }
                auto& herd = herds[{
                    animal.herdCenter.x,
                    animal.herdCenter.y,
                    animal.species
                }];
                herd.x += animal.visualX();
                herd.y += animal.visualY();
                ++herd.count;
            }
            for (const auto& [key, herd] : herds)
            {
                const auto* sprite =
                    art.find("animal." + std::string(std::get<2>(key)));
                if (!sprite || !sprite->texture)
                {
                    continue;
                }
                const auto frame = art.frame(*sprite, false);
                const float scale =
                    float(std::max(1.0, renderer.currentPixelPitch()));
                const auto center = projection.bounds(
                    {herd.x / herd.count + .5,
                     herd.y / herd.count + .5,
                     0,
                     0,
                     0,
                     0,
                     0}
                );
                const RenderRectangle bounds{
                    center.x - frame.width * scale * .5F,
                    center.y - frame.height * scale * .5F,
                    frame.width * scale,
                    frame.height * scale
                };
                if (!projection.visible(bounds))
                {
                    continue;
                }
                renderer.drawTexture(
                    *sprite->texture,
                    frame.x,
                    frame.y,
                    frame.width,
                    frame.height,
                    bounds.x,
                    bounds.y,
                    bounds.width,
                    bounds.height
                );
            }
        }
    };
} // namespace Paladin

#pragma once
#include "rendering/GlobeLighting.h"
#include "rendering/GlobeView.h"
#include "rendering/NaturalSurfaceShape.h"
#include "rendering/OverlayRenderer.h"
#include "rendering/TileRenderMetrics.h"
#include "rendering/WorldFoliage.h"
#include "rendering/WorldGridRenderer.h"
#include <atomic>
#include <chrono>
#include <future>

namespace Paladin
{
    // Fixed atlases are generated off the render thread, uploaded in strips,
    // and shared by all camera scales. Camera motion only transforms a mesh.
    class GlobeRenderer
    {
        struct Atlas
        {
            int w = 0, h = 0, left = 0, top = 0, density = 4;
            std::array<std::vector<RenderColor>, 3> pixels;
        };
        struct Source
        {
            WorldGrid grid;
            std::unordered_map<std::string, SceneSprite> art;
        };
        std::shared_ptr<Source> terrainSource_;
        std::future<Atlas> patchPending_;
        std::shared_ptr<Source> patchSource_;
        Atlas patchReady_, patchActive_;
        std::unique_ptr<Texture> patchTexture_, patchUpload_, patchWater_,
            patchWaterUpload_;
        int patchUploadRow_ = 0;
        int requestedX_ = -99999, requestedY_ = -99999;
        std::uint64_t patchBuilds_ = 0;
        const WorldTile* source_ = nullptr;
        std::uint64_t sourceRevision_ = 0;
        std::vector<std::future<Atlas>> retired_;
        std::future<Atlas> pending_;
        std::shared_ptr<std::atomic_bool> cancelled_;
        Atlas ready_;
        std::array<std::unique_ptr<Texture>, 3> textures_;
        std::unique_ptr<Texture> overview_;
        std::unique_ptr<Texture> atmosphereWhite_;
        int uploadLayer_ = 0, uploadRow_ = 0;
        std::vector<MeshVertex> vertices_;
        std::vector<int> indices_, glintIndices_;
        static std::string climate(const WorldTile& t)
        {
            if (t.biome == BiomeType::Polar || t.temperature.value() < .16)
            {
                return "tundra";
            }
            auto b = t.biome;
            if (t.terrain == TerrainType::Mountain || b == BiomeType::Hills)
            {
                const auto temp = t.temperature.value(),
                           rain = t.rainfall.value();
                b = temp >= .62 ? (rain < .24   ? BiomeType::Desert
                                   : rain < .68 ? BiomeType::Plain
                                                : BiomeType::Jungle)
                    : temp <= .34
                        ? (rain < .45 ? BiomeType::Tundra : BiomeType::Taiga)
                    : rain < .42 ? BiomeType::Plain
                                 : BiomeType::Forest;
            }
            const char* names[] = {
                "plain",
                "forest",
                "jungle",
                "desert",
                "tundra",
                "taiga",
                "water",
                "plain"
            };
            return names[std::min(7, int(b))];
        }
        static RenderColor base(
            const WorldTile& t,
            const std::unordered_map<std::string, SceneSprite>& art
        )
        {
            if (t.terrain == TerrainType::Water)
            {
                return {32, 44, 67, 255};
            }
            if (t.biome == BiomeType::Polar)
            {
                return {215, 224, 227, 255};
            }
            if (t.terrain == TerrainType::Mountain)
            {
                return {108, 116, 122, 255};
            }
            const auto it = art.find("world.terrain." + climate(t));
            return it == art.end() ? RenderColor{35, 87, 71, 255}
                                   : it->second.materialBase;
        }

        static Atlas buildAtlas(
            const WorldGrid& grid,
            const std::unordered_map<std::string, SceneSprite>& art,
            const std::shared_ptr<std::atomic_bool>& cancelled,
            int density,
            int left,
            int top,
            int tileWidth,
            int tileHeight
        )
        {
            Atlas a;
            const int w = grid.width(), h = grid.height();
            a.left = left;
            a.top = top;
            a.density = density;
            a.w = tileWidth * density;
            a.h = tileHeight * density;
            a.pixels[0].resize(std::size_t(a.w) * a.h);
            a.pixels[2].resize(std::size_t(a.w) * a.h);
            // Distance from shore creates connected continental shelves rather
            // than repeating blue tile borders. Prepared on the atlas worker.
            std::vector<int> shore(std::size_t(w) * h, (w + h) * (w + h)),
                nearest(std::size_t(w) * h, -1), frontier;
            for (int yy = 0; yy < h; ++yy)
            {
                for (int xx = 0; xx < w; ++xx)
                {
                    if (grid.tile({xx, yy})->terrain != TerrainType::Water)
                    {
                        shore[yy * w + xx] = 0;
                        nearest[yy * w + xx] = yy * w + xx;
                        frontier.push_back(yy * w + xx);
                    }
                }
            }
            for (std::size_t i = 0; i < frontier.size(); ++i)
            {
                if ((i % 4096) == 0 && cancelled->load())
                {
                    return Atlas{};
                }
                int x = frontier[i] % w, y = frontier[i] / w;
                const int seed = nearest[y * w + x];
                for (auto d : std::array<std::pair<int, int>, 8>{
                         {{1, 0},
                          {-1, 0},
                          {0, 1},
                          {0, -1},
                          {1, 1},
                          {1, -1},
                          {-1, 1},
                          {-1, -1}}
                     })
                {
                    int nx = (x + d.first + w) % w, ny = y + d.second;
                    if (ny < 0 || ny >= h)
                    {
                        continue;
                    }
                    const int rawDx = std::abs(nx - seed % w),
                              dx = std::min(rawDx, w - rawDx),
                              dy = ny - seed / w;
                    const int distance = dx * dx + dy * dy;
                    if (shore[ny * w + nx] > distance)
                    {
                        nearest[ny * w + nx] = seed;
                        shore[ny * w + nx] = distance;
                        frontier.push_back(ny * w + nx);
                    }
                }
            }
            std::vector<const SceneSprite*> materials(std::size_t(w) * h);
            for (int y = 0; y < h; ++y)
            {
                for (int x = 0; x < w; ++x)
                {
                    auto it = art.find(
                        "world.terrain." + climate(*grid.tile({x, y}))
                    );
                    if (it != art.end())
                    {
                        materials[y * w + x] = &it->second;
                    }
                }
            }
            for (int py = 0; py < a.h; ++py)
            {
                if (cancelled->load())
                {
                    return Atlas{};
                }
                for (int px = 0; px < a.w; ++px)
                {
                    const double xx = left + (px + .5) / density,
                                 yy = top + (py + .5) / density;
                    const auto sample = coastSample(xx, yy, true);
                    const auto occupied = [&](int x, int y)
                    {
                        return grid.tile({(x % w + w) % w,
                                          std::clamp(y, 0, h - 1)})
                                   ->terrain != TerrainType::Water;
                    };
                    const double land =
                        surfaceField(sample.x, sample.y, occupied);
                    const int ix = int(std::floor(sample.x - .5)),
                              iy = int(std::floor(sample.y - .5));
                    const bool dry = land >= .5;
                    int x = (int(std::floor(xx)) % w + w) % w,
                        y = std::clamp(int(yy), 0, h - 1);
                    double u = sample.x - .5 - ix, v = sample.y - .5 - iy;
                    u = u * u * (3 - 2 * u);
                    v = v * v * (3 - 2 * v);
                    const double choose =
                        landscapeField(xx * 2.1, yy * 2.1, 193) *
                        (dry ? land : 1 - land);
                    double cumulative = 0, exposure = 0, rock = 0, ice = 0;
                    bool chosen = false;
                    for (int j = 0; j < 2; ++j)
                    {
                        for (int i = 0; i < 2; ++i)
                        {
                            const int nx = ((ix + i) % w + w) % w,
                                      ny = std::clamp(iy + j, 0, h - 1);
                            const auto& n = *grid.tile({nx, ny});
                            const double weight =
                                (i ? u : 1 - u) * (j ? v : 1 - v);
                            rock +=
                                weight *
                                (n.terrain == TerrainType::Mountain ? 1 : 0);
                            ice +=
                                weight * (n.biome == BiomeType::Polar ? 1 : 0);
                            exposure +=
                                weight * (n.terrain == TerrainType::Mountain
                                              ? .88
                                          : (n.biome == BiomeType::Hills ||
                                             n.relief == ReliefType::Hills)
                                              ? .68
                                              : 0);
                            if (occupied(nx, ny) != dry)
                            {
                                continue;
                            }
                            cumulative += weight;
                            if (!chosen && choose < cumulative)
                            {
                                chosen = true;
                                x = nx;
                                y = ny;
                            }
                        }
                    }
                    const auto& t = *grid.tile({(x % w + w) % w, y});
                    auto color = base(t, art);
                    if (dry && materials[y * w + x])
                    {
                        color =
                            landscapePaint(*materials[y * w + x], xx, yy, true);
                    }
                    const double patch =
                        .55 * landscapeField(xx * .38, yy * .38, 931) +
                        .45 * landscapeField(xx * 5, yy * 5, 981);
                    if (dry && patch < exposure)
                    {
                        double grain = landscapeField(xx * 4, yy * 4, 713);
                        color = grain < .24   ? RenderColor{116, 81, 63, 255}
                                : grain > .79 ? RenderColor{113, 109, 112, 255}
                                              : RenderColor{78, 59, 57, 255};
                    }
                    if (dry && patch < rock * .92)
                    {
                        const double grain =
                            landscapeField(xx * 3, yy * 3, 811);
                        color = grain < .25   ? RenderColor{57, 70, 88, 255}
                                : grain > .76 ? RenderColor{154, 167, 175, 255}
                                              : RenderColor{108, 116, 122, 255};
                    }
                    // Sparse low moss and lichen retain tundra's exposed soil;
                    // this is a small palette shift, not a grassland carpet.
                    if (dry && t.biome == BiomeType::Tundra && rock < .3 &&
                        landscapeField(xx * .45, yy * .45, 824) > .60 &&
                        landscapeField(xx * 3, yy * 3, 831) > .56)
                    {
                        color = {79, 140, 122, 255};
                    }
                    if (dry && patch < ice)
                    {
                        const double drift =
                            landscapeField(xx * .8, yy * 1.5, 519);
                        color = drift < .26   ? RenderColor{126, 156, 170, 255}
                                : drift > .72 ? RenderColor{244, 243, 232, 255}
                                              : RenderColor{215, 224, 227, 255};
                    }
                    if (!dry)
                    {
                        const auto distanceAt = [&](int x, int y)
                        {
                            return std::sqrt(
                                double(shore
                                           [std::clamp(y, 0, h - 1) * w +
                                            (x % w + w) % w])
                            );
                        };
                        const double distance =
                            (1 - v) * ((1 - u) * distanceAt(ix, iy) +
                                       u * distanceAt(ix + 1, iy)) +
                            v * ((1 - u) * distanceAt(ix, iy + 1) +
                                 u * distanceAt(ix + 1, iy + 1));
                        const double shelf =
                            1.5 + 1.2 * landscapeField(xx * .04, yy * .04, 518);
                        const double basin =
                            distance / shelf +
                            .35 * landscapeField(xx * .18, yy * .18, 631);
                        // Water depth is a lighting gradient between palette
                        // anchors, not a set of hard contour bands.
                        const auto blendWater =
                            [](RenderColor a, RenderColor b, double t)
                        {
                            t = std::clamp(t, 0., 1.);
                            t = t * t * (3 - 2 * t);
                            return RenderColor{
                                std::uint8_t(a.red + (b.red - a.red) * t),
                                std::uint8_t(a.green + (b.green - a.green) * t),
                                std::uint8_t(a.blue + (b.blue - a.blue) * t),
                                255
                            };
                        };
                        color = basin < 1.5 ? blendWater(
                                                  {70, 98, 125, 255},
                                                  {48, 69, 93, 255},
                                                  basin / 1.5
                                              )
                                            : blendWater(
                                                  {48, 69, 93, 255},
                                                  {32, 44, 67, 255},
                                                  (basin - 1.5) / 2.5
                                              );
                    }
                    a.pixels[2][std::size_t(py) * a.w + px] =
                        {255, 255, 255, std::uint8_t(dry ? 0 : 255)};
                    a.pixels[0][std::size_t(py) * a.w + px] = color;
                }
            }
            a.pixels[1] = a.pixels[0];
            const auto stamp = [&](int layer,
                                   const std::string& id,
                                   double x,
                                   double y,
                                   double width,
                                   double height,
                                   bool snowy = false)
            {
                auto it = art.find(id);
                if (it == art.end() || !it->second.materialPixels)
                {
                    return;
                }
                const auto& s = it->second;
                for (int py = std::max(0, int((y - top) * density));
                     py < std::min(
                              a.h,
                              int(std::ceil((y + height - top) * density))
                          );
                     ++py)
                {
                    for (int px = std::max(0, int((x - left) * density));
                         px < std::min(
                                  a.w,
                                  int(std::ceil((x + width - left) * density))
                              );
                         ++px)
                    {
                        if (grid.tile(
                                    {(left + px / density + w) % w,
                                     std::clamp(top + py / density, 0, h - 1)}
                            )
                                ->terrain == TerrainType::Water)
                        {
                            continue;
                        }
                        const int sx = std::clamp(
                            int((left + (px + .5) / density - x) / width *
                                s.materialWidth),
                            0,
                            s.materialWidth - 1
                        );
                        const int sy = std::clamp(
                            int((top + (py + .5) / density - y) / height *
                                s.materialHeight),
                            0,
                            s.materialHeight - 1
                        );
                        auto color = (*s.materialPixels)
                            [std::size_t(sy) * s.materialWidth + sx];
                        if (color.alpha &&
                            id.find(".ridge.") != std::string::npos)
                        {
                            const double lum = color.red * .3 +
                                               color.green * .5 +
                                               color.blue * .2;
                            const double snowEdge = .25 +
                                                    .065 * std::sin(sx * .7) +
                                                    .035 * std::sin(sx * 1.9);
                            if (snowy &&
                                double(sy) / s.materialHeight < snowEdge)
                            {
                                color = lum < 90
                                            ? RenderColor{126, 156, 170, 255}
                                        : lum < 145
                                            ? RenderColor{175, 201, 214, 255}
                                            : RenderColor{244, 243, 232, 255};
                            }
                            else
                            {
                                color = lum < 65 ? RenderColor{53, 56, 62, 255}
                                        : lum < 100
                                            ? RenderColor{57, 70, 88, 255}
                                        : lum < 135
                                            ? RenderColor{108, 116, 122, 255}
                                        : lum < 170
                                            ? RenderColor{154, 167, 175, 255}
                                            : RenderColor{215, 224, 227, 255};
                            }
                        }
                        if (color.alpha)
                        {
                            a.pixels[layer][std::size_t(py) * a.w + px] = color;
                        }
                    }
                }
            };
            for (int row = std::max(0, (top - 8) / 3);
                 row <= std::min(h / 3, (top + tileHeight + 8) / 3);
                 ++row)
            {
                for (int col = int(std::floor((left - 8) / 4.));
                     col <= (left + tileWidth + 8) / 4;
                     ++col)
                {
                    const auto hash = landscapeHash(
                        (col % std::max(1, w / 4) + std::max(1, w / 4)) %
                            std::max(1, w / 4),
                        row,
                        431
                    );
                    const int x = col * 4 + int(hash % 3),
                              y = row * 3 + int((hash >> 4) % 3);
                    const auto* t = grid.tile({(x % w + w) % w, y});
                    if (!t || t->terrain != TerrainType::Mountain)
                    {
                        continue;
                    }
                    bool core = true;
                    for (const auto& d : std::array<std::pair<int, int>, 4>{
                             {{-1, 0}, {1, 0}, {0, -1}, {0, 1}}
                         })
                    {
                        auto* n = grid.tile(
                            {((x + d.first) % w + w) % w, y + d.second}
                        );
                        core &= n && n->terrain == TerrainType::Mountain;
                    }
                    stamp(
                        1,
                        "world.relief.ridge." +
                            std::to_string(1 + (hash >> 8) % 2) + "." +
                            climate(*t),
                        x - .5 + (hash % 17) / 34.,
                        y - .5,
                        3.7 + ((hash >> 12) % 17) / 10.,
                        (core ? 4.9 : 3.5) + ((hash >> 18) % 9) / 10.,
                        core || t->temperature.value() < .24
                    );
                }
            }
            if (density >= 8)
            {
                for (int y = std::max(0, top - 4);
                     y < std::min(h, top + tileHeight + 4);
                     ++y)
                {
                    for (int x = left - 4; x < left + tileWidth + 4; ++x)
                    {
                        const auto& t = *grid.tile({(x % w + w) % w, y});
                        const auto hash =
                            landscapeHash((x % w + w) % w, y, 171);
                        if (t.terrain == TerrainType::Land &&
                            (t.relief == ReliefType::Hills ||
                             t.biome == BiomeType::Hills) &&
                            hash % 5 == 0)
                        {
                            stamp(
                                1,
                                "world.relief.hill." +
                                    std::to_string(1 + (hash >> 8) % 2) + "." +
                                    climate(t),
                                x - .5,
                                y - .4,
                                3.0 + ((hash >> 14) % 9) / 10.,
                                1.6 + ((hash >> 19) % 7) / 10.
                            );
                        }
                    }
                }
            }
            return a;
        }

    public:
        ~GlobeRenderer()
        {
            if (cancelled_)
            {
                cancelled_->store(true);
            }
        }
        std::uint64_t atlasBuilds = 0;
        bool detailReady() const
        {
            return uploadLayer_ == 3;
        }
        bool fullDetailReady() const
        {
            return bool(patchTexture_) && !patchPending_.valid() &&
                   !patchReady_.w;
        }
        std::uint64_t detailBuilds() const
        {
            return patchBuilds_;
        }
        void reset()
        {
            patchTexture_.reset();
            patchActive_ = {};
            if (cancelled_)
            {
                cancelled_->store(true);
            }
            source_ = nullptr;
        }
        void prepare(
            Renderer& r,
            const World& world,
            const SceneSpriteLibrary& library
        )
        {
            const auto& g = world.grid();
            if (source_ == g.tile({0, 0}) && sourceRevision_ == g.revision())
            {
                return;
            }
            source_ = g.tile({0, 0});
            sourceRevision_ = g.revision();
            if (pending_.valid())
            {
                retired_.push_back(std::move(pending_));
            }
            if (cancelled_)
            {
                cancelled_->store(true);
            }
            cancelled_ = std::make_shared<std::atomic_bool>(false);
            ++atlasBuilds;
            std::unordered_map<std::string, SceneSprite> art;
            const auto add = [&](const std::string& id)
            {
                if (const auto* sprite = library.find(id))
                {
                    auto cpu = *sprite;
                    cpu.texture.reset();
                    cpu.shadow.reset();
                    art.emplace(id, std::move(cpu));
                }
            };
            for (const char* c :
                 {"plain",
                  "forest",
                  "jungle",
                  "desert",
                  "tundra",
                  "taiga",
                  "water"})
            {
                add(std::string("world.terrain.") + c);
                for (const char* k : {"hill", "ridge"})
                {
                    for (int i : {1, 2})
                    {
                        add(std::string("world.relief.") + k + "." +
                            std::to_string(i) + "." + c);
                    }
                }
            }
            add("world.canopy.1");
            add("world.canopy.2");
            std::vector<RenderColor> coarse;
            coarse.reserve(g.tileCount());
            for (int y = 0; y < g.height(); ++y)
            {
                for (int x = 0; x < g.width(); ++x)
                {
                    coarse.push_back(base(*g.tile({x, y}), art));
                }
            }
            overview_ =
                r.createTextureFromPixels(g.width(), g.height(), coarse);
            uploadLayer_ = 0;
            uploadRow_ = 0;
            ready_ = {};
            for (auto& texture : textures_)
            {
                texture.reset();
            }
            terrainSource_ =
                std::make_shared<Source>(Source{g, std::move(art)});
            requestedX_ = requestedY_ = -99999;
            patchTexture_.reset();
            patchUpload_.reset();
            patchWater_.reset();
            patchWaterUpload_.reset();
            patchActive_ = {};
            patchReady_ = {};
            pending_ = std::async(
                std::launch::async,
                [source = terrainSource_, cancelled = cancelled_]()
                {
                    return buildAtlas(
                        source->grid,
                        source->art,
                        cancelled,
                        4,
                        0,
                        0,
                        source->grid.width(),
                        source->grid.height()
                    );
                }
            );
        }
        void updateAtlas(
            Renderer& r,
            const World& world,
            const SceneSpriteLibrary& art
        )
        {
            std::erase_if(
                retired_,
                [](auto& f)
                {
                    return f.wait_for(std::chrono::seconds(0)) ==
                           std::future_status::ready;
                }
            );
            prepare(r, world, art);
            if (patchPending_.valid() &&
                patchPending_.wait_for(std::chrono::seconds(0)) ==
                    std::future_status::ready)
            {
                auto result = patchPending_.get();
                if (patchSource_ == terrainSource_)
                {
                    patchReady_ = std::move(result);
                    patchReady_.pixels[0] = {};
                    patchUploadRow_ = 0;
                }
            }
            if (patchReady_.w)
            {
                if (!patchUpload_)
                {
                    patchUpload_ =
                        r.createEmptyTexture(patchReady_.w, patchReady_.h);
                    patchWaterUpload_ =
                        r.createEmptyTexture(patchReady_.w, patchReady_.h);
                }
                const int rows = std::min(32, patchReady_.h - patchUploadRow_);
                r.updateTextureRegion(
                    *patchUpload_,
                    0,
                    patchUploadRow_,
                    patchReady_.w,
                    rows,
                    std::span(patchReady_.pixels[1])
                        .subspan(
                            std::size_t(patchUploadRow_) * patchReady_.w,
                            std::size_t(rows) * patchReady_.w
                        )
                );
                r.updateTextureRegion(
                    *patchWaterUpload_,
                    0,
                    patchUploadRow_,
                    patchReady_.w,
                    rows,
                    std::span(patchReady_.pixels[2])
                        .subspan(
                            std::size_t(patchUploadRow_) * patchReady_.w,
                            std::size_t(rows) * patchReady_.w
                        )
                );
                patchUploadRow_ += rows;
                if (patchUploadRow_ == patchReady_.h)
                {
                    patchReady_.pixels[1] = {};
                    patchReady_.pixels[2] = {};
                    patchActive_ = std::move(patchReady_);
                    patchReady_ = {};
                    patchTexture_ = std::move(patchUpload_);
                    patchWater_ = std::move(patchWaterUpload_);
                }
            }
            if (pending_.valid() &&
                pending_.wait_for(std::chrono::seconds(0)) ==
                    std::future_status::ready)
            {
                ready_ = pending_.get();
            }
            if (ready_.w && uploadLayer_ < 3)
            {
                if (!textures_[uploadLayer_])
                {
                    textures_[uploadLayer_] =
                        r.createEmptyTexture(ready_.w, ready_.h);
                }
                const int rows = std::min(32, ready_.h - uploadRow_);
                r.updateTextureRegion(
                    *textures_[uploadLayer_],
                    0,
                    uploadRow_,
                    ready_.w,
                    rows,
                    std::span(ready_.pixels[uploadLayer_])
                        .subspan(
                            std::size_t(uploadRow_) * ready_.w,
                            std::size_t(rows) * ready_.w
                        )
                );
                uploadRow_ += rows;
                if (uploadRow_ == ready_.h)
                {
                    ready_.pixels[uploadLayer_].clear();
                    ++uploadLayer_;
                    uploadRow_ = 0;
                }
            }
        }
        void requestDetail(const World& world, const Camera2D& camera)
        {
            const int pw = std::min(128, world.grid().width()),
                      ph = std::min(96, world.grid().height());
            const int wantedX = int(camera.tileX()) - pw / 2,
                      wantedY = std::clamp(
                          int(camera.tileY()) - ph / 2,
                          0,
                          world.grid().height() - ph
                      );
            if (!patchPending_.valid() && !patchReady_.w &&
                (std::abs(wantedX - requestedX_) > pw / 5 ||
                 std::abs(wantedY - requestedY_) > ph / 5))
            {
                requestedX_ = wantedX;
                requestedY_ = wantedY;
                patchSource_ = terrainSource_;
                ++patchBuilds_;
                patchPending_ = std::async(
                    std::launch::async,
                    [source = terrainSource_,
                     cancelled = cancelled_,
                     wantedX,
                     wantedY,
                     pw,
                     ph]()
                    {
                        return buildAtlas(
                            source->grid,
                            source->art,
                            cancelled,
                            16,
                            wantedX,
                            wantedY,
                            pw,
                            ph
                        );
                    }
                );
            }
        }
        const Texture* mapTexture(double tilePixels) const
        {
            const int level =
                std::min(tilePixels >= 3 ? 1 : 0, uploadLayer_ - 1);
            return level >= 0 ? textures_[level].get() : overview_.get();
        }
        void renderFlat(
            Renderer& r,
            const World& world,
            const Camera2D& camera,
            const SceneSpriteLibrary& art,
            double pixels
        )
        {
            updateAtlas(r, world, art);
            requestDetail(world, camera);
            r.fillRectangle(
                0,
                0,
                float(r.outputWidth()),
                float(r.outputHeight()),
                {8, 15, 27, 255}
            );
            const auto* texture = mapTexture(pixels);
            if (!texture)
            {
                return;
            }
            const double gw = world.grid().width(), gh = world.grid().height();
            const double left = camera.tileX() - r.outputWidth() / (2 * pixels),
                         top = camera.tileY() - r.outputHeight() / (2 * pixels);
            const auto draw = [&](const Texture& tex,
                                  double tx,
                                  double ty,
                                  double tw,
                                  double th)
            {
                const double x0 = std::max(left, tx), y0 = std::max(top, ty);
                const double
                    x1 = std::min(left + r.outputWidth() / pixels, tx + tw),
                    y1 = std::min(top + r.outputHeight() / pixels, ty + th);
                if (x1 <= x0 || y1 <= y0)
                {
                    return;
                }
                std::vector<MeshVertex> verts;
                std::vector<int> ids;
                const int nx = std::clamp(int((x1 - x0) / gw * 96) + 1, 1, 96),
                          ny = std::clamp(int((y1 - y0) / gh * 48) + 1, 1, 48);
                for (int j = 0; j <= ny; ++j)
                {
                    for (int i = 0; i <= nx; ++i)
                    {
                        const double x = std::lerp(x0, x1, double(i) / nx),
                                     y = std::lerp(y0, y1, double(j) / ny);
                        verts.push_back(
                            {float((x - left) * pixels),
                             float((y - top) * pixels),
                             float((x - tx) / tw),
                             float((y - ty) / th),
                             globeLight(
                                 x / gw,
                                 y / gh,
                                 world.time().secondsIntoDay(),
                                 1
                             )}
                        );
                    }
                }
                for (int j = 0; j < ny; ++j)
                {
                    for (int i = 0; i < nx; ++i)
                    {
                        int k = j * (nx + 1) + i;
                        ids.insert(
                            ids.end(),
                            {k, k + 1, k + nx + 2, k, k + nx + 2, k + nx + 1}
                        );
                    }
                }
                r.drawMesh(tex, verts, ids);
            };
            for (int wrap = int(std::floor(left / gw));
                 wrap <=
                 int(std::floor((left + r.outputWidth() / pixels) / gw));
                 ++wrap)
            {
                draw(*texture, wrap * gw, 0, gw, gh);
                if (pixels >= 8 && patchTexture_)
                {
                    const double pw = double(patchActive_.w) /
                                      patchActive_.density,
                                 ph = double(patchActive_.h) /
                                      patchActive_.density;
                    for (int offset = -1; offset <= 1; ++offset)
                    {
                        draw(
                            *patchTexture_,
                            patchActive_.left + (wrap + offset) * gw,
                            patchActive_.top,
                            pw,
                            ph
                        );
                    }
                }
            }
        }
        void render(
            Renderer& r,
            const World& world,
            const Camera2D& camera,
            const SceneSpriteLibrary& art,
            std::span<const TileOverlayRenderItem> overlays,
            std::span<const TileOutlineRenderItem> outlines
        )
        {
            updateAtlas(r, world, art);
            const auto view = GlobeView::from(
                camera,
                world.grid(),
                r.outputWidth(),
                r.outputHeight()
            );
            r.fillRectangle(
                0,
                0,
                float(r.outputWidth()),
                float(r.outputHeight()),
                {8, 15, 27, 255}
            );
            // A fixed celestial shell follows camera orientation instead of
            // sticking to screen pixels. Generated once; no textures/uploads
            // or random changes during dragging and zooming.
            static const auto stars = []
            {
                std::vector<WorldSurface::Point3> points;
                points.reserve(6000);
                for (int i = 0; i < 6000; ++i)
                {
                    const auto a = landscapeHash(i, 17, 991),
                               b = landscapeHash(i, 31, 773);
                    const double angle = (a / double(UINT32_MAX)) * 6.283185307;
                    const double y = 2 * (b / double(UINT32_MAX)) - 1;
                    const double radius = std::sqrt(std::max(0., 1 - y * y));
                    points.push_back(
                        {radius * std::sin(angle), y, radius * std::cos(angle)}
                    );
                }
                return points;
            }();
            const double focal =
                std::min(r.outputWidth(), r.outputHeight()) * .65;
            for (std::size_t i = 0; i < stars.size(); ++i)
            {
                const auto star = view.orient(stars[i]);
                if (star.z <= .05)
                {
                    continue;
                }
                const float x = float(view.cx + focal * star.x / star.z),
                            y = float(view.cy - focal * star.y / star.z);
                if (x < 0 || y < 0 || x >= r.outputWidth() ||
                    y >= r.outputHeight())
                {
                    continue;
                }
                const bool bright = i % 17 == 0;
                r.fillRectangle(
                    std::floor(x),
                    std::floor(y),
                    bright ? 2.F : 1.F,
                    bright ? 2.F : 1.F,
                    bright ? RenderColor{244, 243, 232, 255}
                           : RenderColor{175, 201, 214, 220}
                );
            }
            vertices_.clear();
            indices_.clear();
            struct V
            {
                WorldSurface::Point3 p;
                double u, v;
            };
            const auto vertex = [&](double u, double v)
            { return V{view.orient(WorldSurface::sphere(u, v)), u, v}; };
            const auto triangle = [&](V a, V b, V c)
            {
                std::array<V, 5> poly{};
                int count = 0;
                const V input[] = {a, b, c};
                for (int i = 0; i < 3; ++i)
                {
                    const auto& p = input[i];
                    const auto& q = input[(i + 1) % 3];
                    if (p.p.z >= 0)
                    {
                        poly[count++] = p;
                    }
                    if ((p.p.z >= 0) != (q.p.z >= 0))
                    {
                        const double t = p.p.z / (p.p.z - q.p.z);
                        poly[count++] = {
                            {std::lerp(p.p.x, q.p.x, t),
                             std::lerp(p.p.y, q.p.y, t),
                             0},
                            std::lerp(p.u, q.u, t),
                            std::lerp(p.v, q.v, t)
                        };
                    }
                }
                if (count < 3)
                {
                    return;
                }
                const int first = int(vertices_.size());
                for (int i = 0; i < count; ++i)
                {
                    const auto& v = poly[i];
                    auto light = globeLight(
                        v.u,
                        v.v,
                        world.time().secondsIntoDay(),
                        v.p.z
                    );
                    const auto& g = world.grid();
                    const auto* landTile = g.tile(
                        {(int(std::floor(v.u * g.width())) % g.width() +
                          g.width()) %
                             g.width(),
                         std::clamp(int(v.v * g.height()), 0, g.height() - 1)}
                    );
                    if (landTile->terrain == TerrainType::Mountain ||
                        landTile->relief == ReliefType::Hills)
                    {
                        const auto heightAt = [&](double u, double vv)
                        {
                            double x = u * g.width() - .5,
                                   y = vv * g.height() - .5;
                            int ix = int(std::floor(x)),
                                iy = int(std::floor(y));
                            double fx = x - ix, fy = y - iy;
                            const auto e = [&](int x, int y)
                            {
                                return g
                                    .tile(
                                        {(x % g.width() + g.width()) %
                                             g.width(),
                                         std::clamp(y, 0, g.height() - 1)}
                                    )
                                    ->elevation.value();
                            };
                            return (1 - fy) * ((1 - fx) * e(ix, iy) +
                                               fx * e(ix + 1, iy)) +
                                   fy * ((1 - fx) * e(ix, iy + 1) +
                                         fx * e(ix + 1, iy + 1));
                        };
                        const double du = 1. / g.width(), dv = 1. / g.height();
                        const double dx = heightAt(v.u + du, v.v) -
                                          heightAt(v.u - du, v.v),
                                     dy = heightAt(v.u, v.v + dv) -
                                          heightAt(v.u, v.v - dv);
                        const double seconds = world.time().secondsIntoDay();
                        const double east =
                            (globeSunDot(v.u + du, v.v, seconds) -
                             globeSunDot(v.u - du, v.v, seconds)) /
                            (du * 2);
                        const double south =
                            (globeSunDot(v.u, v.v + dv, seconds) -
                             globeSunDot(v.u, v.v - dv, seconds)) /
                            (dv * 2);
                        const double shade = std::clamp(
                            1. - std::max(0., dx * east + dy * south) * 1.5,
                            .72,
                            1.
                        );
                        light.red = std::uint8_t(light.red * shade);
                        light.green = std::uint8_t(light.green * shade);
                        light.blue = std::uint8_t(light.blue * shade);
                    }
                    vertices_.push_back(
                        {float(view.cx + v.p.x * view.radius),
                         float(view.cy - v.p.y * view.radius),
                         float(v.u),
                         float(v.v),
                         light}
                    );
                }
                for (int i = 1; i < count - 1; ++i)
                {
                    indices_.push_back(first);
                    indices_.push_back(first + i);
                    indices_.push_back(first + i + 1);
                }
            };
            constexpr int columns = 96, rows = 48;
            for (int y = 0; y < rows; ++y)
            {
                for (int x = 0; x < columns; ++x)
                {
                    auto a = vertex(double(x) / columns, double(y) / rows),
                         b = vertex(double(x + 1) / columns, double(y) / rows);
                    auto c = vertex(
                             double(x + 1) / columns,
                             double(y + 1) / rows
                         ),
                         d = vertex(double(x) / columns, double(y + 1) / rows);
                    triangle(a, b, c);
                    triangle(a, c, d);
                }
            }
            const double tilePixels =
                view.radius * 6.283185307 / world.grid().width();
            // A fixed-resolution local atlas is prepared off-thread. One
            // coherent patch is promoted only after its upload is complete;
            // terrain and relief share the same canonical 16-pixel samples.
            requestDetail(world, camera);
            // Zoom changes only which prebuilt illustration is sampled.
            // Small foliage and foothill stamps are absent at regional scale.
            int level = tilePixels >= 3 ? 1 : 0;
            level = std::min(level, uploadLayer_ - 1);
            const auto* texture =
                level >= 0 ? textures_[level].get() : overview_.get();
            // Keep the broad solar reflection at globe scale, then smoothly
            // remove it as the camera approaches the terrain. Both atlas
            // passes share this factor so detail promotion cannot restore it.
            const double glintZoom =
                std::clamp((camera.zoom() - 1.) / 4., 0., 1.);
            const double glintOpacity =
                1. - glintZoom * glintZoom * (3. - 2. * glintZoom);
            const auto glint = [&](const Texture& mask, const Atlas* patch)
            {
                if (glintOpacity <= 0.)
                {
                    return;
                }
                for (auto& v : vertices_)
                {
                    double u = v.u, vv = v.v;
                    if (patch)
                    {
                        u = (patch->left + u * patch->w / patch->density) /
                            world.grid().width();
                        vv = (patch->top + vv * patch->h / patch->density) /
                             world.grid().height();
                    }
                    const double shine = oceanSunGlint(
                        u,
                        vv,
                        world.time().secondsIntoDay(),
                        camera.tileX() / world.grid().width(),
                        camera.tileY() / world.grid().height()
                    );
                    v.color = {
                        255,
                        240,
                        196,
                        std::uint8_t(v.color.alpha * shine * glintOpacity)
                    };
                }
                // Skip invisible triangles rather than blending a full-screen
                // transparent water layer on the software renderer.
                glintIndices_.clear();
                for (std::size_t i = 0; i + 2 < indices_.size(); i += 3)
                {
                    if (vertices_[indices_[i]].color.alpha > 1 ||
                        vertices_[indices_[i + 1]].color.alpha > 1 ||
                        vertices_[indices_[i + 2]].color.alpha > 1)
                    {
                        glintIndices_.insert(
                            glintIndices_.end(),
                            {indices_[i], indices_[i + 1], indices_[i + 2]}
                        );
                    }
                }
                if (!glintIndices_.empty())
                {
                    r.drawMesh(mask, vertices_, glintIndices_);
                }
            };
            if (texture)
            {
                r.drawMesh(*texture, vertices_, indices_);
                if (uploadLayer_ >= 3)
                {
                    glint(*textures_[2], nullptr);
                }
            }
            if (patchTexture_ && tilePixels > 12)
            {
                vertices_.clear();
                indices_.clear();
                const auto& a = patchActive_;
                const int width = a.w / a.density, height = a.h / a.density;
                for (int y = 0; y < height; y += 2)
                {
                    for (int x = 0; x < width; x += 2)
                    {
                        const double
                            u0 = double(a.left + x) / world.grid().width(),
                            u1 = double(a.left + std::min(x + 2, width)) /
                                 world.grid().width(),
                            v0 = double(a.top + y) / world.grid().height(),
                            v1 = double(a.top + std::min(y + 2, height)) /
                                 world.grid().height();
                        triangle(
                            vertex(u0, v0),
                            vertex(u1, v0),
                            vertex(u1, v1)
                        );
                        triangle(
                            vertex(u0, v0),
                            vertex(u1, v1),
                            vertex(u0, v1)
                        );
                    }
                }
                for (auto& v : vertices_)
                {
                    const double x = v.u * world.grid().width() - a.left,
                                 y = v.v * world.grid().height() - a.top;
                    const double edge = std::clamp(
                        std::min({x, y, width - x, height - y}) / 8.,
                        0.,
                        1.
                    );
                    v.color.alpha = std::uint8_t(
                        255 * edge * std::clamp((tilePixels - 12) / 12., 0., 1.)
                    );
                    v.u = float(x / width);
                    v.v = float(y / height);
                }
                r.drawMesh(*patchTexture_, vertices_, indices_);
                if (patchWater_)
                {
                    glint(*patchWater_, &patchActive_);
                }
            }
            worldFoliageProjected(r, world, camera, tilePixels, true, art);
            // A thin, sun-weighted atmospheric limb. One immutable white texel
            // and a bounded mesh: no per-frame textures or terrain resampling.
            if (!atmosphereWhite_)
            {
                atmosphereWhite_ = r.createTextureFromPixels(
                    1,
                    1,
                    std::array<RenderColor, 1>{{{255, 255, 255, 255}}}
                );
            }
            if (atmosphereWhite_)
            {
                vertices_.clear();
                indices_.clear();
                constexpr int segments = 192;
                constexpr std::array<double, 4> radii{.963, .994, 1.003, 1.024};
                constexpr std::array<double, 4> opacity{0., .20, .12, 0.};
                for (int i = 0; i <= segments; ++i)
                {
                    const double angle = i * 6.283185307179586 / segments;
                    const double dx = std::cos(angle), dy = std::sin(angle);
                    const auto uv = view.pick(
                        view.cx + dx * view.radius * .999,
                        view.cy + dy * view.radius * .999
                    );
                    const double sun =
                        uv ? std::clamp(
                                 (globeSunDot(
                                      uv->u,
                                      uv->v,
                                      world.time().secondsIntoDay()
                                  ) +
                                  .16) /
                                     .75,
                                 0.,
                                 1.
                             )
                           : 0.;
                    for (int band = 0; band < 4; ++band)
                    {
                        vertices_.push_back(
                            {float(view.cx + dx * view.radius * radii[band]),
                             float(view.cy + dy * view.radius * radii[band]),
                             .5F,
                             .5F,
                             {126,
                              156,
                              170,
                              std::uint8_t(
                                  255 * opacity[band] * (.18 + .82 * sun)
                              )}}
                        );
                    }
                    if (i)
                    {
                        for (int band = 0; band < 3; ++band)
                        {
                            const int a = (i - 1) * 4 + band, b = i * 4 + band;
                            for (int n : {a, b, b + 1, a, b + 1, a + 1})
                            {
                                indices_.push_back(n);
                            }
                        }
                    }
                }
                r.drawMesh(*atmosphereWhite_, vertices_, indices_);
            }
            const auto outline = [&](double x,
                                     double y,
                                     double width,
                                     double height,
                                     RenderColor color)
            {
                WorldSurface::Point3 previous{};
                bool have = false;
                for (int side = 0; side < 4; ++side)
                {
                    for (int i = 0; i <= 24; ++i)
                    {
                        double f = i / 24., xx = x, yy = y;
                        if (side == 0)
                        {
                            xx += width * f;
                        }
                        if (side == 1)
                        {
                            xx += width;
                            yy += height * f;
                        }
                        if (side == 2)
                        {
                            xx += width * (1 - f);
                            yy += height;
                        }
                        if (side == 3)
                        {
                            yy += height * (1 - f);
                        }
                        auto p = view.project(
                            xx / world.grid().width(),
                            yy / world.grid().height()
                        );
                        if (have && p.z > 0 && previous.z > 0)
                        {
                            r.drawLine(
                                float(previous.x),
                                float(previous.y),
                                float(p.x),
                                float(p.y),
                                color
                            );
                        }
                        previous = p;
                        have = true;
                    }
                }
            };
            for (const auto& city : world.settlements())
            {
                const auto at = city.position();
                // Foliage submission skips the settlement clearing; the
                // underlying textured ground and biome remain intact.
                auto p = view.project(
                    (at.x + .5) / world.grid().width(),
                    (at.y + .5) / world.grid().height()
                );
                if (p.z <= 0)
                {
                    continue;
                }
                if (const auto* marker = art.find("world.settlement"))
                {
                    auto f = art.frame(*marker, false);
                    r.drawTexture(
                        *marker->texture,
                        f.x,
                        f.y,
                        f.width,
                        f.height,
                        float(p.x - 10),
                        float(p.y - 16),
                        20,
                        20
                    );
                }
            }
            for (const auto& o : overlays)
            {
                outline(o.tileX, o.tileY, o.widthTiles, o.heightTiles, o.color);
            }
            for (const auto& o : outlines)
            {
                outline(o.tileX, o.tileY, o.widthTiles, o.heightTiles, o.color);
            }
        }
    };
} // namespace Paladin

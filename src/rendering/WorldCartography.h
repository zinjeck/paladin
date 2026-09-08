#pragma once
#include "rendering/ScenePresentation.h"
#include "rendering/Texture.h"
#include "rendering/WorldSurface.h"
#include "world/World.h"
#include <array>
#include <unordered_map>
namespace Paladin
{
    class WorldCartography
    {
        const WorldTile* source_ = nullptr;
        std::uint64_t revision_ = ~std::uint64_t(0), signature_ = 0;
        std::unique_ptr<Texture> map_;

    public:
        std::uint64_t rebuildCount = 0;
        void reset()
        {
            source_ = nullptr;
            map_.reset();
        }
        void render(
            Renderer& renderer,
            const World& world,
            const SceneProjection& p,
            bool draw = true
        )
        {
            const auto& grid = world.grid();
            const int w = grid.width(), h = grid.height();
            if (w <= 0 || h <= 0)
            {
                return;
            }
            std::uint64_t signature = w * 65537ull + h;
            for (const auto& realm : world.realms())
            {
                const auto c = realm.mapColor();
                signature = signature * 31 + realm.id().value() +
                            c.red * 65536 + c.green * 256 + c.blue;
            }
            for (int i = 1; i < 8; ++i)
            {
                signature =
                    signature * 31 +
                    unsigned(
                        grid.tile({w * i / 8, h * i / 8})->elevation.value() *
                        65535
                    );
            }
            if (source_ != grid.tile({0, 0}) ||
                revision_ != world.territory().revision() ||
                signature_ != signature)
            {
                ++rebuildCount;
                source_ = grid.tile({0, 0});
                revision_ = world.territory().revision();
                signature_ = signature;
                const int density = std::clamp(1024 / std::max(w, h), 1, 2),
                          tw = w * density, th = h * density;
                std::vector<RenderColor> pixels(std::size_t(tw) * th);
                std::vector<std::uint64_t> region(pixels.size());
                std::unordered_map<std::uint64_t, RenderColor> colors;
                for (const auto& realm : world.realms())
                {
                    auto c = realm.mapColor();
                    const double gray = (c.red + c.green + c.blue) / 3.;
                    colors[realm.id().value()] = {
                        std::uint8_t(c.red * .56 + gray * .24 + 28),
                        std::uint8_t(c.green * .56 + gray * .24 + 28),
                        std::uint8_t(c.blue * .56 + gray * .24 + 28),
                        255
                    };
                }
                for (int py = 0; py < th; ++py)
                {
                    for (int px = 0; px < tw; ++px)
                    {
                        const double xx = (px + .5) / density - .5,
                                     yy = (py + .5) / density - .5;
                        const int x = int(std::floor(xx)),
                                  y = int(std::floor(yy));
                        const double u = xx - x, v = yy - y;
                        double land = 0, mountain = 0, height = 0, best = -1;
                        RealmId owner;
                        std::array<RealmId, 4> owners{};
                        std::array<double, 4> weights{};
                        for (int j = 0; j < 2; ++j)
                        {
                            for (int i = 0; i < 2; ++i)
                            {
                                WorldTilePosition at{
                                    std::clamp(x + i, 0, w - 1),
                                    std::clamp(y + j, 0, h - 1)
                                };
                                const auto* t = grid.tile(at);
                                const double weight =
                                    (i ? u : 1 - u) * (j ? v : 1 - v);
                                height += weight * t->elevation.value();
                                if (t->terrain != TerrainType::Water)
                                {
                                    land += weight;
                                    mountain +=
                                        weight *
                                        (t->terrain == TerrainType::Mountain);
                                    owners[j * 2 + i] =
                                        world.territory().controllerAt(at);
                                    weights[j * 2 + i] = weight;
                                }
                            }
                        }
                        for (int i = 0; i < 4; ++i)
                        {
                            double total = 0;
                            for (int j = 0; j < 4; ++j)
                            {
                                if (owners[i] == owners[j])
                                {
                                    total += weights[j];
                                }
                            }
                            if (total > best)
                            {
                                best = total;
                                owner = owners[i];
                            }
                        }
                        const bool water = land < .5;
                        const std::size_t at = std::size_t(py) * tw + px;
                        RenderColor color{0xA9, 0x94, 0x78, 255};
                        if (water)
                        {
                            color = land > .25
                                        ? RenderColor{0x30, 0x45, 0x5D, 255}
                                        : RenderColor{0x20, 0x2C, 0x43, 255};
                        }
                        else if (owner)
                        {
                            color = colors[owner.value()];
                        }
                        const double shade =
                            water ? 1
                                  : std::clamp(
                                        1.02 - height * .08 - mountain * .08,
                                        .84,
                                        1.02
                                    );
                        color.red = std::uint8_t(color.red * shade);
                        color.green = std::uint8_t(color.green * shade);
                        color.blue = std::uint8_t(color.blue * shade);
                        pixels[at] = color;
                        region[at] = water ? ~std::uint64_t(0) : owner.value();
                    }
                }
                // Fine ink borders follow the sampled contours, not square
                // tiles.
                for (int y = 1; y < th - 1; ++y)
                {
                    for (int x = 1; x < tw - 1; ++x)
                    {
                        auto at = std::size_t(y) * tw + x;
                        if (region[at] != region[at - 1] ||
                            region[at] != region[at - tw])
                        {
                            auto& c = pixels[at];
                            c.red = std::uint8_t(c.red * .36);
                            c.green = std::uint8_t(c.green * .36);
                            c.blue = std::uint8_t(c.blue * .36);
                        }
                    }
                }
                map_ = renderer.createTextureFromPixels(tw, th, pixels);
            }
            if (!draw)
            {
                return;
            }
            const auto& texture = map_;
            if (!texture)
            {
                return;
            }
            renderer.drawTexture(
                *texture,
                0,
                0,
                float(texture->width()),
                float(texture->height()),
                float(p.screenWidth * .5 - p.cameraX * p.tilePixels),
                float(p.screenHeight * .5 - p.cameraY * p.tilePixels),
                float(w * p.tilePixels),
                float(h * p.tilePixels)
            );
        }
    };
} // namespace Paladin

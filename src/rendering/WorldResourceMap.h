#pragma once

#include "rendering/SceneSpriteLibrary.h"
#include "rendering/WorldMapNavigation.h"
#include "ui/GrayUiRenderer.h"
#include "world/ResourceSurvey.h"
#include "world/World.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include <array>
#include <vector>

namespace Paladin
{
    // Static resource geography is reduced once, in bounded slices. Parent
    // cells accumulate the same survey values, so zoom changes never rescan
    // the planet or allocate textures. Symbols are native map annotations.
    class WorldResourceMap
    {
        struct Cell
        {
            std::array<double, 8> amounts{};
            double tiles = 0;
        };
        struct Level
        {
            int width = 0, height = 0, side = 16;
            std::vector<Cell> cells;
        };
        std::vector<Level> levels_;
        std::vector<std::uint8_t> occupied_;
        std::size_t cursor_ = 0, lastPreparedTiles_ = 0;
        std::uint64_t seed_ = 0, revision_ = 0;
        int width_ = 0, height_ = 0;

    public:
        void reset()
        {
            levels_.clear();
        }
        bool ready() const
        {
            return !levels_.empty() && cursor_ == levels_[0].cells.size();
        }
        std::size_t lastPreparedTiles() const
        {
            return lastPreparedTiles_;
        }
        void prepare(const World& world)
        {
            lastPreparedTiles_ = 0;
            const auto& grid = world.grid();
            if (levels_.empty() || seed_ != world.generationSeed() ||
                revision_ != grid.revision() || width_ != grid.width() ||
                height_ != grid.height())
            {
                levels_.clear();
                cursor_ = 0;
                seed_ = world.generationSeed();
                revision_ = grid.revision();
                width_ = grid.width();
                height_ = grid.height();
                for (int side = 16;; side *= 2)
                {
                    const int w = (width_ + side - 1) / side;
                    const int h = (height_ + side - 1) / side;
                    levels_.push_back(
                        {w, h, side, std::vector<Cell>(std::size_t(w) * h)}
                    );
                    if (w <= 1 && h <= 1)
                    {
                        break;
                    }
                }
            }
            auto& base = levels_.front();
            // At most 4096 source tiles per frame, regardless of world size.
            const auto end = std::min(cursor_ + 16, base.cells.size());
            while (cursor_ < end)
            {
                const int x = int(cursor_ % base.width),
                          y = int(cursor_ / base.width);
                const auto survey =
                    surveyResources(grid, {x * 16 + 8, y * 16 + 8}, 16, 16);
                lastPreparedTiles_ += std::size_t(survey.tiles);
                for (std::size_t level = 0; level < levels_.size(); ++level)
                {
                    auto& data = levels_[level];
                    auto& cell = data.cells
                                     [std::size_t(y >> level) * data.width +
                                      (x >> level)];
                    cell.tiles += survey.tiles;
                    for (std::size_t i = 0; i < cell.amounts.size(); ++i)
                    {
                        cell.amounts[i] += survey.resources[i].amount;
                    }
                }
                ++cursor_;
            }
        }

        void render(
            Renderer& renderer,
            const World& world,
            const Camera2D& camera,
            double pixels,
            bool globe,
            const SceneSpriteLibrary& art
        )
        {
            prepare(world);
            if (levels_.empty())
            {
                return;
            }
            std::size_t selected = 0;
            while (selected + 1 < levels_.size() &&
                   levels_[selected].side * pixels < 68)
            {
                ++selected;
            }
            const auto& level = levels_[selected];
            constexpr std::array ids{
                "lumber",
                "stone",
                "wheat",
                "fish",
                "meat",
                "coal",
                "iron",
                "gold"
            };
            constexpr std::array weight{1., 2., .6, .65, .4, 12., 15., 30.};
            const double localWeight =
                worldPresentationState(pixels).localWorldWeight;
            const double radius = std::hypot(
                                      double(renderer.outputWidth()),
                                      double(renderer.outputHeight())
                                  ) / std::max(.001, pixels) +
                                  level.side;
            const int firstRow =
                !globe || localWeight > .99
                    ? std::max(0, int((camera.tileY() - radius) / level.side))
                    : 0;
            const int lastRow =
                !globe || localWeight > .99
                    ? std::min(
                          level.height,
                          int((camera.tileY() + radius) / level.side) + 1
                      )
                    : level.height;
            const int columns = (renderer.outputWidth() + 63) / 64;
            const int rows = (renderer.outputHeight() + 47) / 48;
            occupied_.assign(std::size_t(columns) * rows, 0);
            for (int row = firstRow; row < lastRow; ++row)
            {
                for (int column = 0; column < level.width; ++column)
                {
                    const double x = std::min(
                        double(width_) - .5,
                        (column + .5) * level.side
                    );
                    const double y =
                        std::min(double(height_) - .5, (row + .5) * level.side);
                    double dx = x - camera.tileX();
                    if (globe)
                    {
                        dx = std::remainder(dx, double(width_));
                    }
                    if ((!globe || localWeight > .99) && std::abs(dx) > radius)
                    {
                        continue;
                    }
                    const auto point = WorldMapNavigation::annotationPosition(
                        camera,
                        world.grid(),
                        renderer.outputWidth(),
                        renderer.outputHeight(),
                        pixels,
                        globe,
                        x,
                        y
                    );
                    if (!point || point->x < 20 || point->y < 20 ||
                        point->x >= renderer.outputWidth() - 20 ||
                        point->y >= renderer.outputHeight() - 20)
                    {
                        continue;
                    }
                    const auto& cell =
                        level.cells[std::size_t(row) * level.width + column];
                    if (cell.tiles == 0)
                    {
                        continue;
                    }
                    const auto bin = std::size_t(int(point->y) / 48) * columns +
                                     int(point->x) / 64;
                    if (occupied_[bin])
                    {
                        continue;
                    }
                    std::array<std::size_t, 8> order{0, 1, 2, 3, 4, 5, 6, 7};
                    std::sort(
                        order.begin(),
                        order.end(),
                        [&](auto a, auto b)
                        {
                            const auto left = cell.amounts[a] * weight[a];
                            const auto right = cell.amounts[b] * weight[b];
                            return left == right ? a < b : left > right;
                        }
                    );
                    int count = 0;
                    for (auto index : order)
                    {
                        if (index == 2 || index == 3)
                        {
                            continue;
                        }
                        if (cell.amounts[index] <= 0 ||
                            (index < 5 &&
                             cell.amounts[index] / cell.tiles < .04))
                        {
                            continue;
                        }
                        const auto* sprite =
                            art.find("ui.goods." + std::string(ids[index]));
                        if (!sprite || !sprite->texture)
                        {
                            continue;
                        }
                        const auto frame = art.frame(*sprite, false);
                        const float px = std::round(
                            float(point->x) + (count == 0 ? -24 : 2)
                        );
                        const float py =
                            std::round(float(point->y) - frame.height * .5F);
                        renderer.fillRectangle(
                            px - 2,
                            py - 2,
                            frame.width + 4,
                            frame.height + 4,
                            {8, 15, 27, 190}
                        );
                        renderer.drawTexture(
                            *sprite->texture,
                            frame.x,
                            frame.y,
                            frame.width,
                            frame.height,
                            px,
                            py,
                            frame.width,
                            frame.height
                        );
                        if (++count == 2)
                        {
                            break;
                        }
                    }
                    occupied_[bin] = count > 0;
                }
            }
        }
    };

    class RegionResourceTooltip
    {
        ResourceSurvey survey_;
        WorldTilePosition center_{-1, -1};
        std::uint64_t seed_ = 0, revision_ = 0;
        int width_ = 0, height_ = 0;

    public:
        void render(
            Renderer& renderer,
            const GrayUiRenderer& ui,
            const World& world,
            WorldTilePosition center,
            int width,
            int height,
            float pointerX,
            float pointerY
        )
        {
            if (center != center_ || seed_ != world.generationSeed() ||
                revision_ != world.grid().revision() || width != width_ ||
                height != height_)
            {
                center_ = center;
                seed_ = world.generationSeed();
                revision_ = world.grid().revision();
                width_ = width;
                height_ = height;
                survey_ = surveyResources(world.grid(), center, width, height);
            }
            int count = 0;
            for (const auto& resource : survey_.resources)
            {
                count += resource.amount > 0;
            }
            const float w = 246, h = 52.F + count * 22;
            float x = pointerX + 20, y = pointerY + 24;
            if (x + w > renderer.outputWidth() - 8)
            {
                x = pointerX - w - 16;
            }
            if (y + h > renderer.outputHeight() - 8)
            {
                y = pointerY - h - 16;
            }
            x = std::clamp(
                x,
                8.F,
                std::max(8.F, renderer.outputWidth() - w - 8)
            );
            y = std::clamp(
                y,
                8.F,
                std::max(8.F, renderer.outputHeight() - h - 8)
            );
            ui.drawPanel(renderer, {x, y, w, h});
            ui.drawLabel(renderer, "REGION RESOURCES", x + 12, y + 12, 1.8F);
            float row = y + 38;
            for (const auto& resource : survey_.resources)
            {
                if (resource.amount <= 0)
                {
                    continue;
                }
                const auto* definition =
                    SettlementResourceCatalog::definition(resource.resource);
                ui.drawLabel(
                    renderer,
                    definition ? definition->displayName : resource.resource,
                    x + 12,
                    row,
                    1.5F
                );
                ui.drawLabel(
                    renderer,
                    std::string(resource.abundance(survey_.tiles), '+'),
                    x + 158,
                    row,
                    1.5F,
                    {121, 181, 109, 255}
                );
                row += 22;
            }
            if (count == 0)
            {
                ui.drawLabel(
                    renderer,
                    "No accessible resources",
                    x + 12,
                    row,
                    1
                );
            }
        }
    };
} // namespace Paladin

#pragma once

#include "world/settlements/SettlementMap.h"
#include <array>
#include <vector>

namespace Paladin
{
    // Exact presence reduction: even one ore tile survives every overview
    // level. Only changed 32-tile chunks are rescanned after excavation.
    class CityMineralOverview
    {
        struct Level
        {
            int width = 0, height = 0;
            std::vector<std::uint8_t> cells;
            std::vector<std::uint32_t> revisions;
        };
        struct Chunk
        {
            std::uint64_t sourceVersion = 0, presentationVersion = 0;
            bool built = false;
        };
        std::vector<Level> levels_;
        std::vector<Chunk> chunks_;
        std::uint64_t instance_ = 0;
        int columns_ = 0, rows_ = 0;
        std::size_t cursor_ = 0, examinedTiles_ = 0;
        std::uint32_t revision_ = 0;
        static constexpr std::size_t TileBudget = 4096;

        void setBase(int x, int y, std::uint8_t mask)
        {
            auto& base = levels_.front();
            auto& value = base.cells[std::size_t(y) * base.width + x];
            if (value == mask)
            {
                return;
            }
            value = mask;
            for (std::size_t index = 1; index < levels_.size(); ++index)
            {
                x /= 2;
                y /= 2;
                const auto& children = levels_[index - 1];
                auto& parent = levels_[index];
                std::uint8_t combined = 0;
                for (int dy = 0; dy < 2; ++dy)
                {
                    for (int dx = 0; dx < 2; ++dx)
                    {
                        const int cx = x * 2 + dx, cy = y * 2 + dy;
                        if (cx < children.width && cy < children.height)
                        {
                            combined |=
                                children.cells
                                    [std::size_t(cy) * children.width + cx];
                        }
                    }
                }
                auto& result = parent.cells[std::size_t(y) * parent.width + x];
                if (result == combined)
                {
                    break;
                }
                result = combined;
            }
        }

    public:
        void reset()
        {
            instance_ = 0;
            levels_.clear();
            chunks_.clear();
            cursor_ = 0;
            revision_ = 0;
        }
        void begin(const SettlementMap& map)
        {
            if (instance_ != map.instanceId())
            {
                reset();
                instance_ = map.instanceId();
                columns_ = (map.grid().width() + 31) / 32;
                rows_ = (map.grid().height() + 31) / 32;
                chunks_.resize(std::size_t(columns_) * rows_);
                int width = (map.grid().width() + 3) / 4;
                int height = (map.grid().height() + 3) / 4;
                for (;;)
                {
                    levels_.push_back(
                        {width,
                         height,
                         std::vector<std::uint8_t>(std::size_t(width) * height),
                         std::vector<std::uint32_t>(
                             std::size_t(width) * height
                         )}
                    );
                    if (width <= 1 && height <= 1)
                    {
                        break;
                    }
                    width = (width + 1) / 2;
                    height = (height + 1) / 2;
                }
            }
            examinedTiles_ = 0;
        }
        std::size_t examinedTiles() const noexcept
        {
            return examinedTiles_;
        }
        std::uint64_t touch(const SettlementMap& map, int cx, int cy)
        {
            if (cx < 0 || cy < 0 || cx >= columns_ || cy >= rows_)
            {
                return 0;
            }
            auto& chunk = chunks_[std::size_t(cy) * columns_ + cx];
            const auto revision = map.mining.depletionChunkVersion(cx, cy);
            if ((!chunk.built || chunk.sourceVersion != revision) &&
                examinedTiles_ + 1024 <= TileBudget)
            {
                const int right = std::min((cx + 1) * 32, map.grid().width());
                const int bottom = std::min((cy + 1) * 32, map.grid().height());
                for (int by = cy * 32; by < bottom; by += 4)
                {
                    for (int bx = cx * 32; bx < right; bx += 4)
                    {
                        std::uint8_t mask = 0;
                        for (int y = by; y < std::min(by + 4, bottom); ++y)
                        {
                            for (int x = bx; x < std::min(bx + 4, right); ++x)
                            {
                                ++examinedTiles_;
                                const auto& tile = *map.grid().tile({x, y});
                                if (tile.terrain != TerrainType::Water &&
                                    tile.mineral != MineralDeposit::None &&
                                    map.mining.remainingAt(
                                        {x, y},
                                        tile.mineral
                                    ) > 0)
                                {
                                    mask |= std::uint8_t(
                                        1U << (std::size_t(tile.mineral) - 1)
                                    );
                                }
                            }
                        }
                        setBase(bx / 4, by / 4, mask);
                    }
                }
                chunk.built = true;
                chunk.sourceVersion = revision;
                ++chunk.presentationVersion;
                // A depleted column can leave the same coarse presence mask
                // if adjacent ore remains. Its close-view page must still
                // refresh, so invalidation follows the changed chunk itself.
                const auto presentation = ++revision_;
                int side = 4;
                for (auto& level : levels_)
                {
                    if (side >= 32 || &level == &levels_.back())
                    {
                        level.revisions
                            [std::size_t(cy * 32 / side) * level.width +
                             cx * 32 / side] = presentation;
                    }
                    side *= 2;
                }
            }
            return chunk.presentationVersion;
        }
        void advance(const SettlementMap& map)
        {
            // The bounded audit also notices off-screen depleted chunks; no
            // full-map revision comparison runs during a render frame.
            for (int inspected = 0; inspected < 64 && !chunks_.empty() &&
                                    examinedTiles_ + 1024 <= TileBudget;
                 ++inspected)
            {
                cursor_ %= chunks_.size();
                const int x = int(cursor_ % columns_),
                          y = int(cursor_ / columns_);
                ++cursor_;
                static_cast<void>(touch(map, x, y));
            }
        }
        std::uint8_t maskAt(int x, int y, int stride) const noexcept
        {
            if (levels_.empty() || x < 0 || y < 0)
            {
                return 0;
            }
            std::size_t index = 0;
            int side = 4;
            while (index + 1 < levels_.size() && side < stride)
            {
                ++index;
                side *= 2;
            }
            const auto& level = levels_[index];
            const int cx = x / side, cy = y / side;
            return cx < level.width && cy < level.height
                       ? level.cells[std::size_t(cy) * level.width + cx]
                       : 0;
        }
        std::uint32_t revisionAt(int x, int y, int span) const noexcept
        {
            if (levels_.empty() || x < 0 || y < 0)
            {
                return 0;
            }
            std::size_t index = 0;
            int side = 4;
            while (index + 1 < levels_.size() && side < span)
            {
                ++index;
                side *= 2;
            }
            const auto& level = levels_[index];
            const int cx = x / side, cy = y / side;
            return cx < level.width && cy < level.height
                       ? level.revisions[std::size_t(cy) * level.width + cx]
                       : 0;
        }
    };
} // namespace Paladin

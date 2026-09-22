#pragma once
#include "world/SettlementGrid.h"
#include "world/generation/GenerationNoise.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace Paladin
{
    // Topology lives at range scale. Small noise belongs to rock textures and
    // boundary chips, never a per-tile mask punching holes through a range.
    class SettlementRelief
    {
    public:
        SettlementRelief(int width, int height, std::uint64_t seed)
            : width_(width), height_(height), seed_(seed),
              sideways_((seed & 1) != 0)
        {
        }

        bool valley(int x, int y) const noexcept
        {
            const double across = sideways_ ? y : x;
            const double along = sideways_ ? x : y;
            const double extent = sideways_ ? height_ : width_;
            const double length = sideways_ ? width_ : height_;
            const int count = std::clamp(int(extent / 240), 1, 3);
            const double spacing = extent / (count + 1);
            for (int i = 0; i < count; ++i)
            {
                const double phase = double((seed_ >> (8 + i * 8)) & 255) * .024;
                const double t = along / length;
                const double bend = spacing * .23 * std::sin(t * 5.0 + phase) +
                    spacing * .055 * std::sin(t * 11.0 + phase);
                const double center = spacing * (i + 1) + bend;
                const double halfWidth = 3.0 + 1.25 * (1.0 + std::sin(t * 9.0 + phase));
                if (std::abs(across - center) <= halfWidth) { return true; }
            }
            // An oblique, winding connector, rather than a straight cross
            // dividing the map into rectangular mountain blocks.
            const double phase = double(seed_ & 255) * .02;
            const double bend = length * .065 * std::sin(across / extent * 7 + phase);
            const double center = length * .52 + (across / extent - .5) * length * .22 + bend;
            return std::abs(along - center) <= 3.0 + .75 * (1 + std::sin(across / 53 + phase));
        }

        void caves(SettlementGrid& grid) const
        {
            constexpr std::array<SettlementTilePosition, 4> steps{
                {{1, 0}, {0, 1}, {-1, 0}, {0, -1}}};
            const auto solid = [&](SettlementTilePosition p)
            {
                const auto* t = grid.tile(p);
                return t && t->terrain == TerrainType::Mountain &&
                       t->relief == ReliefType::Mountain;
            };
            // At most one short cave per 128x128 district, reached from an
            // existing valley/lowland. No random chambers isolated in a range.
            constexpr int district = 128;
            for (int by = 0; by < height_; by += district)
            for (int bx = 0; bx < width_; bx += district)
            {
                auto hash = GenerationNoise::mix(seed_ ^
                    std::uint64_t(bx) * 73856093ULL ^
                    std::uint64_t(by) * 19349663ULL);
                if (hash % 3 == 0) { continue; }
                const int w = std::min(district, width_ - bx);
                const int h = std::min(district, height_ - by);
                bool carved = false;
                const int offset = int((hash >> 8) % (w * h));
                for (int i = 0; i < w * h && !carved; ++i)
                {
                    const int cell = (offset + i) % (w * h);
                    SettlementTilePosition entry{bx + cell % w, by + cell / w};
                    if (!solid(entry)) { continue; }
                    for (const auto dir : steps)
                    {
                        const SettlementTilePosition outside{entry.x - dir.x, entry.y - dir.y};
                        const auto* approach = grid.tile(outside);
                        if (!approach || approach->terrain != TerrainType::Land ||
                            approach->rockFloor) { continue; }
                        bool deep = true;
                        for (int s = 1; s <= 8; ++s)
                        {
                            deep &= solid({entry.x + dir.x * s, entry.y + dir.y * s});
                        }
                        if (!deep) { continue; }
                        auto p = entry;
                        const int length = 18 + int((hash >> 24) % 19);
                        const SettlementTilePosition side{-dir.y, dir.x};
                        for (int s = 0; s < length; ++s)
                        {
                            if (!solid(p) || !solid({p.x + dir.x * 2, p.y + dir.y * 2}))
                            { break; }
                            auto* floor = grid.tile(p);
                            floor->terrain = TerrainType::Land;
                            floor->rockFloor = true;
                            // Rare side chamber, never another exposed valley.
                            if (s > 7 && s % 11 == 0 &&
                                solid({p.x + side.x * 2, p.y + side.y * 2}))
                            {
                                auto* chamber = grid.tile({p.x + side.x, p.y + side.y});
                                if (chamber && chamber->terrain == TerrainType::Mountain)
                                {
                                    chamber->terrain = TerrainType::Land;
                                    chamber->rockFloor = true;
                                }
                            }
                            p.x += dir.x;
                            p.y += dir.y;
                        }
                        carved = true;
                        break;
                    }
                }
            }
        }

    private:
        int width_, height_;
        std::uint64_t seed_;
        bool sideways_;
    };
}

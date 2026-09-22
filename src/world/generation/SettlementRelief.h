#pragma once
#include "world/SettlementGrid.h"
#include "world/generation/GenerationNoise.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Paladin
{
    // Topology lives at range scale. Small noise belongs to rock textures and
    // boundary chips, never a per-tile mask punching holes through a range.
    class SettlementRelief
    {
    public:
        SettlementRelief(int width, int height, std::uint64_t seed)
            : width_(width), height_(height), seed_(seed)
        {
        }

        ReliefType classify(int x, int y, double mountain, double hills) const noexcept
        {
            if (mountain < .18 && hills < .18) { return ReliefType::Lowland; }
            const double scale = std::max(64.0, std::min(width_, height_) * .36);
            const double angle = double(seed_ % 1009) * .006227;
            const double cs = std::cos(angle), sn = std::sin(angle);
            const double u = (x * cs + y * sn) / scale;
            const double v = (y * cs - x * sn) / scale;
            const double wx = GenerationNoise::simplexFractal(u * .55, v * .55, seed_ + 7907, 2, .4, 2);
            const double wy = GenerationNoise::simplexFractal(u * .55, v * .55, seed_ + 3911, 2, .4, 2);
            const double a = u + wx * .8, b = v + wy * .8;
            const double massif = GenerationNoise::simplexFractal(a * .72, b, seed_ + 8191, 2, .3, 2);
            const double threshold = -.10 + (1.0 - mountain) * .72;
            if (mountain >= .18 && massif > threshold)
            {
                return ReliefType::Mountain;
            }
            // Foothills follow a massif's contour. Independent hill country
            // uses smaller rounded landforms, not scaled-down mountain stripes.
            if (mountain >= .35 && massif > threshold - .12) { return ReliefType::Hills; }
            const double hill = GenerationNoise::simplexFractal(a * 1.8, b * 2.0, seed_ + 4561, 2, .25, 2);
            if (hills >= .18 && hill > .16 + (1.0 - hills) * .55)
            { return ReliefType::Hills; }
            // Gaps are the actual low basins between landforms. Nothing carves
            // an obligatory constant-width channel across the entire map.
            return ReliefType::Lowland;
        }

        void consolidate(SettlementGrid& grid) const
        {
            constexpr SettlementTilePosition steps[]{{1,0},{0,1},{-1,0},{0,-1}};
            std::vector<std::uint8_t> seen(grid.tileCount());
            std::vector<SettlementTilePosition> component;
            for (int y = 0; y < height_; ++y) for (int x = 0; x < width_; ++x)
            {
                const auto index = std::size_t(y) * width_ + x;
                if (seen[index] || grid.tile({x,y})->terrain == TerrainType::Water) { continue; }
                const bool rock = grid.tile({x,y})->terrain == TerrainType::Mountain;
                bool open = false;
                component.clear(); component.push_back({x,y}); seen[index] = 1;
                for (std::size_t i = 0; i < component.size(); ++i)
                {
                    for (auto d : steps)
                    {
                        const SettlementTilePosition p{component[i].x+d.x,component[i].y+d.y};
                        const auto* tile = grid.tile(p);
                        if (!tile || tile->terrain == TerrainType::Water) { open = true; continue; }
                        if ((tile->terrain == TerrainType::Mountain) != rock) { continue; }
                        const auto at = std::size_t(p.y) * width_ + p.x;
                        if (!seen[at]) { seen[at] = 1; component.push_back(p); }
                    }
                }
                const bool remove = rock && component.size() < 48;
                const bool fill = !rock && !open && component.size() < 20;
                if (!remove && !fill) { continue; }
                for (auto p : component)
                {
                    auto& tile = *grid.tile(p);
                    tile.terrain = fill ? TerrainType::Mountain : TerrainType::Land;
                    tile.relief = fill ? ReliefType::Hills : ReliefType::Lowland;
                }
            }
        }

        void caves(SettlementGrid& grid) const
        {
            constexpr std::array<SettlementTilePosition, 4> steps{
                {{1, 0}, {0, 1}, {-1, 0}, {0, -1}}};
            const auto solid = [&](SettlementTilePosition p)
            {
                const auto* t = grid.tile(p);
                return t && t->terrain == TerrainType::Mountain;
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
                        // A mountain's foothills are solid too. Enter through
                        // that apron instead of requiring lowland to touch a
                        // high peak directly. Isolated hillocks get no tunnels.
                        bool mountainCore = false;
                        for (int s = 0; s <= 32; ++s)
                        {
                            const auto* core = grid.tile({entry.x+dir.x*s,entry.y+dir.y*s});
                            if (!core || core->terrain != TerrainType::Mountain) { break; }
                            mountainCore |= core->relief == ReliefType::Mountain;
                        }
                        if (!mountainCore) { continue; }
                        auto p = entry;
                        const int length = 18 + int((hash >> 24) % 19);
                        const SettlementTilePosition side{-dir.y, dir.x};
                        for (int s = 0; s < length; ++s)
                        {
                            const auto* center = grid.tile(p);
                            if (!center || (!solid(p) && !center->rockFloor) ||
                                !solid({p.x + dir.x * 3, p.y + dir.y * 3}))
                            { break; }
                            // Narrow entrance, a bending passage, then occasional
                            // small chambers. Every floor cell joins the mouth.
                            const int radius = s < 4 ? 0 : (s > 8 && s % 11 < 3 ? 2 : 1);
                            for (int oy=-radius; oy<=radius; ++oy)
                            for (int ox=-radius; ox<=radius; ++ox)
                            {
                                if (ox*ox + oy*oy > radius*radius) { continue; }
                                const SettlementTilePosition at{p.x+ox,p.y+oy};
                                if (solid(at))
                                {
                                    auto* floor = grid.tile(at);
                                    floor->terrain = TerrainType::Land;
                                    floor->rockFloor = true;
                                }
                            }
                            if (s > 3 && s % 5 == 0)
                            {
                                const int turn = (GenerationNoise::mix(hash + s) & 1) ? 1 : -1;
                                const SettlementTilePosition next{p.x+side.x*turn,p.y+side.y*turn};
                                if (solid({next.x+dir.x*3,next.y+dir.y*3}))
                                {
                                    if (auto* floor = grid.tile(next); floor && floor->terrain != TerrainType::Water)
                                    { floor->terrain = TerrainType::Land; floor->rockFloor = true; p = next; }
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
    };
}

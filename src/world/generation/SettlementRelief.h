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
            : width_(width), height_(height), seed_(seed),
              scale_(std::max(64.0, std::min(width, height) * .36)),
              cosine_(std::cos(double(seed % 1009) * .006227)),
              sine_(std::sin(double(seed % 1009) * .006227))
        {
        }

        ReliefType classify(int x, int y, double mountain, double hills) const noexcept
        {
            if (mountain < .18 && hills < .18) { return ReliefType::Lowland; }
            mountain = std::clamp(mountain, 0.0, 1.0);
            hills = std::clamp(hills, 0.0, 1.0);
            const double u = (x * cosine_ + y * sine_) / scale_;
            const double v = (y * cosine_ - x * sine_) / scale_;
            const double wx = GenerationNoise::simplexFractal(u * .55, v * .55, seed_ + 7907, 2, .4, 2);
            const double wy = GenerationNoise::simplexFractal(u * .55, v * .55, seed_ + 3911, 2, .4, 2);
            const double a = u + wx * .8, b = v + wy * .8;
            // Broad bodies determine topology; subordinate spurs roughen the
            // shoulders without punching noisy holes through the interior.
            const double spurs = GenerationNoise::simplexFractal(
                a * 2.3, b * 1.3, seed_ + 6073, 2, .25, 2);
            const double massif = GenerationNoise::simplexFractal(
                a * .72, b, seed_ + 8191, 2, .3, 2) + spurs * .11;
            const double threshold = -.10 + (1.0 - mountain) * .72;
            if (mountain >= .18 && massif > threshold)
            {
                return ReliefType::Mountain;
            }
            // Foothills follow a massif's contour. Independent hill country
            // uses smaller rounded landforms, not scaled-down mountain stripes.
            const double apron = .075 + .075 * std::clamp(.5 + spurs, 0.0, 1.0);
            if (mountain >= .35 && massif > threshold - apron) { return ReliefType::Hills; }
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
                bool open = false, highRim = false;
                component.clear(); component.push_back({x,y}); seen[index] = 1;
                for (std::size_t i = 0; i < component.size(); ++i)
                {
                    for (auto d : steps)
                    {
                        const SettlementTilePosition p{component[i].x+d.x,component[i].y+d.y};
                        const auto* tile = grid.tile(p);
                        if (!tile || tile->terrain == TerrainType::Water) { open = true; continue; }
                        if ((tile->terrain == TerrainType::Mountain) != rock)
                        {
                            highRim |= tile->terrain == TerrainType::Mountain &&
                                       tile->relief == ReliefType::Mountain;
                            continue;
                        }
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
                    // A sealed pocket inherits its surrounding massif instead
                    // of leaving an isolated hill-colored patch in solid rock.
                    tile.relief = fill ? (highRim ? ReliefType::Mountain : ReliefType::Hills)
                                       : ReliefType::Lowland;
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
            const auto index = [&](SettlementTilePosition p)
            { return std::size_t(p.y) * width_ + p.x; };
            // Keep cave walls inside the original massif. Measuring the rock
            // apron once also prevents a chamber from opening onto a nearby
            // valley or lake as its centreline bends through a narrow shoulder.
            std::vector<std::uint8_t> depth(grid.tileCount(), 8);
            std::vector<SettlementTilePosition> edge;
            for (int y = 0; y < height_; ++y)
            for (int x = 0; x < width_; ++x)
            {
                const SettlementTilePosition p{x,y};
                if (!solid(p)) { depth[index(p)] = 0; continue; }
                for (const auto d : steps)
                {
                    if (!solid({x+d.x,y+d.y}))
                    { depth[index(p)] = 1; edge.push_back(p); break; }
                }
            }
            for (std::size_t cursor = 0; cursor < edge.size(); ++cursor)
            {
                const auto p = edge[cursor];
                const int nextDepth = depth[index(p)] + 1;
                if (nextDepth > 6) { continue; }
                for (const auto d : steps)
                {
                    const SettlementTilePosition at{p.x+d.x,p.y+d.y};
                    if (solid(at) && depth[index(at)] > nextDepth)
                    {
                        depth[index(at)] = std::uint8_t(nextDepth);
                        edge.push_back(at);
                    }
                }
            }
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
                        std::vector<SettlementTilePosition> passage{entry};
                        auto heading = dir;
                        const int length = 14 + int((hash >> 24) % 17);
                        for (int s = 1; s < length; ++s)
                        {
                            const auto p = passage.back();
                            const std::array<SettlementTilePosition,3> choices{
                                heading, SettlementTilePosition{-heading.y,heading.x},
                                SettlementTilePosition{heading.y,-heading.x}};
                            int best = -1, bestScore = -1;
                            for (int k = 0; k < (s < 4 ? 1 : 3); ++k)
                            {
                                const auto d = choices[k];
                                const SettlementTilePosition at{p.x+d.x,p.y+d.y};
                                if (!solid(at) || depth[index(at)] < (s < 3 ? 1 : 3))
                                { continue; }
                                bool loops = false;
                                for (std::size_t old = 0; old + 2 < passage.size(); ++old)
                                {
                                    if (std::abs(at.x-passage[old].x) +
                                        std::abs(at.y-passage[old].y) <= 1)
                                    { loops = true; break; }
                                }
                                if (loops) { continue; }
                                const int score = std::min(5, int(depth[index(at)])) * 4 +
                                    (k == 0 ? 7 : 0) + int(GenerationNoise::mix(
                                        hash + std::uint64_t(s/4)*104729 + k*7919) % 17);
                                if (score > bestScore) { best = k; bestScore = score; }
                            }
                            if (best < 0) { break; }
                            heading = choices[best];
                            passage.push_back({p.x+heading.x,p.y+heading.y});
                        }
                        if (passage.size() < 6) { continue; }
                        // Plan first, excavate second: previous cells do not
                        // bias the route toward the next district's cave. Every
                        // step is cardinally connected and has a rock roof/wall.
                        for (std::size_t s = 0; s < passage.size(); ++s)
                        {
                            const auto p = passage[s];
                            const bool chamber = s > 7 &&
                                (GenerationNoise::mix(hash + s/5) % 3 == 0);
                            const int desired = s < 4 ? 0 : chamber ? 2 : 1;
                            const int radius = std::max(0,
                                std::min(desired, int(depth[index(p)]) - 2));
                            for (int oy=-radius; oy<=radius; ++oy)
                            for (int ox=-radius; ox<=radius; ++ox)
                            {
                                if (ox*ox + oy*oy > radius*radius) { continue; }
                                const SettlementTilePosition at{p.x+ox,p.y+oy};
                                if (solid(at) && (s < 4 || depth[index(at)] >= 2))
                                {
                                    auto* floor = grid.tile(at);
                                    floor->terrain = TerrainType::Land;
                                    floor->rockFloor = true;
                                }
                            }
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
        double scale_, cosine_, sine_;
    };
}

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
              scale_(std::max(48.0, std::min(width, height) * .32)),
              mountains_(width, height), shoulders_(width, height),
              hills_(width, height)
        {
            // Compact, overlapping fractured blocks make the mass first. There
            // is no ridge spline, contour stripe or mandatory valley to carve.
            const double pitch = scale_ * 1.6;
            for (int cy = -1; cy <= int(height / pitch); ++cy)
            for (int cx = -1; cx <= int(width / pitch); ++cx)
            {
                const auto key = cellKey(cx, cy, seed_ + 8191);
                if (key % 13 == 0) { continue; }
                const double x = (cx + .15 + random(key, 1) * .70) * pitch;
                const double y = (cy + .15 + random(key, 2) * .70) * pitch;
                const double radius = scale_ * (.48 + random(key, 3) * .25);
                cluster(mountains_, x, y, radius, key, 3 + int(key % 3));

                // Talus shoulders occupy selected sides, not an even collar
                // tracing every mountain. Other sides end in exposed cliffs.
                for (int i = 0; i < 1 + int((key >> 12) % 3); ++i)
                {
                    const double angle = random(key, 20 + i) * tau;
                    const double reach = radius * (.88 + random(key, 30 + i) * .38);
                    cluster(shoulders_, x + std::cos(angle) * reach,
                        y + std::sin(angle) * reach,
                        radius * (.22 + random(key, 40 + i) * .20),
                        GenerationNoise::mix(key + 50 + i), 2);
                }
            }

            // Hill country has its own smaller, intermittently grouped nuclei.
            // It is not a threshold band around the mountain field.
            const double hillPitch = scale_ * .72;
            for (int cy = -1; cy <= int(height / hillPitch); ++cy)
            for (int cx = -1; cx <= int(width / hillPitch); ++cx)
            {
                const auto key = cellKey(cx, cy, seed_ + 4561);
                if (key % 5 == 0) { continue; }
                const double x = (cx + .10 + random(key, 1) * .80) * hillPitch;
                const double y = (cy + .10 + random(key, 2) * .80) * hillPitch;
                cluster(hills_, x, y, scale_ * (.15 + random(key, 3) * .13),
                        key, 2 + int(key % 2));
            }
        }

        ReliefType classify(int x, int y, double mountain, double hills) const noexcept
        {
            if (mountain < .18 && hills < .18) { return ReliefType::Lowland; }
            mountain = std::clamp(mountain, 0.0, 1.0);
            hills = std::clamp(hills, 0.0, 1.0);
            const double px = x + .5, py = y + .5;
            // Piecewise-planar erosion preserves broken faces at three scales.
            // Its bounded amplitude only bites the perimeter, leaving deep rock
            // intact instead of perforating it with tile-sized noise.
            const double chips = scale_ * (
                .075 * fracture(px, py, scale_ * .22, seed_ + 6073) +
                .048 * fracture(px, py, scale_ * .085, seed_ + 7907) +
                .018 * fracture(px, py, scale_ * .033, seed_ + 3911));
            if (mountain >= .18 && mountains_.sample(px, py) >
                (1.0 - mountain) * scale_ * .62 + chips)
            {
                return ReliefType::Mountain;
            }
            if (mountain >= .35 && shoulders_.sample(px, py) >
                (1.0 - mountain) * scale_ * .25 + chips * .40)
            { return ReliefType::Hills; }
            if (hills >= .18 && hills_.sample(px, py) >
                (1.0 - hills) * scale_ * .24 + chips * .48)
            { return ReliefType::Hills; }
            // Gaps are the actual low basins between landforms. Nothing carves
            // an obligatory constant-width channel across the entire map.
            return ReliefType::Lowland;
        }

        void consolidate(SettlementGrid& grid, bool afterCaves = false) const
        {
            constexpr SettlementTilePosition steps[]{{1,0},{0,1},{-1,0},{0,-1}};
            std::vector<std::uint8_t> seen(grid.tileCount());
            std::vector<SettlementTilePosition> component;
            for (int y = 0; y < height_; ++y) for (int x = 0; x < width_; ++x)
            {
                const auto index = std::size_t(y) * width_ + x;
                if (seen[index] || grid.tile({x,y})->terrain == TerrainType::Water) { continue; }
                const bool rock = grid.tile({x,y})->terrain == TerrainType::Mountain;
                bool open = false, highRim = false, caveRim = false;
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
                            caveRim |= tile->rockFloor;
                            highRim |= tile->terrain == TerrainType::Mountain &&
                                       tile->relief == ReliefType::Mountain;
                            continue;
                        }
                        const auto at = std::size_t(p.y) * width_ + p.x;
                        if (!seen[at]) { seen[at] = 1; component.push_back(p); }
                    }
                }
                const bool remove = rock && component.size() < 48 && (!afterCaves || caveRim);
                // Excavation may sever a thin shoulder into tiny pillars. Clear
                // those remnants as cave floor, but never refill cave passages.
                const bool fill = !afterCaves && !rock && !open && component.size() < 20;
                if (!remove && !fill) { continue; }
                for (auto p : component)
                {
                    auto& tile = *grid.tile(p);
                    tile.terrain = fill ? TerrainType::Mountain : TerrainType::Land;
                    if (afterCaves)
                    {
                        tile.rockFloor = true;
                        continue;
                    }
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
        static constexpr double tau = 6.2831853071795864769;

        struct Face { double x, y, distance; };
        struct Block
        {
            double x, y, bounds;
            std::array<Face, 8> faces;
        };

        // Spatial bins are built once during generation. A tile only examines
        // nearby blocks, with no per-tile allocations or all-map shape scans.
        struct Field
        {
            static constexpr int binSize = 32;
            int columns, rows;
            std::vector<Block> blocks;
            std::vector<std::vector<std::size_t>> bins;

            Field(int width, int height)
                : columns((width + binSize - 1) / binSize),
                  rows((height + binSize - 1) / binSize),
                  bins(std::size_t(columns) * rows) {}

            void add(Block block)
            {
                const int left = std::max(0, int(std::floor((block.x-block.bounds)/binSize)));
                const int top = std::max(0, int(std::floor((block.y-block.bounds)/binSize)));
                const int right = std::min(columns-1, int(std::floor((block.x+block.bounds)/binSize)));
                const int bottom = std::min(rows-1, int(std::floor((block.y+block.bounds)/binSize)));
                if (left > right || top > bottom) { return; }
                const auto index = blocks.size();
                blocks.push_back(block);
                for (int y = top; y <= bottom; ++y)
                for (int x = left; x <= right; ++x)
                { bins[std::size_t(y)*columns+x].push_back(index); }
            }

            double sample(double x, double y) const noexcept
            {
                double result = -1e9;
                const auto bin = std::size_t(int(y)/binSize)*columns + int(x)/binSize;
                for (const auto index : bins[bin])
                {
                    const auto& block = blocks[index];
                    const double dx = x-block.x, dy = y-block.y;
                    if (std::abs(dx) > block.bounds || std::abs(dy) > block.bounds) { continue; }
                    double inside = 1e9;
                    for (const auto& face : block.faces)
                    { inside = std::min(inside, face.distance - dx*face.x - dy*face.y); }
                    result = std::max(result, inside);
                }
                return result;
            }
        };

        static std::uint64_t cellKey(int x, int y, std::uint64_t seed) noexcept
        {
            return GenerationNoise::mix(seed ^
                std::uint64_t(std::uint32_t(x)) * 0x9E3779B185EBCA87ULL ^
                std::uint64_t(std::uint32_t(y)) * 0xC2B2AE3D27D4EB4FULL);
        }

        static double random(std::uint64_t key, int channel) noexcept
        {
            return double(GenerationNoise::mix(key + std::uint64_t(channel) *
                0x9E3779B185EBCA87ULL) >> 11) * (1.0 / 9007199254740992.0);
        }

        static double fracture(double x, double y, double size, std::uint64_t seed) noexcept
        {
            // Rotate away from the storage lattice. Linear triangular samples
            // produce chipped shoulders rather than smoothly curving lobes.
            const double u = (x * .913 + y * .408) / size;
            const double v = (y * .913 - x * .408) / size;
            const int ix = int(std::floor(u)), iy = int(std::floor(v));
            const double a = u-ix, b = v-iy;
            const auto value = [&](int dx, int dy)
            { return random(cellKey(ix+dx, iy+dy, seed), 0)*2-1; };
            if (a+b <= 1)
            { return value(0,0)*(1-a-b) + value(1,0)*a + value(0,1)*b; }
            return value(1,1)*(a+b-1) + value(0,1)*(1-a) + value(1,0)*(1-b);
        }

        void block(Field& field, double x, double y, double radius, std::uint64_t key)
        {
            Block shape{x, y, radius*1.65 + scale_*.12, {}};
            const double rotation = random(key, 70)*tau;
            for (int i = 0; i < int(shape.faces.size()); ++i)
            {
                const double angle = rotation + (i + random(key, 80+i)*.30)*tau/8;
                shape.faces[i] = {std::cos(angle), std::sin(angle),
                                  radius*(.80 + random(key, 90+i)*.35)};
            }
            field.add(shape);
        }

        void cluster(Field& field, double x, double y, double radius,
                     std::uint64_t key, int count)
        {
            block(field, x, y, radius, key);
            for (int i = 0; i < count; ++i)
            {
                const double angle = random(key, 100+i)*tau;
                const double offset = radius*(.52 + random(key, 110+i)*.45);
                block(field, x+std::cos(angle)*offset, y+std::sin(angle)*offset,
                      radius*(.34 + random(key, 120+i)*.29),
                      GenerationNoise::mix(key+130+i));
            }
        }

        int width_, height_;
        std::uint64_t seed_;
        double scale_;
        Field mountains_, shoulders_, hills_;
    };
}

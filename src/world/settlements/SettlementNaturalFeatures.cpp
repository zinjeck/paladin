#include "world/settlements/SettlementNaturalFeatures.h"
#include "world/generation/GenerationNoise.h"
#include <algorithm>
#include <cmath>

namespace Paladin
{
    namespace
    {
        double smooth(double low, double high, double value)
        {
            const double t = std::clamp((value - low) / (high - low), 0.0, 1.0);
            return t * t * (3.0 - 2.0 * t);
        }
        double roll(std::uint64_t seed, int x, int y, std::uint64_t salt)
        {
            return (GenerationNoise::mix(
                        seed ^ (std::uint64_t(x) << 32) ^ std::uint64_t(y) ^
                        salt
                    ) >>
                    11) *
                   0x1.0p-53;
        }
    } // namespace
    SettlementNaturalFeatures::SettlementNaturalFeatures(int width, int height)
        : width_(width), height_(height),
          chunkColumns_((width + ChunkSide - 1) / ChunkSide),
          features_(std::size_t(width) * height),
          versions_(
              std::size_t(chunkColumns_) *
                  ((height + ChunkSide - 1) / ChunkSide),
              1
          )
    {
    }
    bool SettlementNaturalFeatures::valid(
        SettlementTilePosition p
    ) const noexcept
    {
        return p.x >= 0 && p.y >= 0 && p.x < width_ && p.y < height_;
    }
    NaturalFeature SettlementNaturalFeatures::at(
        SettlementTilePosition p
    ) const noexcept
    {
        return valid(p) ? features_[std::size_t(p.y) * width_ + p.x]
                        : NaturalFeature{};
    }
    void SettlementNaturalFeatures::changed(SettlementTilePosition p)
    {
        ++version_;
        ++versions_
            [std::size_t(p.y / ChunkSide) * chunkColumns_ + p.x / ChunkSide];
    }
    void SettlementNaturalFeatures::set(
        SettlementTilePosition p,
        NaturalFeatureKind kind
    )
    {
        if (!valid(p))
        {
            return;
        }
        auto& feature = features_[std::size_t(p.y) * width_ + p.x];
        if (feature.kind == kind)
        {
            return;
        }
        if (chunkCounts_.empty())
        {
            chunkCounts_.resize(versions_.size());
            overviewCounts_.resize(
                std::size_t((width_ + OverviewSide - 1) / OverviewSide) *
                ((height_ + OverviewSide - 1) / OverviewSide)
            );
        }
        auto& summary = overviewCounts_
            [std::size_t(p.y / OverviewSide) *
                 ((width_ + OverviewSide - 1) / OverviewSide) +
             p.x / OverviewSide];
        if (feature.kind != NaturalFeatureKind::None)
        {
            --summary[int(feature.kind) - 1];
        }
        if (kind != NaturalFeatureKind::None)
        {
            ++summary[int(kind) - 1];
        }
        chunkCounts_
            [std::size_t(p.y / ChunkSide) * chunkColumns_ + p.x / ChunkSide] +=
            int(kind != NaturalFeatureKind::None) -
            int(feature.kind != NaturalFeatureKind::None);
        feature = {kind, false};
        changed(p);
    }
    void SettlementNaturalFeatures::harvest(
        SettlementTilePosition p,
        double minute
    )
    {
        if (at(p).kind == NaturalFeatureKind::Tree)
        {
            const auto seed =
                std::uint32_t(p.x) * 73856093u ^ std::uint32_t(p.y) * 19349663u;
            regrowth_.emplace(minute + (12 + seed % 9) * 1440.0, p);
        }
        set(p, NaturalFeatureKind::None);
    }
    void SettlementNaturalFeatures::regrow(
        const SettlementGrid& grid,
        const SettlementObjectState& objects,
        double minute
    )
    {
        // Only visit due stumps, not every tile each simulation minute.
        while (!regrowth_.empty() && regrowth_.begin()->first <= minute)
        {
            const auto p = regrowth_.begin()->second;
            regrowth_.erase(regrowth_.begin());
            const auto* tile = grid.tile(p);
            if (tile && tile->terrain == TerrainType::Land &&
                !objects.completedObjectAt(p) &&
                !objects.constructionSiteAt(p) &&
                at(p).kind == NaturalFeatureKind::None)
            {
                set(p, NaturalFeatureKind::Tree);
            }
        }
    }
    void SettlementNaturalFeatures::mark(SettlementTilePosition p, bool marked)
    {
        if (!valid(p))
        {
            return;
        }
        auto& feature = features_[std::size_t(p.y) * width_ + p.x];
        if (feature.marked == marked)
        {
            return;
        }
        feature.marked = marked;
        changed(p);
    }
    void SettlementNaturalFeatures::clear(
        const SettlementObjectFootprint& footprint
    )
    {
        for (int y = std::max(0, footprint.topLeft.y);
             y < std::min(height_, footprint.topLeft.y + footprint.height);
             ++y)
        {
            for (int x = std::max(0, footprint.topLeft.x);
                 x < std::min(width_, footprint.topLeft.x + footprint.width);
                 ++x)
            {
                set({x, y}, NaturalFeatureKind::None);
            }
        }
    }
    std::uint64_t SettlementNaturalFeatures::chunkVersion(
        int x,
        int y
    ) const noexcept
    {
        return versions_[std::size_t(y) * chunkColumns_ + x];
    }
    void SettlementNaturalFeatures::generate(
        const SettlementGrid& grid,
        std::uint64_t seed,
        const NaturalFeatureGenerationPolicy& policy
    )
    {
        for (int y = 0; y < height_; ++y)
        {
            for (int x = 0; x < width_; ++x)
            {
                const auto& tile = *grid.tile({x, y});
                NaturalFeatureKind kind = NaturalFeatureKind::None;
                const auto entry = std::find_if(
                    policy.biomes.begin(),
                    policy.biomes.end(),
                    [&](const auto& item) { return item.biome == tile.biome; }
                );
                if (tile.terrain == TerrainType::Land &&
                    grid.cityTileType({x, y}) != CityTileType::Beach &&
                    entry != policy.biomes.end())
                {
                    const auto noise =
                        [&](double frequency, std::uint64_t salt, int octaves)
                    {
                        return (GenerationNoise::simplexFractal(
                                    x * frequency,
                                    y * frequency,
                                    seed + salt,
                                    octaves
                                ) +
                                1.0) *
                               .5;
                    };
                    const double patch = noise(policy.treeFrequency, 104729, 3);
                    double chance =
                        patch <= entry->clusterStart
                            ? 0.0
                            : entry->treeChance * std::lerp(
                                                      entry->edgeOccupancy,
                                                      1.0,
                                                      smooth(
                                                          entry->clusterStart,
                                                          entry->clusterFull,
                                                          patch
                                                      )
                                                  );
                    chance *=
                        std::clamp(
                            1.12 - std::abs(
                                       tile.temperature.value() -
                                       entry->preferredTemperature
                                   ) * .55,
                            .80,
                            1.12
                        ) *
                        std::lerp(.78, 1.18, double(tile.rainfall.value()));
                    const double reservedChance = chance;
                    if (entry->denseForest)
                    {
                        chance *= std::lerp(
                            .34,
                            1.0,
                            smooth(
                                .18,
                                .36,
                                noise(policy.clearingFrequency, 117877, 2)
                            )
                        );
                    }
                    const double treeRoll = roll(seed, x, y, 101);
                    if (treeRoll < std::clamp(chance, 0.0, .72))
                    {
                        kind = NaturalFeatureKind::Tree;
                    }
                    else
                    {
                        double rockChance =
                            entry->rockChance +
                            entry->rockClusterChance *
                                smooth(
                                    policy.rockClusterStart,
                                    policy.rockClusterFull,
                                    noise(policy.rockFrequency, 130363, 2)
                                );
                        if (treeRoll < reservedChance)
                        {
                            rockChance *= .35;
                        }
                        if (roll(seed, x, y, 211) < rockChance)
                        {
                            kind = NaturalFeatureKind::Rock;
                        }
                    }
                }
                set({x, y}, kind);
            }
        }
    }
    std::optional<SettlementTilePosition> SettlementNaturalFeatures::nextIn(
        const SettlementObjectFootprint& f,
        std::size_t& cursor,
        std::size_t& budget
    ) const noexcept
    {
        const int left = std::max(0, f.topLeft.x),
                  top = std::max(0, f.topLeft.y);
        const int right = std::min(width_, f.topLeft.x + f.width);
        const int bottom = std::min(height_, f.topLeft.y + f.height);
        if (left >= right || top >= bottom || chunkCounts_.empty())
        {
            return {};
        }
        const int firstX = left / ChunkSide, firstY = top / ChunkSide;
        const int columns = (right - 1) / ChunkSide - firstX + 1;
        const int rows = (bottom - 1) / ChunkSide - firstY + 1;
        constexpr std::size_t chunkArea = ChunkSide * ChunkSide;
        const std::size_t extent = std::size_t(columns) * rows * chunkArea;
        std::size_t scanned = 0;
        while (budget > 0 && scanned < extent)
        {
            --budget;
            cursor %= extent;
            const auto chunk = cursor / chunkArea;
            const int cx = firstX + int(chunk % columns);
            const int cy = firstY + int(chunk / columns);
            if (chunkCounts_[std::size_t(cy) * chunkColumns_ + cx] == 0)
            {
                const auto skipped = chunkArea - cursor % chunkArea;
                cursor += skipped;
                scanned += skipped;
                continue;
            }
            const auto local = cursor++ % chunkArea;
            ++scanned;
            const SettlementTilePosition tile{
                cx * ChunkSide + int(local % ChunkSide),
                cy * ChunkSide + int(local / ChunkSide)
            };
            if (f.contains(tile) && at(tile).kind != NaturalFeatureKind::None)
            {
                return tile;
            }
        }
        return {};
    }
    std::size_t SettlementNaturalFeatures::countIn(
        const SettlementObjectFootprint& f
    ) const noexcept
    {
        if (chunkCounts_.empty())
        {
            return 0;
        }
        std::size_t count = 0;
        const int left = std::max(0, f.topLeft.x),
                  top = std::max(0, f.topLeft.y);
        const int right = std::min(width_, f.topLeft.x + f.width),
                  bottom = std::min(height_, f.topLeft.y + f.height);
        for (int cy = top / ChunkSide; cy * ChunkSide < bottom; ++cy)
        {
            for (int cx = left / ChunkSide; cx * ChunkSide < right; ++cx)
            {
                const auto total =
                    chunkCounts_[std::size_t(cy) * chunkColumns_ + cx];
                if (!total)
                {
                    continue;
                }
                const int x0 = cx * ChunkSide, y0 = cy * ChunkSide,
                          x1 = std::min(width_, x0 + ChunkSide),
                          y1 = std::min(height_, y0 + ChunkSide);
                if (left <= x0 && top <= y0 && right >= x1 && bottom >= y1)
                {
                    count += total;
                }
                else
                {
                    for (int y = std::max(top, y0); y < std::min(bottom, y1);
                         ++y)
                    {
                        for (int x = std::max(left, x0);
                             x < std::min(right, x1);
                             ++x)
                        {
                            count +=
                                at({x, y}).kind != NaturalFeatureKind::None;
                        }
                    }
                }
            }
        }
        return count;
    }
} // namespace Paladin

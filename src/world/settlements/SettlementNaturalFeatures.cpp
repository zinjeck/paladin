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
            return (GenerationNoise::mix(seed ^ (std::uint64_t(x) << 32)
                ^ std::uint64_t(y) ^ salt) >> 11) * 0x1.0p-53;
        }
    }
    SettlementNaturalFeatures::SettlementNaturalFeatures(int width, int height)
        : width_(width), height_(height),
          chunkColumns_((width + ChunkSide - 1) / ChunkSide),
          features_(std::size_t(width) * height),
          versions_(std::size_t(chunkColumns_)
              * ((height + ChunkSide - 1) / ChunkSide), 1)
    {
    }
    bool SettlementNaturalFeatures::valid(SettlementTilePosition p) const noexcept
    {
        return p.x >= 0 && p.y >= 0 && p.x < width_ && p.y < height_;
    }
    NaturalFeature SettlementNaturalFeatures::at(SettlementTilePosition p) const noexcept
    {
        return valid(p) ? features_[std::size_t(p.y) * width_ + p.x] : NaturalFeature{};
    }
    void SettlementNaturalFeatures::changed(SettlementTilePosition p)
    {
        ++version_;
        ++versions_[std::size_t(p.y / ChunkSide) * chunkColumns_ + p.x / ChunkSide];
    }
    void SettlementNaturalFeatures::set(SettlementTilePosition p, NaturalFeatureKind kind)
    {
        if (!valid(p)) return;
        auto& feature = features_[std::size_t(p.y) * width_ + p.x];
        if (feature.kind == kind) return;
        feature = {kind, false};
        changed(p);
    }
    void SettlementNaturalFeatures::mark(SettlementTilePosition p, bool marked)
    {
        if (!valid(p)) return;
        auto& feature = features_[std::size_t(p.y) * width_ + p.x];
        if (feature.marked == marked) return;
        feature.marked = marked;
        changed(p);
    }
    void SettlementNaturalFeatures::clear(const SettlementObjectFootprint& footprint)
    {
        for (int y = std::max(0, footprint.topLeft.y);
             y < std::min(height_, footprint.topLeft.y + footprint.height); ++y)
            for (int x = std::max(0, footprint.topLeft.x);
                 x < std::min(width_, footprint.topLeft.x + footprint.width); ++x)
                set({x, y}, NaturalFeatureKind::None);
    }
    std::uint64_t SettlementNaturalFeatures::chunkVersion(int x, int y) const noexcept
    {
        return versions_[std::size_t(y) * chunkColumns_ + x];
    }
    void SettlementNaturalFeatures::generate(const SettlementGrid& grid,
        std::uint64_t seed, const NaturalFeatureGenerationPolicy& policy)
    {
        for (int y = 0; y < height_; ++y)
        {
            for (int x = 0; x < width_; ++x)
            {
                const auto& tile = *grid.tile({x, y});
                NaturalFeatureKind kind = NaturalFeatureKind::None;
                const auto entry = std::find_if(policy.biomes.begin(), policy.biomes.end(),
                    [&](const auto& item) { return item.biome == tile.biome; });
                if (tile.terrain == TerrainType::Land && entry != policy.biomes.end())
                {
                    const auto noise = [&](double frequency, std::uint64_t salt, int octaves)
                    {
                        return (GenerationNoise::simplexFractal(
                            x * frequency, y * frequency, seed + salt, octaves) + 1.0) * .5;
                    };
                    const double patch = noise(policy.treeFrequency, 104729, 3);
                    double chance = patch <= entry->clusterStart ? 0.0
                        : entry->treeChance * std::lerp(entry->edgeOccupancy, 1.0,
                            smooth(entry->clusterStart, entry->clusterFull, patch));
                    chance *= std::clamp(1.12 - std::abs(tile.temperature.value()
                        - entry->preferredTemperature) * .55, .80, 1.12)
                        * std::lerp(.78, 1.18, double(tile.rainfall.value()));
                    const double reservedChance = chance;
                    if (entry->denseForest)
                        chance *= std::lerp(.34, 1.0,
                            smooth(.18, .36, noise(policy.clearingFrequency, 117877, 2)));
                    const double treeRoll = roll(seed, x, y, 101);
                    if (treeRoll < std::clamp(chance, 0.0, .72))
                        kind = NaturalFeatureKind::Tree;
                    else
                    {
                        double rockChance = entry->rockChance + entry->rockClusterChance
                            * smooth(policy.rockClusterStart, policy.rockClusterFull,
                                noise(policy.rockFrequency, 130363, 2));
                        if (treeRoll < reservedChance) rockChance *= .35;
                        if (roll(seed, x, y, 211) < rockChance) kind = NaturalFeatureKind::Rock;
                    }
                }
                set({x, y}, kind);
            }
        }
    }
}

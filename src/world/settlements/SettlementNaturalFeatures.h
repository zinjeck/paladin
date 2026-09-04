#pragma once
#include "world/SettlementGrid.h"
#include "world/settlements/objects/SettlementObjectState.h"
#include <vector>
#include <array>

namespace Paladin
{
    enum class NaturalFeatureKind : std::uint8_t { None, Tree, Rock };
    struct NaturalFeature
    {
        NaturalFeatureKind kind = NaturalFeatureKind::None;
        bool marked = false;
    };
    struct NaturalFeatureBiomePolicy
    {
        BiomeType biome;
        double treeChance;
        double clusterStart;
        double clusterFull;
        double edgeOccupancy;
        double rockChance;
        double rockClusterChance;
        double preferredTemperature;
        bool denseForest;
    };
    struct NaturalFeatureGenerationPolicy
    {
        std::array<NaturalFeatureBiomePolicy, 7> biomes{{
            {BiomeType::Plain, .11, .66, .82, .05, .0034, .14, .52, false},
            {BiomeType::Forest, .62, .34, .58, .28, .0028, .12, .52, true},
            {BiomeType::Jungle, .68, .34, .58, .28, .0025, .10, .82, true},
            {BiomeType::Taiga, .56, .34, .58, .28, .0030, .14, .26, true},
            {BiomeType::Tundra, .025, .74, .88, .02, .0032, .16, .18, false},
            {BiomeType::Desert, 0, .40, .62, .24, .0026, .15, .78, false},
            {BiomeType::Ocean, 0, 0, 1, 0, 0, 0, .52, false}
        }};
        double treeFrequency = .018;
        double clearingFrequency = .034;
        double rockFrequency = .085;
        double rockClusterStart = .60;
        double rockClusterFull = .78;
    };
    class SettlementNaturalFeatures
    {
    public:
        static constexpr int ChunkSide = 32;
        SettlementNaturalFeatures(int width, int height);
        void generate(const SettlementGrid& grid, std::uint64_t seed,
            const NaturalFeatureGenerationPolicy& policy = {});
        NaturalFeature at(SettlementTilePosition position) const noexcept;
        void set(SettlementTilePosition position, NaturalFeatureKind kind);
        void mark(SettlementTilePosition position, bool marked);
        void clear(const SettlementObjectFootprint& footprint);
        std::uint64_t version() const noexcept { return version_; }
        std::uint64_t chunkVersion(int x, int y) const noexcept;
    private:
        std::uint64_t version_ = 0;
        int width_;
        int height_;
        int chunkColumns_;
        std::vector<NaturalFeature> features_;
        std::vector<std::uint64_t> versions_;
        bool valid(SettlementTilePosition p) const noexcept;
        void changed(SettlementTilePosition p);
    };
}

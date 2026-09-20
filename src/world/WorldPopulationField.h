#pragma once
#include "world/WorldTilePosition.h"
#include <cstdint>
#include <unordered_set>
#include <vector>
namespace Paladin
{
    class World;
    // Cartographic census, not ownership. Detailed cities use residents' homes;
    // strategic cities distribute their census across a bounded connected urban
    // and rural catchment. The latter is explicitly an estimate, not simulated
    // buildings.
    class WorldPopulationField
    {
    public:
        explicit WorldPopulationField(const World&, bool deferred = false);
        bool complete() const noexcept
        {
            return nextCity_ >= cityCount_;
        }
        void advance(const World&);
        float at(WorldTilePosition) const noexcept;
        double total() const noexcept;
        static std::uint64_t fingerprint(const World&) noexcept;
    private:
        std::size_t nextCity_ = 0, cityCount_ = 0;
        std::unordered_set<std::size_t> roads_;
        int width_=0,height_=0;
        std::vector<float> people_;
    };
}

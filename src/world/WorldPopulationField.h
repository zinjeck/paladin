#pragma once
#include "world/WorldTilePosition.h"
#include <cstdint>
#include <vector>
namespace Paladin
{
    class World;
    // Cartographic census, not ownership. Detailed cities use residents' homes;
    // strategic cities distribute their census across a bounded connected urban
    // footprint. The latter is explicitly an estimate, not simulated buildings.
    class WorldPopulationField
    {
    public:
        explicit WorldPopulationField(const World&);
        float at(WorldTilePosition) const noexcept;
        double total() const noexcept;
        static std::uint64_t fingerprint(const World&) noexcept;
    private:
        int width_=0,height_=0;
        std::vector<float> people_;
    };
}

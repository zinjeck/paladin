#pragma once
#include "world/WorldTilePosition.h"
#include <cstdint>
namespace Paladin
{
    class World;
    class AiRealmGenerator
    {
    public:
        void generate(World&) const;
        void ensurePlayerNeighbors(World&, WorldTilePosition) const;
    };
} // namespace Paladin

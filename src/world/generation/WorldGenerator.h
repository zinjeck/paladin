#pragma once

namespace Paladin
{
    class WorldGrid;
    struct WorldGenerationSettings;

    class WorldGenerator
    {
    public:
        void generate(
            WorldGrid& grid,
            const WorldGenerationSettings& settings
        ) const;
    };
}

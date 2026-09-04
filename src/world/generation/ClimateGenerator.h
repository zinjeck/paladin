#pragma once

namespace Paladin
{
    class WorldGrid;
    struct WorldGenerationSettings;

    class ClimateGenerator
    {
    public:
        void generate(
            WorldGrid& grid,
            const WorldGenerationSettings& settings
        ) const;
    };
}

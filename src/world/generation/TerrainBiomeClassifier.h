#pragma once

namespace Paladin
{
    class WorldGrid;
    struct WorldGenerationSettings;

    class TerrainBiomeClassifier
    {
    public:
        void classify(
            WorldGrid& grid,
            const WorldGenerationSettings& settings
        ) const;
    };
}

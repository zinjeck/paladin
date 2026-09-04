#pragma once

namespace Paladin
{
    class WorldGrid;
    struct WorldGenerationSettings;

    class LandmassGenerator
    {
    public:
        void generate(
            WorldGrid& grid,
            const WorldGenerationSettings& settings
        ) const;
    };
}

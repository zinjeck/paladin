#include "world/generation/WorldGenerator.h"

#include "world/WorldGrid.h"
#include "world/generation/ClimateGenerator.h"
#include "world/generation/LandmassGenerator.h"
#include "world/generation/TerrainBiomeClassifier.h"
#include "world/generation/WorldGenerationSettings.h"

#include <stdexcept>

namespace Paladin
{
    namespace
    {
        void validateSettings(
            const WorldGrid& grid,
            const WorldGenerationSettings& settings
        )
        {
            if (
                settings.width <= 0 ||
                settings.height <= 0
            )
            {
                throw std::invalid_argument(
                    "World generation dimensions must be positive."
                );
            }

            if (
                grid.width() != settings.width ||
                grid.height() != settings.height
            )
            {
                throw std::invalid_argument(
                    "World generation settings must match the target grid."
                );
            }

            if (
                settings.minimumContinentCount < 3 ||
                settings.maximumContinentCount > 5 ||
                settings.minimumContinentCount
                    > settings.maximumContinentCount
            )
            {
                throw std::invalid_argument(
                    "World generation requires between three and five continents."
                );
            }

            if (
                settings.seaLevel <= 0.0F ||
                settings.seaLevel >= 1.0F
            )
            {
                throw std::invalid_argument(
                    "World generation sea level must be between zero and one."
                );
            }
        }
    }

    void WorldGenerator::generate(
        WorldGrid& grid,
        const WorldGenerationSettings& settings
    ) const
    {
        validateSettings(grid, settings);

        LandmassGenerator{}.generate(grid, settings);
        ClimateGenerator{}.generate(grid, settings);
        TerrainBiomeClassifier{}.classify(grid, settings);
    }
}

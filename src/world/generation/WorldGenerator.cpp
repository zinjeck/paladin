#include "world/generation/WorldGenerator.h"
#include "world/Geology.h"

#include "world/WorldGrid.h"
#include "world/generation/ClimateGenerator.h"
#include "world/generation/LandmassGenerationTemplate.h"
#include "world/generation/LandmassGenerator.h"
#include "world/generation/TerrainBiomeClassifier.h"
#include "world/generation/WorldGenerationSettings.h"
#include "world/generation/WorldRelief.h"

#include <stdexcept>
#include <vector>

namespace Paladin
{
    namespace
    {
        void markPolarContinents(WorldGrid& grid)
        {
            std::vector<bool> visited(grid.tileCount(), false);
            std::vector<WorldTilePosition> land;
            for (int y = 0; y < grid.height(); ++y)
            {
                for (int x = 0; x < grid.width(); ++x)
                {
                    const auto index = std::size_t(y) * grid.width() + x;
                    if (visited[index] ||
                        grid.tile({x, y})->terrain == TerrainType::Water)
                    {
                        continue;
                    }
                    land.clear();
                    land.push_back({x, y});
                    visited[index] = true;
                    std::size_t cold = 0;
                    double latitude = 0;
                    for (std::size_t head = 0; head < land.size(); ++head)
                    {
                        const auto p = land[head];
                        const auto* tile = grid.tile(p);
                        cold += tile->biome == BiomeType::Polar ||
                                tile->biome == BiomeType::Tundra;
                        latitude += (p.y + .5) / grid.height();
                        for (const auto d :
                             {WorldTilePosition{1, 0},
                              {-1, 0},
                              {0, 1},
                              {0, -1}})
                        {
                            const WorldTilePosition q{
                                (p.x + d.x + grid.width()) % grid.width(),
                                p.y + d.y
                            };
                            const auto* next = grid.tile(q);
                            if (!next || next->terrain == TerrainType::Water)
                            {
                                continue;
                            }
                            const auto i =
                                std::size_t(q.y) * grid.width() + q.x;
                            if (!visited[i])
                            {
                                visited[i] = true;
                                land.push_back(q);
                            }
                        }
                    }
                    const double center = latitude / land.size();
                    const bool polar = (center < .16 || center > .84) &&
                                       cold * 2 >= land.size();
                    for (const auto p : land)
                    {
                        grid.tile(p)->polarContinent = polar;
                    }
                }
            }
        }
        void validateSettings(
            const WorldGrid& grid,
            const WorldGenerationSettings& settings
        )
        {
            if (settings.width <= 0 || settings.height <= 0)
            {
                throw std::invalid_argument(
                    "World generation dimensions must be positive."
                );
            }

            if (grid.width() != settings.width ||
                grid.height() != settings.height)
            {
                throw std::invalid_argument(
                    "World generation settings must match the target grid."
                );
            }

            const LandmassGenerationTemplate* landmassTemplate =
                findLandmassGenerationTemplate(settings.landmassTemplateId);

            if (!landmassTemplate ||
                !isValidLandmassGenerationTemplate(*landmassTemplate))
            {
                throw std::invalid_argument(
                    "World generation requires a valid landmass template."
                );
            }

            const LandmassContinentCountRange continentCount =
                resolveLandmassContinentCountRange(
                    *landmassTemplate,
                    settings.minimumContinentCount,
                    settings.maximumContinentCount
                );

            if (continentCount.minimum <= 0 ||
                continentCount.maximum < continentCount.minimum ||
                static_cast<std::size_t>(continentCount.maximum) >
                    landmassTemplate->continentSlots.size())
            {
                throw std::invalid_argument(
                    "World generation continent count exceeds the selected "
                    "template."
                );
            }

            if (settings.seaLevel <= 0.0F || settings.seaLevel >= 1.0F)
            {
                throw std::invalid_argument(
                    "World generation sea level must be between zero and one."
                );
            }
        }
    } // namespace

    void WorldGenerator::generate(
        WorldGrid& grid,
        const WorldGenerationSettings& settings
    ) const
    {
        validateSettings(grid, settings);

        LandmassGenerator{}.generate(grid, settings);
        generateWorldRelief(grid, settings.seaLevel, settings.seed);
        ClimateGenerator{}.generate(grid, settings);
        TerrainBiomeClassifier{}.classify(grid, settings);
        markPolarContinents(grid);
        generateGeology(grid, settings.seed);
    }
} // namespace Paladin

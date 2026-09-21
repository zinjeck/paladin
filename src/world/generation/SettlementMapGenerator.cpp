#include "world/generation/SettlementMapGenerator.h"

#include "world/BiomeType.h"
#include "world/SettlementGrid.h"
#include "world/TerrainType.h"
#include "world/WorldGrid.h"
#include "world/WorldTile.h"
#include "world/generation/GenerationNoise.h"
#include "world/settlements/SettlementMap.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <utility>

namespace Paladin
{
    namespace
    {
        constexpr std::size_t biomeCount = 9;

        std::size_t biomeIndex(BiomeType biome) noexcept
        {
            return static_cast<std::size_t>(biome);
        }

        std::uint64_t citySeed(
            std::uint64_t worldSeed,
            WorldTilePosition center,
            std::int32_t width,
            std::int32_t height
        ) noexcept
        {
            return GenerationNoise::mix(
                worldSeed ^
                GenerationNoise::mix(
                    static_cast<std::uint64_t>(center.x) * 73'856'093ULL
                ) ^
                GenerationNoise::mix(
                    static_cast<std::uint64_t>(center.y) * 19'349'663ULL
                ) ^
                GenerationNoise::mix(
                    static_cast<std::uint64_t>(width) * 83'492'791ULL +
                    static_cast<std::uint64_t>(height)
                )
            );
        }

        double weight(bool condition, double sampleWeight) noexcept
        {
            return condition ? sampleWeight : 0.0;
        }

        BiomeType dominantLandBiome(
            const std::array<double, biomeCount>& biomeWeights,
            std::int32_t x,
            std::int32_t y,
            std::uint64_t seed,
            double boundaryNoiseStrength
        ) noexcept
        {
            constexpr std::array<BiomeType, 8> landBiomes{
                BiomeType::Plain,
                BiomeType::Forest,
                BiomeType::Jungle,
                BiomeType::Desert,
                BiomeType::Tundra,
                BiomeType::Taiga,
                BiomeType::Hills,
                BiomeType::Polar
            };

            BiomeType bestBiome = BiomeType::Plain;
            double bestScore = -std::numeric_limits<double>::infinity();

            for (std::size_t index = 0; index < landBiomes.size(); ++index)
            {
                const BiomeType biome = landBiomes[index];
                const double boundaryNoise = GenerationNoise::simplexFractal(
                    static_cast<double>(x) * 0.050,
                    static_cast<double>(y) * 0.050,
                    seed + 6'397ULL + index * 1'003ULL,
                    3,
                    0.50,
                    2.0
                );

                const double score = biomeWeights[biomeIndex(biome)] +
                                     boundaryNoise * boundaryNoiseStrength;

                if (score > bestScore)
                {
                    bestScore = score;
                    bestBiome = biome;
                }
            }

            return bestBiome;
        }
    } // namespace


    SettlementMapGenerationSettings
    defaultSettlementMapGenerationSettings() noexcept
    {
        return {};
    }


    std::unique_ptr<SettlementMap> SettlementMapGenerator::generate(
        const WorldGrid& sourceGrid,
        WorldTilePosition sourceRegionCenter,
        std::int32_t sourceRegionWidth,
        std::int32_t sourceRegionHeight,
        std::uint64_t worldSeed,
        const SettlementMapGenerationSettings& settings
    ) const
    {
        if (sourceRegionWidth <= 0 || sourceRegionHeight <= 0 ||
            settings.localTilesPerWorldTile <= 0)
        {
            return nullptr;
        }

        const WorldTilePosition sourceTopLeft{
            sourceRegionCenter.x - sourceRegionWidth / 2,
            sourceRegionCenter.y - sourceRegionHeight / 2
        };

        const WorldTilePosition sourceBottomRight{
            sourceTopLeft.x + sourceRegionWidth - 1,
            sourceTopLeft.y + sourceRegionHeight - 1
        };

        if (!sourceGrid.isValidPosition(sourceTopLeft) ||
            !sourceGrid.isValidPosition(sourceBottomRight))
        {
            return nullptr;
        }

        const std::int32_t cityWidth =
            sourceRegionWidth * settings.localTilesPerWorldTile;

        const std::int32_t cityHeight =
            sourceRegionHeight * settings.localTilesPerWorldTile;

        SettlementGrid cityGrid(cityWidth, cityHeight);

        const std::uint64_t seed = citySeed(
            worldSeed,
            sourceRegionCenter,
            sourceRegionWidth,
            sourceRegionHeight
        );

        for (std::int32_t y = 0; y < cityHeight; ++y)
        {
            for (std::int32_t x = 0; x < cityWidth; ++x)
            {
                double sourceX = (static_cast<double>(x) + 0.5) /
                                     static_cast<double>(cityWidth) *
                                     static_cast<double>(sourceRegionWidth) -
                                 0.5;

                double sourceY = (static_cast<double>(y) + 0.5) /
                                     static_cast<double>(cityHeight) *
                                     static_cast<double>(sourceRegionHeight) -
                                 0.5;

                sourceX += GenerationNoise::simplexFractal(
                               static_cast<double>(x) * 0.026,
                               static_cast<double>(y) * 0.026,
                               seed + 1'771ULL,
                               3,
                               0.52,
                               2.0
                           ) *
                           settings.coordinateWarpStrength;

                sourceY += GenerationNoise::simplexFractal(
                               static_cast<double>(x + 9'173) * 0.026,
                               static_cast<double>(y - 4'289) * 0.026,
                               seed + 1'771ULL,
                               3,
                               0.52,
                               2.0
                           ) *
                           settings.coordinateWarpStrength;

                sourceX = std::clamp(
                    sourceX,
                    0.0,
                    static_cast<double>(sourceRegionWidth - 1)
                );

                sourceY = std::clamp(
                    sourceY,
                    0.0,
                    static_cast<double>(sourceRegionHeight - 1)
                );

                const std::int32_t x0 =
                    static_cast<std::int32_t>(std::floor(sourceX));

                const std::int32_t y0 =
                    static_cast<std::int32_t>(std::floor(sourceY));

                const std::int32_t x1 = std::min(x0 + 1, sourceRegionWidth - 1);

                const std::int32_t y1 =
                    std::min(y0 + 1, sourceRegionHeight - 1);

                const double tx = sourceX - static_cast<double>(x0);
                const double ty = sourceY - static_cast<double>(y0);

                const std::array<double, 4> sampleWeights{
                    (1.0 - tx) * (1.0 - ty),
                    tx * (1.0 - ty),
                    (1.0 - tx) * ty,
                    tx * ty
                };

                const std::array<const WorldTile*, 4> samples{
                    sourceGrid.tile(
                        {sourceTopLeft.x + x0, sourceTopLeft.y + y0}
                    ),
                    sourceGrid.tile(
                        {sourceTopLeft.x + x1, sourceTopLeft.y + y0}
                    ),
                    sourceGrid.tile(
                        {sourceTopLeft.x + x0, sourceTopLeft.y + y1}
                    ),
                    sourceGrid.tile(
                        {sourceTopLeft.x + x1, sourceTopLeft.y + y1}
                    )
                };

                double elevation = 0.0;
                double temperature = 0.0;
                double rainfall = 0.0;
                double waterWeight = 0.0;
                double mountainWeight = 0.0;
                double hillWeight = 0.0;
                std::array<double, biomeCount> biomeWeights{};

                for (std::size_t index = 0; index < samples.size(); ++index)
                {
                    const WorldTile& sample = *samples[index];
                    const double sampleWeight = sampleWeights[index];

                    elevation += static_cast<double>(sample.elevation.value()) *
                                 sampleWeight;

                    temperature +=
                        static_cast<double>(sample.temperature.value()) *
                        sampleWeight;

                    rainfall += static_cast<double>(sample.rainfall.value()) *
                                sampleWeight;

                    waterWeight += weight(
                        sample.terrain == TerrainType::Water,
                        sampleWeight
                    );

                    mountainWeight += weight(
                        sample.terrain == TerrainType::Mountain,
                        sampleWeight
                    );

                    hillWeight += weight(
                        sample.relief == ReliefType::Hills ||
                            sample.biome == BiomeType::Hills,
                        sampleWeight
                    );
                    biomeWeights[biomeIndex(sample.biome)] += sampleWeight;
                }

                const double coastNoise = GenerationNoise::simplexFractal(
                    static_cast<double>(x) * 0.060,
                    static_cast<double>(y) * 0.060,
                    seed + 2'887ULL,
                    4,
                    0.52,
                    2.0
                );

                const double coastlineThreshold =
                    0.50 + coastNoise * settings.coastlineNoiseStrength;

                WorldTile* output = cityGrid.tile({x, y});

                const double elevationDetail =
                    GenerationNoise::simplexFractal(
                        static_cast<double>(x) * 0.055,
                        static_cast<double>(y) * 0.055,
                        seed,
                        4,
                        0.50,
                        2.0
                    ) *
                    settings.localElevationNoiseStrength;

                output->elevation =
                    Elevation(static_cast<float>(elevation + elevationDetail));

                output->temperature =
                    Temperature(static_cast<float>(temperature));

                output->rainfall = Rainfall(static_cast<float>(rainfall));

                if (waterWeight > coastlineThreshold)
                {
                    output->terrain = TerrainType::Water;
                    output->biome = BiomeType::Ocean;
                    continue;
                }

                output->biome = dominantLandBiome(
                    biomeWeights,
                    x,
                    y,
                    seed,
                    settings.biomeBoundaryNoiseStrength
                );

                const double mountainThreshold =
                    0.50 + GenerationNoise::simplexFractal(
                               static_cast<double>(x) * 0.050,
                               static_cast<double>(y) * 0.050,
                               seed + 7'409ULL,
                               3,
                               0.50,
                               2.0
                           ) * settings.biomeBoundaryNoiseStrength;

                // Anisotropic ridges leave connected valleys through uplands.
                // Hills use narrower, broken ranges rather than a flat biome.
                const double ridge = std::abs(
                    GenerationNoise::simplexFractal(
                        x * .025,
                        y * .055,
                        seed + 7411,
                        2,
                        .5,
                        2.0
                    )
                );
                const bool hills = hillWeight > .45;
                const bool mountain = mountainWeight > mountainThreshold;
                const bool rock =
                    (mountain && ridge > .09) || (hills && ridge > .32);
                output->terrain =
                    rock ? TerrainType::Mountain : TerrainType::Land;
                output->relief = hills      ? ReliefType::Hills
                                 : mountain ? ReliefType::Mountain
                                            : ReliefType::Lowland;
                const int nearest = (tx >= .5 ? 1 : 0) + (ty >= .5 ? 2 : 0);
                const double vein = GenerationNoise::fractal(
                    x * .32,
                    y * .32,
                    seed ^ 0x0AEULL,
                    2
                );
                output->mineral = MineralDeposit::None;
                if (vein > (rock ? .22 : hills ? .32 : .48))
                {
                    output->mineral = samples[nearest]->mineral;
                }
            }
        }

        // Sparse connected caves: one bounded candidate per 64-tile district.
        // Cardinal tunnels stay navigable; occasional small chambers give
        // them shape without hollowing out whole mountain ranges.
        constexpr SettlementTilePosition
            caveSteps[]{{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
        for (int by = 0; by < cityGrid.height(); by += 64)
        {
            for (int bx = 0; bx < cityGrid.width(); bx += 64)
            {
                auto hash = GenerationNoise::mix(
                    seed ^ std::uint64_t(bx) * 73856093ULL ^
                    std::uint64_t(by) * 19349663ULL
                );
                if (hash % 4 != 0)
                {
                    continue;
                }
                const SettlementTilePosition origin{
                    bx + int((hash >> 8) % 64),
                    by + int((hash >> 16) % 64)
                };
                const auto* start = cityGrid.tile(origin);
                if (!start || start->terrain != TerrainType::Mountain ||
                    start->relief == ReliefType::Hills)
                {
                    continue;
                }
                for (int branch = 0; branch < 2; ++branch)
                {
                    auto p = origin;
                    int direction = int((hash + branch * 2) % 4);
                    const int length = 18 + int((hash >> 24) % 28);
                    for (int step = 0; step < length; ++step)
                    {
                        auto* tile = cityGrid.tile(p);
                        if (!tile || (tile->terrain != TerrainType::Mountain &&
                                      !tile->rockFloor))
                        {
                            break;
                        }
                        hash = GenerationNoise::mix(hash);
                        const int radius = step % 13 == 0 ? 1 : 0;
                        for (int dy = -radius; dy <= radius; ++dy)
                        {
                            for (int dx = -radius; dx <= radius; ++dx)
                            {
                                auto* floor =
                                    cityGrid.tile({p.x + dx, p.y + dy});
                                if (floor &&
                                    floor->terrain == TerrainType::Mountain)
                                {
                                    floor->terrain = TerrainType::Land;
                                    floor->rockFloor = true;
                                }
                            }
                        }
                        if (step % 4 == 0 && hash % 3 == 0)
                        {
                            direction = (direction + (hash % 2 ? 1 : 3)) % 4;
                        }
                        p.x += caveSteps[direction].x;
                        p.y += caveSteps[direction].y;
                    }
                }
            }
        }
        cityGrid.classifyCoast(seed);
        auto result = std::make_unique<SettlementMap>(
            std::move(cityGrid),
            sourceRegionCenter,
            sourceRegionWidth,
            sourceRegionHeight,
            settings.localTilesPerWorldTile,
            seed
        );
        result->planetU = (sourceRegionCenter.x + .5) / sourceGrid.width();
        result->planetV = (sourceRegionCenter.y + .5) / sourceGrid.height();
        result->activities.policy.solarTimeOffsetMinutes =
            PlanetAstronomy::solarOffsetMinutes(result->planetU);
        result->naturalFeatures().generate(result->grid(), seed);
        result->animals.initialize(*result, seed);
        return result;
    }
} // namespace Paladin

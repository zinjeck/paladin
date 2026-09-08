#include "world/generation/LandmassGenerator.h"

#include "world/BiomeType.h"
#include "world/EnvironmentalValues.h"
#include "world/TerrainType.h"
#include "world/WorldGrid.h"
#include "world/WorldTile.h"
#include "world/WorldTilePosition.h"
#include "world/generation/GenerationNoise.h"
#include "world/generation/LandmassGenerationTemplate.h"
#include "world/generation/WorldGenerationSettings.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace Paladin
{
    namespace
    {
        constexpr double pi = 3.14159265358979323846;

        struct Point
        {
            double x = 0.0;
            double y = 0.0;
        };

        struct ContinentLobe
        {
            double offsetX = 0.0;
            double offsetY = 0.0;
            double inverseRadiusX = 1.0;
            double inverseRadiusY = 1.0;
            double strength = 0.0;
            double cosine = 1.0;
            double sine = 0.0;
        };

        struct ContinentShape
        {
            Point position;
            std::vector<ContinentLobe> lobes;
        };

        class GenerationRandom
        {
        public:
            explicit GenerationRandom(std::uint64_t seed) noexcept
                : state_(seed)
            {
            }

            [[nodiscard]]
            std::int32_t integer(
                std::int32_t minimum,
                std::int32_t maximum
            ) noexcept
            {
                const auto range =
                    static_cast<std::uint64_t>(maximum - minimum + 1);

                return minimum + static_cast<std::int32_t>(next() % range);
            }

            [[nodiscard]]
            double range(double minimum, double maximum) noexcept
            {
                return minimum + (maximum - minimum) * unit();
            }

        private:
            [[nodiscard]]
            std::uint64_t next() noexcept
            {
                state_ += 0x9E37'79B9'7F4A'7C15ULL;
                return GenerationNoise::mix(state_);
            }

            [[nodiscard]]
            double unit() noexcept
            {
                constexpr double inverse53Bits = 1.0 / 9'007'199'254'740'992.0;

                return static_cast<double>(next() >> 11) * inverse53Bits;
            }

            std::uint64_t state_ = 0;
        };

        double sampleShapeNoise(
            const FractalShapeNoise& definition,
            double x,
            double y,
            std::uint64_t worldSeed
        ) noexcept
        {
            const double sampleX = x * definition.frequency;
            const double sampleY = y * definition.frequency;
            const std::uint64_t seed = worldSeed + definition.seedOffset;

            const double noise = definition.type == ShapeNoiseType::Value
                                     ? GenerationNoise::fractal(
                                           sampleX,
                                           sampleY,
                                           seed,
                                           definition.octaveCount,
                                           definition.gain,
                                           definition.lacunarity
                                       )
                                     : GenerationNoise::simplexFractal(
                                           sampleX,
                                           sampleY,
                                           seed,
                                           definition.octaveCount,
                                           definition.gain,
                                           definition.lacunarity
                                       );

            return noise * definition.amplitude;
        }

        ContinentShape createContinent(
            Point position,
            GenerationRandom& random,
            double worldWidth,
            double worldHeight,
            const ContinentLobeTemplate& definition
        )
        {
            ContinentShape continent;
            continent.position = position;

            const std::int32_t lobeCount = random.integer(
                definition.minimumCount,
                definition.maximumCount
            );

            continent.lobes.reserve(static_cast<std::size_t>(lobeCount));
            const double backboneAngle = random.range(0.0, 2.0 * pi);
            const double massScale = random.range(.75, 1.05);
            const double bend = random.range(-1.1, 1.1);

            // A broad interior anchors each landmass; peripheral lobes change
            // its outline instead of making a chain of thin connected islands.
            continent.lobes.push_back(
                {0,
                 0,
                 1.0 / (worldWidth * .13 * massScale),
                 1.0 / (worldHeight * .17 * massScale),
                 1.05,
                 std::cos(backboneAngle),
                 std::sin(backboneAngle)}
            );
            for (std::int32_t index = 0; index < lobeCount; ++index)
            {
                const double along =
                    (index / double(std::max(1, lobeCount - 1)) - .5) * 2.;
                const double offsetX =
                    along * worldWidth * definition.maximumOffsetWidthFraction *
                        std::cos(backboneAngle + bend * along) +
                    .35 *
                        random.range(
                            -worldWidth * definition.maximumOffsetWidthFraction,
                            worldWidth * definition.maximumOffsetWidthFraction
                        );

                const double offsetY =
                    along * worldHeight *
                        definition.maximumOffsetHeightFraction *
                        std::sin(backboneAngle + bend * along) +
                    .35 *
                        random.range(
                            -worldHeight *
                                definition.maximumOffsetHeightFraction,
                            worldHeight * definition.maximumOffsetHeightFraction
                        );

                const double radiusX = random.range(
                    worldWidth * definition.minimumRadiusWidthFraction,
                    worldWidth * definition.maximumRadiusWidthFraction
                );

                const double radiusY = random.range(
                    worldHeight * definition.minimumRadiusHeightFraction,
                    worldHeight * definition.maximumRadiusHeightFraction
                );

                const double angle = backboneAngle + random.range(-.65, .65);

                continent.lobes.push_back(
                    {offsetX,
                     offsetY,
                     1.0 / (radiusX * massScale),
                     1.0 / (radiusY * massScale),
                     random.range(
                         definition.minimumStrength,
                         definition.maximumStrength
                     ),
                     std::cos(angle),
                     std::sin(angle)}
                );
            }

            // Short, broad shoulders create bays and peninsulas. Independent
            // shelf fragments introduce substantial offshore land, between
            // the scale of the main continents and tiny archipelago islands.
            const double armAngle = backboneAngle + random.range(.8, 2.1);
            continent.lobes.push_back(
                {std::cos(armAngle) * worldWidth * .075,
                 std::sin(armAngle) * worldHeight * .075,
                 1.0 / (worldWidth * .085 * massScale),
                 1.0 / (worldHeight * .075),
                 .82,
                 std::cos(armAngle),
                 std::sin(armAngle)}
            );

            return continent;
        }

        std::vector<ContinentShape> createContinents(
            const WorldGenerationSettings& settings,
            const LandmassGenerationTemplate& definition
        )
        {
            GenerationRandom random(settings.seed);

            const LandmassContinentCountRange countRange =
                resolveLandmassContinentCountRange(
                    definition,
                    settings.minimumContinentCount,
                    settings.maximumContinentCount
                );

            const std::int32_t continentCount =
                random.integer(countRange.minimum, countRange.maximum);

            std::vector<NormalizedMapPoint> slots = definition.continentSlots;

            for (std::size_t index = slots.size() - 1; index > 0; --index)
            {
                const auto swapIndex = static_cast<std::size_t>(
                    random.integer(0, static_cast<std::int32_t>(index))
                );

                std::swap(slots[index], slots[swapIndex]);
            }

            std::vector<ContinentShape> continents;
            continents.reserve(static_cast<std::size_t>(continentCount));

            const double worldWidth = static_cast<double>(settings.width);

            const double worldHeight = static_cast<double>(settings.height);

            for (std::int32_t index = 0; index < continentCount; ++index)
            {
                const NormalizedMapPoint slot =
                    slots[static_cast<std::size_t>(index)];

                const Point position{
                    worldWidth * std::clamp(
                                     slot.x + random.range(
                                                  -definition.centerJitterX,
                                                  definition.centerJitterX
                                              ),
                                     definition.minimumCenterX,
                                     definition.maximumCenterX
                                 ),
                    worldHeight * std::clamp(
                                      slot.y + random.range(
                                                   -definition.centerJitterY,
                                                   definition.centerJitterY
                                               ),
                                      definition.minimumCenterY,
                                      definition.maximumCenterY
                                  )
                };

                continents.push_back(createContinent(
                    position,
                    random,
                    worldWidth,
                    worldHeight,
                    definition.continentLobes
                ));
                if (index % 2 == 0)
                {
                    const double angle = random.range(0., 2 * pi);
                    ContinentShape fragment;
                    fragment.position = {
                        std::clamp(
                            position.x + std::cos(angle) * worldWidth * .20,
                            worldWidth * .08,
                            worldWidth * .92
                        ),
                        std::clamp(
                            position.y + std::sin(angle) * worldHeight * .23,
                            worldHeight * .09,
                            worldHeight * .91
                        )
                    };
                    const double scale = random.range(.65, 1.3);
                    fragment.lobes.push_back(
                        {0,
                         0,
                         1.0 / (worldWidth * .085 * scale),
                         1.0 / (worldHeight * .10 * scale),
                         .93,
                         std::cos(angle),
                         std::sin(angle)}
                    );
                    continents.push_back(std::move(fragment));
                }
            }

            // Preserve each connected outline while opening wider ocean basins
            // between continents. Scale offsets and radii together so broad
            // interiors and peninsulas keep their proportions.
            constexpr double footprintScale = .80;
            for (auto& continent : continents)
            {
                for (auto& lobe : continent.lobes)
                {
                    lobe.offsetX *= footprintScale;
                    lobe.offsetY *= footprintScale;
                    lobe.inverseRadiusX /= footprintScale;
                    lobe.inverseRadiusY /= footprintScale;
                }
            }
            return continents;
        }

        double continentCenterBias(
            double x,
            double y,
            const std::vector<ContinentShape>& continents,
            const LandmassGenerationTemplate& definition
        ) noexcept
        {
            double strongestBias = 0.0;

            for (const ContinentShape& continent : continents)
            {
                double continentBias = 0.0;

                for (const ContinentLobe& lobe : continent.lobes)
                {
                    const double offsetX =
                        x - (continent.position.x + lobe.offsetX);

                    const double offsetY =
                        y - (continent.position.y + lobe.offsetY);

                    const double rotatedX =
                        offsetX * lobe.cosine - offsetY * lobe.sine;

                    const double rotatedY =
                        offsetX * lobe.sine + offsetY * lobe.cosine;

                    const double normalizedX =
                        std::abs(rotatedX) * lobe.inverseRadiusX;

                    const double normalizedY =
                        std::abs(rotatedY) * lobe.inverseRadiusY;

                    const double distance = std::pow(
                        std::pow(
                            normalizedX,
                            definition.continentLobes.distanceExponent
                        ) +
                            std::pow(
                                normalizedY,
                                definition.continentLobes.distanceExponent
                            ),
                        1.0 / definition.continentLobes.distanceExponent
                    );

                    double bias = std::clamp(1.0 - distance, 0.0, 1.0);

                    bias = std::pow(
                               bias,
                               definition.continentLobes.falloffExponent
                           ) *
                           lobe.strength;

                    continentBias += bias;
                }

                strongestBias = std::max(strongestBias, continentBias);
            }

            return std::clamp(
                strongestBias,
                0.0,
                definition.maximumContinentCore
            );
        }

        double coastlineBreakup(
            double x,
            double y,
            double continentCore,
            std::uint64_t seed,
            const CoastlineBreakupTemplate& definition
        ) noexcept
        {
            const double noise = sampleShapeNoise(definition.noise, x, y, seed);

            if (continentCore > definition.interiorCoreThreshold)
            {
                return noise * definition.interiorAmplitude;
            }

            if (continentCore > definition.coastlineCoreThreshold)
            {
                return noise * definition.coastlineAmplitude;
            }

            return noise * definition.offshoreAmplitude;
        }

        double islandValue(
            double x,
            double y,
            double continentCore,
            std::uint64_t seed,
            const IslandTemplate& definition
        ) noexcept
        {
            if (continentCore > definition.maximumContinentCore)
            {
                return 0.0;
            }

            const double noise = sampleShapeNoise(definition.noise, x, y, seed);

            return noise > definition.noiseThreshold
                       ? (noise - definition.noiseThreshold) *
                             definition.amplitude
                       : 0.0;
        }

        double edgeFalloff(
            double x,
            double y,
            std::int32_t width,
            std::int32_t height,
            const EdgeFalloffTemplate& definition
        ) noexcept
        {
            const double normalizedX =
                width > 1
                    ? std::abs((x / static_cast<double>(width - 1)) * 2.0 - 1.0)
                    : 0.0;

            const double normalizedY =
                height > 1
                    ? std::abs(
                          (y / static_cast<double>(height - 1)) * 2.0 - 1.0
                      )
                    : 0.0;

            return std::pow(
                       std::max(normalizedX, normalizedY),
                       definition.exponent
                   ) *
                   definition.strength;
        }

        double normalizedElevation(
            double rawElevation,
            double seaLevel
        ) noexcept
        {
            return rawElevation >= 0.0
                       ? seaLevel + rawElevation * (1.0 - seaLevel)
                       : seaLevel + rawElevation * seaLevel;
        }
    } // namespace

    void LandmassGenerator::generate(
        WorldGrid& grid,
        const WorldGenerationSettings& settings
    ) const
    {
        const LandmassGenerationTemplate* definition =
            findLandmassGenerationTemplate(settings.landmassTemplateId);

        if (!definition)
        {
            throw std::invalid_argument(
                "Unknown landmass generation template."
            );
        }

        const LandmassContinentCountRange countRange =
            resolveLandmassContinentCountRange(
                *definition,
                settings.minimumContinentCount,
                settings.maximumContinentCount
            );

        if (!isValidLandmassGenerationTemplate(*definition) ||
            countRange.minimum <= 0 ||
            countRange.maximum < countRange.minimum ||
            static_cast<std::size_t>(countRange.maximum) >
                definition->continentSlots.size())
        {
            throw std::invalid_argument(
                "Invalid settings for the selected landmass template."
            );
        }

        const std::vector<ContinentShape> continents =
            createContinents(settings, *definition);

        for (std::int32_t y = 0; y < grid.height(); ++y)
        {
            for (std::int32_t x = 0; x < grid.width(); ++x)
            {
                const double positionX = static_cast<double>(x);

                const double positionY = static_cast<double>(y);

                const double continentCore = continentCenterBias(
                    positionX + grid.width() * .035 *
                                    GenerationNoise::simplexFractal(
                                        positionX / grid.width() * 3.8,
                                        positionY / grid.height() * 3.8,
                                        settings.seed + 771,
                                        3,
                                        .5,
                                        2
                                    ),
                    positionY + grid.height() * .035 *
                                    GenerationNoise::simplexFractal(
                                        positionX / grid.width() * 3.8,
                                        positionY / grid.height() * 3.8,
                                        settings.seed + 997,
                                        3,
                                        .5,
                                        2
                                    ),
                    continents,
                    *definition
                );

                double rawElevation = continentCore +
                                      sampleShapeNoise(
                                          definition->continentNoise,
                                          positionX,
                                          positionY,
                                          settings.seed
                                      ) * continentCore +
                                      sampleShapeNoise(
                                          definition->regionalNoise,
                                          positionX,
                                          positionY,
                                          settings.seed
                                      ) +
                                      coastlineBreakup(
                                          positionX,
                                          positionY,
                                          continentCore,
                                          settings.seed,
                                          definition->coastline
                                      ) +
                                      islandValue(
                                          positionX,
                                          positionY,
                                          continentCore,
                                          settings.seed,
                                          definition->islands
                                      ) -
                                      edgeFalloff(
                                          positionX,
                                          positionY,
                                          grid.width(),
                                          grid.height(),
                                          definition->edgeFalloff
                                      ) +
                                      definition->elevationBias;

                // Broad, winding flooded rifts can split a shelf into a large
                // island and mainland. Their width varies, creating straits
                // and sheltered inland seas rather than a uniform channel.
                const double nx = positionX / grid.width(),
                             ny = positionY / grid.height();
                const double rift = std::abs(
                    GenerationNoise::simplexFractal(
                        nx * 3.8,
                        ny * 3.8,
                        settings.seed + 84391,
                        2,
                        .4,
                        2.
                    )
                );
                const double riftStrength =
                    std::clamp((.045 - rift) / .045, 0., 1.);
                if (continentCore < .40)
                {
                    rawElevation -= riftStrength * .07;
                }

                // A seed-selected polar continent gives the coldest biome
                // real land. The other pole may remain ocean or island coast.
                const double pole = (settings.seed & 1) ? ny : 1 - ny;
                const double polarEdge =
                    .88 + .025 * GenerationNoise::simplexFractal(
                                     nx * 9,
                                     0,
                                     settings.seed + 773,
                                     2,
                                     .4,
                                     2.
                                 );
                if (pole > polarEdge)
                {
                    rawElevation =
                        std::max(rawElevation, (pole - polarEdge) * 3.5 - .035);
                }

                WorldTile* tile = grid.tile({x, y});

                tile->elevation =
                    Elevation{static_cast<float>(normalizedElevation(
                        rawElevation,
                        static_cast<double>(settings.seaLevel)
                    ))};

                tile->temperature = Temperature{};
                tile->rainfall = Rainfall{};

                const bool isLand = rawElevation > 0.0;

                tile->terrain = isLand ? TerrainType::Land : TerrainType::Water;

                tile->biome = isLand ? BiomeType::Plain : BiomeType::Ocean;
            }
        }
    }
} // namespace Paladin

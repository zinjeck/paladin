#include "simulation/systems/SettlementEconomySystem.h"
#include "simulation/WorldMarketSystem.h"

#include "world/Settlement.h"
#include "world/World.h"
#include "world/generation/GenerationNoise.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace Paladin
{
    namespace
    {
        bool forecastGeographicEconomy(const World& world, Settlement& city)
        {
            auto& economy = city.simulationState().economy();
            const auto* realm = world.realm(city.ownerRealmId());
            if (!realm || !realm->aiControlled)
            {
                return false;
            }
            const auto revision = world.grid().revision() * 1000003ULL +
                                  world.territory().revision() * 9176ULL +
                                  world.settlementCount();
            if (economy.geographyRevision == revision)
            {
                return false;
            }
            std::array<double, 4> ore{};
            double land = 0, fertile = 0, forest = 0, rugged = 0;
            const auto centre = city.position();
            for (int dy = -12; dy <= 12; ++dy)
            {
                for (int dx = -12; dx <= 12; ++dx)
                {
                    if (dx * dx + dy * dy > 144)
                    {
                        continue;
                    }
                    WorldTilePosition p{
                        (centre.x + dx + world.grid().width()) %
                            world.grid().width(),
                        centre.y + dy
                    };
                    const auto* tile = world.grid().tile(p);
                    if (!tile || tile->terrain == TerrainType::Water ||
                        tile->biome == BiomeType::Polar)
                    {
                        continue;
                    }
                    const auto owner = world.territory().controllerAt(p);
                    if (realm->usesCivicControl()
                            ? owner != realm->id()
                            : owner && owner != realm->id())
                    {
                        continue;
                    }
                    // Divide shared districts between their closest towns; the
                    // same ore cannot support every city at full output.
                    bool nearest = true;
                    for (const auto& other : world.settlements())
                    {
                        if (other.id() == city.id() ||
                            other.ownerRealmId() != city.ownerRealmId())
                        {
                            continue;
                        }
                        int x = std::abs(p.x - other.position().x);
                        x = std::min(x, world.grid().width() - x);
                        const int y = p.y - other.position().y;
                        if (x * x + y * y < dx * dx + dy * dy ||
                            (x * x + y * y == dx * dx + dy * dy &&
                             other.id() < city.id()))
                        {
                            nearest = false;
                            break;
                        }
                    }
                    if (!nearest)
                    {
                        continue;
                    }
                    ++land;
                    const bool mountain =
                        tile->terrain == TerrainType::Mountain ||
                        tile->relief == ReliefType::Mountain;
                    rugged += mountain                            ? 1
                              : tile->relief == ReliefType::Hills ? .5
                                                                  : 0;
                    fertile += mountain                           ? .05
                               : tile->biome == BiomeType::Desert ? .2
                               : tile->biome == BiomeType::Tundra ? .15
                               : tile->biome == BiomeType::Plain  ? 1
                                                                  : .7;
                    forest += tile->biome == BiomeType::Forest ||
                                      tile->biome == BiomeType::Jungle ||
                                      tile->biome == BiomeType::Taiga
                                  ? 1
                                  : .08;
                    ore[std::size_t(tile->mineral)] += 1;
                }
            }
            const double denominator = std::max(1., land);
            const double development =
                double(
                    GenerationNoise::mix(
                        world.generationSeed() ^ city.id().value()
                    ) %
                    10000
                ) /
                9999.;
            const double food =
                2 * (.83 + .23 * fertile / denominator + .12 * development);
            const double population = std::max(1., double(city.population()));
            std::vector<ResourceFlowRate> flows{
                {"food", food, 2, 1},
                {"materials", .028 + development * .035, .035, 0},
                {"lumber", .01 + forest / denominator * .065, .025, 0},
                {"stone", .005 + rugged / denominator * .065, .018, 0},
                {"wheat", .004 + fertile / denominator * .012, .012, 0},
                {"coal", std::min(.04, ore[1] * 2 / population), .012, 0},
                {"iron", std::min(.025, ore[2] * 1.25 / population), .007, 0},
                {"gold",
                 std::min(.0000005, ore[3] * .0005 / population),
                 .00000002,
                 0}
            };
            if (economy.configure(flows))
            {
                economy.geographyRevision = revision;
            }
            return true;
        }
    } // namespace

    void SettlementEconomySystem::tick(
        World& world,
        const WorldSimulationStep& step
    )
    {
        constexpr double gameMinutesPerDay = 24.0 * 60.0;
        bool surveyed = false;

        for (const SettlementSimulationStep& settlementStep :
             step.settlementSteps)
        {
            Settlement* settlement =
                world.settlement(settlementStep.settlementId);

            if (!settlement)
            {
                continue;
            }

            SettlementSimulationState& state = settlement->simulationState();

            if (state.hasLocalMap())
            {
                continue;
            }

            if (!surveyed)
            {
                surveyed = forecastGeographicEconomy(world, *settlement);
            }
            state.economy().simulate(
                state.stockpile(),
                state.population().residents(),
                static_cast<double>(settlementStep.gameMinutes) /
                    gameMinutesPerDay
            );
        }
        WorldMarketSystem::tick(world);
    }
} // namespace Paladin

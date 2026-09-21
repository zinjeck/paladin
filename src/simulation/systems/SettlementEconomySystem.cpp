#include "simulation/systems/SettlementEconomySystem.h"
#include "simulation/WorldMarketSystem.h"

#include "world/ResourceSurvey.h"
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
            const auto& policy = world.territoryFoundationPolicy();
            const auto survey = surveyResources(
                world.grid(),
                city.position(),
                settlementRegionDimension(
                    policy.settlementRegionWidth,
                    city.kind()
                ),
                settlementRegionDimension(
                    policy.settlementRegionHeight,
                    city.kind()
                )
            );
            const double denominator = std::max(1., survey.land);
            const double fertile = survey.amount("wheat");
            const double forest = survey.amount("lumber");
            const double rugged = survey.amount("stone");
            const std::array<double, 4> ore{
                0,
                survey.amount("coal"),
                survey.amount("iron"),
                survey.amount("gold")
            };
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
                {"lumber", forest / denominator * .10, .025, 0},
                {"stone", rugged / denominator * .25, .018, 0},
                {"wheat", fertile / denominator * .025, .012, 0},
                {"fish",
                 survey.amount("fish") / std::max(1., survey.tiles) * .04,
                 .01,
                 0},
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

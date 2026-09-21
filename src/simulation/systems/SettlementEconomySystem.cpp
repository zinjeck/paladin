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
            const auto revision =
                world.grid().revision() * 1000003ULL +
                world.territory().revision() * 9176ULL +
                world.settlementCount() +
                (world.time().totalGameMinutes() / 4320) * 999983ULL;
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
            const auto capacity = [&](std::uint64_t sector)
            {
                return .35 + 1.4 *
                                 double(
                                     GenerationNoise::mix(
                                         world.generationSeed() ^
                                         city.id().value() * 9176ULL ^ sector
                                     ) %
                                     10000
                                 ) /
                                 9999.;
            };
            // Construction and industry ebb independently of local deposits.
            // Resource-poor cities import; well-equipped producers can export.
            const double phase = double(world.time().totalGameMinutes() / 4320);
            const double building =
                .6 + .8 * (.5 + .5 * std::sin(phase * .9 + city.id().value()));
            const double grain = fertile / denominator * capacity(11);
            const double baking = grain * (1.4 + development);
            std::vector<ResourceFlowRate> flows{
                {"lumber",
                 forest / denominator * .20 * capacity(21),
                 .06 * building,
                 0},
                {"stone",
                 rugged / denominator * .30 * capacity(22),
                 .035 * building,
                 0},
                {"wheat", grain * .45 + baking, .10 + baking, 0},
                {"fish",
                 survey.amount("fish") / std::max(1., survey.tiles) * 3.5 *
                     capacity(31),
                 .45,
                 1},
                {"bread", baking, 1.15, 1},
                {"meat",
                 survey.amount("meat") / denominator * 2.8 * capacity(32),
                 .40,
                 1},
                {"rations", .025 + development * .04, .02, 0},
                {"coal",
                 ore[1] / denominator * .18 * capacity(41),
                 .009 * (.5 + development) * building,
                 0},
                {"iron",
                 ore[2] / denominator * .14 * capacity(42),
                 .006 * (.5 + development) * building,
                 0},
                {"gold",
                 ore[3] / denominator * .018 * capacity(43),
                 .001 * (.5 + development),
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
        // Forecasts must not wait for a settlement's aggregate simulation step.
        // Four small surveys per world tick keep initialization and refresh
        // bounded.
        const auto cities = world.settlements();
        for (std::size_t n = 0; n < std::min<std::size_t>(4, cities.size());
             ++n)
        {
            const auto id = cities[forecastCursor_++ % cities.size()].id();
            auto* city = world.settlement(id);
            if (city && !city->simulationState().hasLocalMap())
            {
                forecastGeographicEconomy(world, *city);
            }
        }

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

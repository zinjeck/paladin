#include "world/generation/AiRealmGenerator.h"
#include "simulation/RealmRulerSystem.h"
#include "world/World.h"
#include "world/generation/GenerationNoise.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include <array>
#include <cmath>
#include <numeric>

namespace Paladin
{
    namespace
    {
        struct Site
        {
            WorldTilePosition at;
            double quality;
            int component;
        };
        struct Geography
        {
            std::vector<int> component, area;
            std::vector<Site> sites;
            explicit Geography(const World& world)
            {
                const auto& grid = world.grid();
                const int width = grid.width(), height = grid.height();
                component.assign(std::size_t(width) * height, -1);
                std::vector<int> queue;
                for (int y = 0; y < height; ++y)
                {
                    for (int x = 0; x < width; ++x)
                    {
                        const int start = y * width + x;
                        if (component[start] >= 0 ||
                            grid.tile({x, y})->terrain == TerrainType::Water)
                        {
                            continue;
                        }
                        const int label = int(area.size());
                        queue.clear();
                        queue.push_back(start);
                        component[start] = label;
                        for (std::size_t i = 0; i < queue.size(); ++i)
                        {
                            const int cx = queue[i] % width,
                                      cy = queue[i] / width;
                            for (auto [dx, dy] :
                                 std::array<std::pair<int, int>, 4>{
                                     {{1, 0}, {-1, 0}, {0, 1}, {0, -1}}
                                 })
                            {
                                const int nx = (cx + dx + width) % width,
                                          ny = cy + dy;
                                if (ny < 0 || ny >= height)
                                {
                                    continue;
                                }
                                const int next = ny * width + nx;
                                if (component[next] >= 0 ||
                                    grid.tile({nx, ny})->terrain ==
                                        TerrainType::Water)
                                {
                                    continue;
                                }
                                component[next] = label;
                                queue.push_back(next);
                            }
                        }
                        area.push_back(int(queue.size()));
                    }
                }
                for (int y = 5; y < height - 5; y += 2)
                {
                    for (int x = 5; x < width - 5; x += 2)
                    {
                        const auto& tile = *grid.tile({x, y});
                        if (tile.terrain != TerrainType::Land ||
                            tile.biome == BiomeType::Polar ||
                            tile.biome == BiomeType::Tundra)
                        {
                            continue;
                        }
                        int water = 0;
                        for (int dy = -4; dy <= 4; ++dy)
                        {
                            for (int dx = -4; dx <= 4; ++dx)
                            {
                                water += grid.tile({x + dx, y + dy})->terrain ==
                                         TerrainType::Water;
                            }
                        }
                        if (water > 32)
                        {
                            continue;
                        }
                        const auto hash = GenerationNoise::mix(
                            world.generationSeed() ^ (std::uint64_t(x) << 32) ^
                            std::uint32_t(y)
                        );
                        // Fertile lowlands and sheltered coasts are favored,
                        // with seeded variation.
                        const double quality =
                            .6 + (hash % 10000) / 10000.0 +
                            (water > 0 ? .35 : 0) +
                            (tile.biome == BiomeType::Plain ? .3 : 0);
                        sites.push_back(
                            {{x, y}, quality, component[y * width + x]}
                        );
                    }
                }
            }
        };
        double distance(WorldTilePosition a, WorldTilePosition b, int width)
        {
            int dx = std::abs(a.x - b.x);
            dx = std::min(dx, width - dx);
            return std::hypot(dx, a.y - b.y);
        }
        bool spaced(const World& world, WorldTilePosition at, double minimum)
        {
            for (const auto& city : world.settlements())
            {
                if (distance(at, city.position(), world.grid().width()) <
                    minimum)
                {
                    return false;
                }
            }
            return true;
        }
        std::uint64_t next(std::uint64_t& state)
        {
            return state = GenerationNoise::mix(state);
        }
        constexpr std::array<std::string_view, 32> names{
            "Aster",   "Valmere", "Dunmar",  "Eldara", "Kestrel", "Ostara",
            "Thalen",  "Nerath",  "Caldrin", "Verden", "Ashen",   "Istria",
            "Merrow",  "Tarsen",  "Orvale",  "Selkar", "Ardent",  "Bracken",
            "Caerwyn", "Darovar", "Estrel",  "Fallow", "Galren",  "Harrow",
            "Ildren",  "Junara",  "Keldar",  "Lorien", "Morven",  "Norath",
            "Perwyn",  "Ravelle"
        };
        constexpr std::array<MapColor, 12> colors{
            {{185, 95, 74},
             {81, 146, 130},
             {191, 159, 72},
             {122, 109, 168},
             {98, 147, 79},
             {186, 113, 157},
             {77, 131, 175},
             {190, 137, 85},
             {133, 160, 163},
             {165, 84, 109},
             {146, 155, 94},
             {117, 109, 84}}
        };
        RealmId foundRealm(
            World& world,
            const Geography& geography,
            const Site& capital,
            RealmScale scale,
            int desired,
            std::uint64_t& rng
        )
        {
            if (!world.canFoundSettlementAt(capital.at) ||
                !spaced(world, capital.at, 12))
            {
                return {};
            }
            const auto id = world.createRealm();
            const auto number = id.value();
            const std::string base(names[next(rng) % names.size()]);
            FoundingIdentity identity;
            identity.realmName =
                base + (scale == RealmScale::Empire ? " Empire" : " Realm") +
                " " + std::to_string(number);
            identity.cultureName = base + " Folk";
            identity.capitalName = base + " " + std::to_string(number);
            identity.mapColor = colors[(number + next(rng)) % colors.size()];
            identity.realmOriginId = next(rng) % 3 == 0 ? "tribal" : "civic";
            identity.flag.primaryColor = identity.mapColor;
            for (std::size_t i = 0; i < identity.flag.cells.size(); ++i)
            {
                if (i % identity.flag.width == number % identity.flag.width ||
                    i / identity.flag.width == 4)
                {
                    identity.flag.cells[i] = {true, identity.mapColor};
                }
            }
            const auto profileFor = [&](bool isCapital, SettlementKind kind)
            {
                auto profile = defaultSettlementFoundationProfile();
                profile.kind = kind;
                profile.initialDetailedCitizenCount = 0;
                profile.initialSimulationTier =
                    SettlementSimulationTier::Strategic;
                const int mean =
                    kind == SettlementKind::Fortress
                        ? 55
                        : (scale == RealmScale::Small
                               ? 105
                               : (isCapital
                                      ? (scale == RealmScale::Empire ? 480
                                                                     : 270)
                                      : 200));
                const double variation =
                    .70 + (next(rng) % 1000 + next(rng) % 1000) / 3330.0;
                profile.initialPopulation = std::max(24, int(mean * variation));
                profile.initialResources = {
                    {std::string(SettlementResourceTypes::Food),
                     double(profile.initialPopulation) * 8},
                    {std::string(SettlementResourceTypes::Materials),
                     double(profile.initialPopulation) * 2}
                };
                // Strategic output represents the settlement's existing supply
                // economy. No local farms, citizens, routes or inter-settlement
                // transfers are created.
                profile.resourceFlowRates[0].dailyProductionPerResident =
                    1.02 + (next(rng) % 80) / 1000.0;
                return profile;
            };
            const auto capitalId = world.foundCapitalSettlement(
                capital.at,
                id,
                identity,
                profileFor(true, SettlementKind::City)
            );
            if (!capitalId)
            {
                return {};
            }
            world.realm(id)->scale = scale;
            RealmRulerSystem::establishAi(world, id);
            std::vector<std::pair<double, const Site*>> nearby;
            for (const auto& site : geography.sites)
            {
                const double d =
                    distance(site.at, capital.at, world.grid().width());
                if (site.component == capital.component && d >= 10 &&
                    d <= (scale == RealmScale::Empire ? 48 : 34))
                {
                    nearby.push_back({site.quality - d * .018, &site});
                }
            }
            std::stable_sort(
                nearby.begin(),
                nearby.end(),
                [](auto a, auto b) { return a.first > b.first; }
            );
            int count = 1;
            for (auto [score, site] : nearby)
            {
                if (count >= desired)
                {
                    break;
                }
                if (!world.canFoundSettlementAt(site->at, id) ||
                    !spaced(world, site->at, 10))
                {
                    continue;
                }
                bool nearOtherCapital = false;
                for (const auto& realm : world.realms())
                {
                    if (realm.id() != id)
                    {
                        if (const auto* other =
                                world.settlement(realm.capitalSettlementId()))
                        {
                            nearOtherCapital |= distance(
                                                    site->at,
                                                    other->position(),
                                                    world.grid().width()
                                                ) < 19;
                        }
                    }
                }
                if (nearOtherCapital)
                {
                    continue;
                }
                const auto kind = count >= 2 && next(rng) % 4 == 0
                                      ? SettlementKind::Fortress
                                      : SettlementKind::City;
                const auto settlement = world.foundSettlement(
                    site->at,
                    id,
                    profileFor(false, kind)
                );
                if (!settlement)
                {
                    continue;
                }
                ++count;
                static_cast<void>(world.renameSettlement(
                    settlement,
                    base +
                        (kind == SettlementKind::Fortress ? " Watch "
                                                          : " Haven ") +
                        std::to_string(count)
                ));
            }
            return id;
        }
    } // namespace

    void AiRealmGenerator::generate(World& world) const
    {
        const Geography geography(world);
        std::uint64_t rng = world.generationSeed() ^ 0xA1EAAULL;
        int empires = 0;
        std::vector<int> continents(geography.area.size());
        std::iota(continents.begin(), continents.end(), 0);
        std::stable_sort(
            continents.begin(),
            continents.end(),
            [&](int a, int b) { return geography.area[a] > geography.area[b]; }
        );
        for (int continent : continents)
        {
            const int area = geography.area[continent];
            if (area < 450)
            {
                continue;
            }
            const int target = std::clamp(
                int(std::sqrt(area / 110.0)),
                1,
                6 + int(next(rng) % 2)
            );
            const double spacing =
                std::max(22.0, std::sqrt(double(area) / target) * .65);
            std::vector<WorldTilePosition> centers;
            for (int n = 0; n < target; ++n)
            {
                const Site* best = nullptr;
                double bestScore = -1;
                for (const auto& site : geography.sites)
                {
                    if (site.component != continent)
                    {
                        continue;
                    }
                    double nearest = spacing * 2;
                    for (const auto center : centers)
                    {
                        nearest = std::min(
                            nearest,
                            distance(center, site.at, world.grid().width())
                        );
                    }
                    if (nearest < spacing ||
                        !world.canFoundSettlementAt(site.at) ||
                        !spaced(world, site.at, 16))
                    {
                        continue;
                    }
                    const double score = site.quality + nearest / spacing;
                    if (score > bestScore)
                    {
                        best = &site;
                        bestScore = score;
                    }
                }
                if (!best)
                {
                    break;
                }
                const auto scale =
                    empires < 2 && area >= 4500 && n == 0
                        ? RealmScale::Empire
                        : (next(rng) % 3 == 0 ? RealmScale::Medium
                                              : RealmScale::Small);
                const int count = scale == RealmScale::Empire
                                      ? 9 + int(next(rng) % 4)
                                      : (scale == RealmScale::Medium
                                             ? 5 + int(next(rng) % 3)
                                             : 2 + int(next(rng) % 2));
                if (foundRealm(world, geography, *best, scale, count, rng))
                {
                    centers.push_back(best->at);
                    empires += scale == RealmScale::Empire;
                }
            }
        }
    }

    void AiRealmGenerator::ensurePlayerNeighbors(
        World& world,
        WorldTilePosition player
    ) const
    {
        const Geography geography(world);
        const int component =
            geography.component[player.y * world.grid().width() + player.x];
        std::uint64_t rng = world.generationSeed() ^
                            (std::uint64_t(player.x) << 32) ^
                            std::uint32_t(player.y) ^ 0xAE16ULL;
        std::vector<WorldTilePosition> neighbors;
        for (const auto& realm : world.realms())
        {
            if (realm.aiControlled)
            {
                if (const auto* city =
                        world.settlement(realm.capitalSettlementId());
                    city &&
                    geography.component
                            [city->position().y * world.grid().width() +
                             city->position().x] == component &&
                    distance(city->position(), player, world.grid().width()) <=
                        80)
                {
                    neighbors.push_back(city->position());
                }
            }
        }
        while (neighbors.size() < 3)
        {
            const Site* best = nullptr;
            double bestScore = -1;
            for (const auto& site : geography.sites)
            {
                const double d =
                    distance(site.at, player, world.grid().width());
                if (site.component != component || d < 24 || d > 76 ||
                    !spaced(world, site.at, 16) ||
                    !world.canFoundSettlementAt(site.at))
                {
                    continue;
                }
                double nearest = 60;
                for (const auto at : neighbors)
                {
                    nearest = std::min(
                        nearest,
                        distance(at, site.at, world.grid().width())
                    );
                }
                if (nearest < 26)
                {
                    continue;
                }
                const double score =
                    site.quality + nearest / 30 - std::abs(d - 45) / 50;
                if (score > bestScore)
                {
                    best = &site;
                    bestScore = score;
                }
            }
            if (!best || !foundRealm(
                             world,
                             geography,
                             *best,
                             RealmScale::Small,
                             2 + int(next(rng) % 2),
                             rng
                         ))
            {
                break;
            }
            neighbors.push_back(best->at);
        }
    }
} // namespace Paladin

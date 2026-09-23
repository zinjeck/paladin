#include "world/WorldPopulationField.h"
#include "world/World.h"
#include "world/generation/GenerationNoise.h"
#include "world/settlements/SettlementMap.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <unordered_map>
#include <unordered_set>

namespace Paladin
{
    std::uint64_t WorldPopulationField::fingerprint(const World& world) noexcept
    {
        std::uint64_t hash=1469598103934665603ULL;
        const auto mix=[&](std::uint64_t value) { hash=(hash^value)*1099511628211ULL; };
        mix(world.grid().revision());
        mix(world.populationDistributionRevision());
        mix(world.worldRoadCount());
        for(const auto& city:world.settlements())
        {
            mix(city.id().value()); mix(city.position().x); mix(city.position().y); mix(city.population()); mix(city.isFortress());
            if(const auto* map=city.simulationState().localMap())
            { mix(map->instanceId()); mix(map->objectState().presentationVersion()); mix(city.simulationState().citizens().version()); }
        }
        return hash;
    }
    WorldPopulationField::WorldPopulationField(
        const World& world,
        bool deferred
    )
        : cityCount_(world.settlements().size()), width_(world.grid().width()),
          height_(world.grid().height()), people_(world.grid().tileCount(), 0)
    {
        census_.reserve(cityCount_);
        for(const auto& city:world.settlements())
            census_.push_back({city.id(),city.position(),double(city.population()),city.isFortress()});
        for (const auto& road : world.worldRoads())
        {
            for (const auto p : road.points())
            {
                if (world.grid().isValidPosition(p))
                {
                    roads_.insert(std::size_t(p.y) * width_ + p.x);
                }
            }
        }
        if (!deferred)
        {
            while (!complete())
            {
                advance(world);
            }
        }
    }

    void WorldPopulationField::advance(const World& world)
    {
        if (complete() || width_ <= 0 || height_ <= 0 ||
            width_ != world.grid().width() || height_ != world.grid().height())
        {
            nextCity_ = cityCount_;
            return;
        }
        // One bounded catchment per slice, identified by stable city ID.
        // Headcounts stay at the requested census; a subsequent refresh picks
        // up newer migration/death events without starving this build.
        const auto& snapshot = census_[nextCity_++];
        const auto* city = world.settlement(snapshot.id);
        if (!snapshot.population)
        {
            return;
        }
        const auto centre = snapshot.centre;
        const auto dry = [&](WorldTilePosition p)
        {
            const auto* t = world.grid().tile(p);
            return t && t->terrain == TerrainType::Land;
        };
        const auto index = [&](WorldTilePosition p)
        { return std::size_t(p.y) * width_ + p.x; };
        const auto deposit = [&](WorldTilePosition p, double count)
        {
            p.x = (p.x % width_ + width_) % width_;
            if (dry(p))
            {
                people_[index(p)] += float(count);
            }
        };
        if (const auto* map = city ? city->simulationState().localMap() : nullptr)
        {
            const auto& citizens = city->simulationState().citizens();
            const double residents = double(citizens.residentCount());
            if (residents > 0)
            {
                std::unordered_map<
                    SettlementObjectId,
                    SettlementTilePosition,
                    StrongIdHash>
                    homes;
                SettlementTilePosition keep{};
                for (const auto& object : map->objectState().completedObjects())
                {
                    const auto& f = object.footprint;
                    SettlementTilePosition p{
                        f.topLeft.x + f.width / 2,
                        f.topLeft.y + f.height / 2
                    };
                    homes.emplace(object.id, p);
                    if (object.objectTypeId == SettlementObjectTypes::CityKeep)
                    {
                        keep = p;
                    }
                }
                for (const auto& person : citizens.citizens())
                {
                    if (person.health <= 0 || person.militaryDeployed)
                    {
                        continue;
                    }
                    auto local = person.homeId ? person.tilePosition : keep;
                    if (const auto it = homes.find(person.homeId);
                        it != homes.end())
                    {
                        local = it->second;
                    }
                    WorldTilePosition tile{
                        map->sourceRegionCenter().x -
                            map->sourceRegionWidth() / 2 +
                            int(std::floor(
                                (local.x + .5) / map->localTilesPerWorldTile()
                            )),
                        map->sourceRegionCenter().y -
                            map->sourceRegionHeight() / 2 +
                            int(std::floor(
                                (local.y + .5) / map->localTilesPerWorldTile()
                            ))
                    };
                    tile.x = (tile.x % width_ + width_) % width_;
                    deposit(
                        dry(tile) ? tile : centre,
                        double(snapshot.population) / residents
                    );
                }
                return;
            }
        }
        // The census includes hinterland residents, rather than inventing
        // people to tint ownership. Paths may reach unclaimed countryside but
        // never cross water. Ownership changes do not relocate residents.
        const int radius =
            snapshot.fortress
                ? 1
                : std::clamp(
                      int(9 +
                          std::log2(1 + double(snapshot.population) / 200) * 2),
                      9,
                      28
                  );
        const int side = radius * 2 + 1;
        std::vector<bool> visited(std::size_t(side * side), false);
        std::vector<WorldTilePosition> offsets{{0, 0}};
        visited[std::size_t(radius * side + radius)] = true;
        constexpr std::array<WorldTilePosition, 4> steps{
            {{1, 0}, {-1, 0}, {0, 1}, {0, -1}}
        };
        std::vector<WorldTilePosition> cells;
        std::vector<double> urban, rural;
        double urbanSum = 0, ruralSum = 0;
        for (std::size_t cursor = 0; cursor < offsets.size(); ++cursor)
        {
            const auto offset = offsets[cursor];
            WorldTilePosition p{
                (centre.x + offset.x + width_) % width_,
                centre.y + offset.y
            };
            const auto* tile = world.grid().tile(p);
            if (!tile || tile->terrain == TerrainType::Water)
            {
                continue;
            }
            if (dry(p))
            {
                const double d2 =
                    double(offset.x * offset.x + offset.y * offset.y);
                const double fertility =
                    tile->biome == BiomeType::Polar    ? 0
                    : tile->biome == BiomeType::Desert ? .035
                    : tile->biome == BiomeType::Tundra ? .09
                    : tile->biome == BiomeType::Taiga  ? .35
                                                       : 1;
                const double relief = tile->relief == ReliefType::Mountain
                                          ? .003
                                      : tile->relief == ReliefType::Hills ? .45
                                                                          : 1;
                const double field = GenerationNoise::fractal(
                    p.x * .19,
                    p.y * .19,
                    world.generationSeed() ^ 0xCE115ULL,
                    3
                );
                const double patch =
                    .05 + std::pow(std::clamp((field + 1) * .5, 0., 1.), 3) * 3;
                const double access = roads_.contains(index(p)) ? 2.5 : 1;
                const double u =
                    std::exp(-d2 / (snapshot.fortress ? .7 : 3.8)) *
                    std::max(.1, fertility * relief);
                const double v = fertility * relief * patch * access *
                                 std::exp(-d2 / (radius * radius * .42));
                cells.push_back(p);
                urban.push_back(u);
                rural.push_back(v);
                urbanSum += u;
                ruralSum += v;
            }
            for (const auto d : steps)
            {
                const WorldTilePosition q{offset.x + d.x, offset.y + d.y};
                if (std::abs(q.x) > radius || std::abs(q.y) > radius ||
                    q.x * q.x + q.y * q.y > radius * radius)
                {
                    continue;
                }
                const auto i =
                    std::size_t((q.y + radius) * side + q.x + radius);
                if (!visited[i])
                {
                    visited[i] = true;
                    offsets.push_back(q);
                }
            }
        }
        if (urbanSum <= 0)
        {
            deposit(centre, double(snapshot.population));
            return;
        }
        const double urbanShare = snapshot.fortress || ruralSum <= 0 ? 1 : .58;
        for (std::size_t i = 0; i < cells.size(); ++i)
        {
            deposit(
                cells[i],
                double(snapshot.population) *
                    (urbanShare * urban[i] / urbanSum +
                     (1 - urbanShare) * rural[i] / std::max(1e-30, ruralSum))
            );
        }
    }
    float WorldPopulationField::at(WorldTilePosition p) const noexcept
    { return width_<=0 || p.y<0 || p.y>=height_?0:people_[std::size_t(p.y)*width_+(p.x%width_+width_)%width_]; }
    double WorldPopulationField::total() const noexcept { return std::accumulate(people_.begin(),people_.end(),0.); }
}

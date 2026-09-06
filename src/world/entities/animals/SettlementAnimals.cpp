#include "world/entities/animals/SettlementAnimals.h"
#include "world/generation/GenerationNoise.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

namespace Paladin
{
    namespace
    {
        constexpr std::array speciesDefinitions{
            AnimalSpecies{"cow", "Cow", 2, 1, 3, 8, .68, .48, 0xA77549, .20, 3},
            AnimalSpecies{
                "pig",
                "Pig",
                2,
                1,
                2.5,
                6,
                .58,
                .40,
                0xD49699,
                .30,
                2
            },
            AnimalSpecies{
                "chicken",
                "Chicken",
                1,
                .5,
                1.25,
                2,
                .32,
                .32,
                0xE6DCC3,
                .45,
                1
            }
        };
        int distance(SettlementTilePosition a, SettlementTilePosition b)
        {
            return std::abs(a.x - b.x) + std::abs(a.y - b.y);
        }
    } // namespace
    std::span<const AnimalSpecies> animalSpecies()
    {
        return speciesDefinitions;
    }
    const AnimalSpecies* animalSpecies(std::string_view id)
    {
        for (const auto& d : speciesDefinitions)
        {
            if (d.id == id)
            {
                return &d;
            }
        }
        return nullptr;
    }
    SettlementAnimal* SettlementAnimals::find(EntityId id)
    {
        // Animal IDs use the high-bit entity namespace; never alias citizens.
        const auto index = (id.value() & ~(std::uint64_t(1) << 63));
        return (id.value() >> 63) && index > 0 && index <= animals_.size()
                   ? &animals_[index - 1]
                   : nullptr;
    }
    const SettlementAnimal* SettlementAnimals::find(EntityId id) const
    {
        return const_cast<SettlementAnimals*>(this)->find(id);
    }
    EntityId SettlementAnimals::spawn(
        const SettlementMap& map,
        std::string_view type,
        SettlementTilePosition p
    )
    {
        SettlementNavigation navigation;
        if (!animalSpecies(type) || !navigation.walkable(map, p) ||
            std::any_of(
                animals_.begin(),
                animals_.end(),
                [&](const auto& a)
                {
                    return a.health > 0 &&
                           (a.tilePosition == p ||
                            (a.visualProgress < 1 && a.previousTile == p));
                }
            ))
        {
            return {};
        }
        SettlementAnimal animal;
        animal.id = EntityId{(std::uint64_t(1) << 63) | nextId_++};
        animal.name = animalSpecies(type)->name;
        animal.species = type;
        animal.female = nextId_ % 2 == 0;
        animal.tilePosition = animal.previousTile = animal.herdCenter = p;
        animals_.push_back(std::move(animal));
        return animals_.back().id;
    }
    void SettlementAnimals::initialize(
        const SettlementMap& map,
        std::uint64_t seed
    )
    {
        if (initialized_)
        {
            return;
        }
        initialized_ = true;
        seed_ = seed;
        std::uint64_t sequence = 0;
        const auto roll = [&](int maximum)
        {
            return int(
                GenerationNoise::mix(seed ^ ++sequence) % std::max(1, maximum)
            );
        };
        for (const auto& d : speciesDefinitions)
        {
            for (int herd = 0; herd < policy.herdsPerSpecies; ++herd)
            {
                SettlementTilePosition center{-1, -1};
                for (int attempt = 0; attempt < policy.spawnAttemptsPerHerd;
                     ++attempt)
                {
                    const SettlementTilePosition p{
                        roll(map.grid().width()),
                        roll(map.grid().height())
                    };
                    if (spawn(map, d.id, p))
                    {
                        center = p;
                        break;
                    }
                }
                if (center.x < 0)
                {
                    continue;
                }
                for (int member = 1; member < policy.animalsPerHerd; ++member)
                {
                    for (int attempt = 0; attempt < 24; ++attempt)
                    {
                        const auto id = spawn(
                            map,
                            d.id,
                            {center.x + roll(7) - 3, center.y + roll(7) - 3}
                        );
                        if (id)
                        {
                            find(id)->herdCenter = center;
                            break;
                        }
                    }
                }
            }
        }
    }
    std::size_t SettlementAnimals::designate(
        const SettlementObjectFootprint& area,
        AnimalOrder order
    )
    {
        std::size_t count = 0;
        for (auto& a : animals_)
        {
            if (a.health <= 0 || !area.contains(a.tilePosition) ||
                (order == AnimalOrder::Gather && a.pasture))
            {
                continue;
            }
            if (a.order != order)
            {
                if (a.order == AnimalOrder::None)
                {
                    ++pendingOrders_;
                }
                a.order = order;
                a.handler = {};
                a.reservedPasture = {};
                ++count;
            }
        }
        return count;
    }
    std::size_t SettlementAnimals::cancel(const SettlementObjectFootprint& area)
    {
        std::size_t count = 0;
        for (auto& a : animals_)
        {
            if (area.contains(a.tilePosition) && a.order != AnimalOrder::None)
            {
                a.order = AnimalOrder::None;
                a.handler = {};
                a.reservedPasture = {};
                --pendingOrders_;
                ++count;
            }
        }
        return count;
    }
    void SettlementAnimals::release(CitizenId handler)
    {
        for (auto& a : animals_)
        {
            if (a.tender == handler)
            {
                a.tender = {};
            }
            if (a.handler == handler)
            {
                a.handler = {};
                a.reservedPasture = {};
            }
        }
    }
    int SettlementAnimals::usedSpace(SettlementObjectId pasture) const
    {
        int used = 0;
        for (const auto& a : animals_)
        {
            if (a.health > 0 &&
                (a.pasture == pasture || a.reservedPasture == pasture))
            {
                used += animalSpecies(a.species)->pastureSpace;
            }
        }
        return used;
    }
    int SettlementAnimals::containedCount(SettlementObjectId pasture) const
    {
        return int(std::count_if(
            animals_.begin(),
            animals_.end(),
            [&](const auto& a) { return a.health > 0 && a.pasture == pasture; }
        ));
    }
    bool SettlementAnimals::reserve(
        EntityId id,
        CitizenId handler,
        SettlementObjectId pasture,
        const SettlementMap& map
    )
    {
        auto* a = find(id);
        if (!a || a->health <= 0 || a->handler || a->order == AnimalOrder::None)
        {
            return false;
        }
        if (a->order == AnimalOrder::Gather)
        {
            const auto* object = map.objectState().completedObject(pasture);
            if (!object ||
                object->objectTypeId != SettlementObjectTypes::Pastureland ||
                capacity(object->footprint) <
                    usedSpace(pasture) +
                        animalSpecies(a->species)->pastureSpace)
            {
                return false;
            }
            a->reservedPasture = pasture;
        }
        a->handler = handler;
        return true;
    }
    void SettlementAnimals::follow(
        EntityId id,
        CitizenId handler,
        SettlementTilePosition next,
        const SettlementMap& map
    )
    {
        auto* a = find(id);
        SettlementNavigation navigation;
        if (!a || a->handler != handler ||
            !navigation.canStep(map, a->tilePosition, next))
        {
            return;
        }
        a->previousTile = a->tilePosition;
        a->tilePosition = next;
        a->visualProgress = 0;
    }
    bool SettlementAnimals::contain(
        EntityId id,
        CitizenId handler,
        const SettlementMap& map
    )
    {
        auto* a = find(id);
        const auto* pasture =
            a ? map.objectState().completedObject(a->reservedPasture) : nullptr;
        if (!a || a->handler != handler || !pasture ||
            !pasture->footprint.contains(a->tilePosition))
        {
            return false;
        }
        a->pasture = a->reservedPasture;
        if (a->order != AnimalOrder::None)
        {
            --pendingOrders_;
        }
        a->order = AnimalOrder::None;
        a->reservedPasture = {};
        a->handler = {};
        return true;
    }
    bool SettlementAnimals::hunt(
        EntityId id,
        CitizenId handler,
        SettlementMap& map,
        double minute
    )
    {
        auto* a = find(id);
        if (!a || a->health <= 0 || a->handler != handler ||
            a->order != AnimalOrder::Hunt)
        {
            return false;
        }
        map.logistics.drop(
            a->tilePosition,
            "meat",
            animalSpecies(a->species)->huntMeat,
            minute
        );
        a->health = 0;
        --pendingOrders_;
        a->order = AnimalOrder::None;
        a->handler = {};
        a->pasture = a->reservedPasture = {};
        return true;
    }
    void SettlementAnimals::tick(
        SettlementMap& map,
        const SettlementCitizenState& citizens,
        double minute,
        double elapsed,
        bool move
    )
    {
        SettlementNavigation navigation;
        const auto tileKey = [](SettlementTilePosition p)
        {
            return (std::uint64_t(std::uint32_t(p.x)) << 32) |
                   std::uint32_t(p.y);
        };
        struct HerdCenter
        {
            double x = 0, y = 0;
            int count = 0;
        };
        const auto herdKey = [](const SettlementAnimal& a)
        {
            return a.species + ":" +
                   (a.pasture ? std::to_string(a.pasture.value())
                              : "wild:" + std::to_string(a.herdCenter.x) + ":" +
                                    std::to_string(a.herdCenter.y));
        };
        // Entity-sized indexes, never a pasture-sized or world-sized scan.
        std::unordered_map<std::uint64_t, int> occupied;
        std::unordered_map<std::string, HerdCenter> centers;
        if (move)
        {
            for (const auto& a : animals_)
            {
                if (a.health <= 0)
                {
                    continue;
                }
                ++occupied[tileKey(a.tilePosition)];
                if (a.visualProgress < 1 && a.previousTile != a.tilePosition)
                {
                    ++occupied[tileKey(a.previousTile)];
                }
                auto& center = centers[herdKey(a)];
                center.x += a.tilePosition.x;
                center.y += a.tilePosition.y;
                ++center.count;
            }
        }
        for (auto& a : animals_)
        {
            if (a.health <= 0)
            {
                continue;
            }
            a.ageMinutes += elapsed;
            if (a.juvenile &&
                a.ageMinutes >= animalSpecies(a.species)->maturationDays * 1440)
            {
                a.juvenile = false;
            }
            a.visualProgress = std::min(1.0, a.visualProgress + elapsed);
            const auto* pasture = map.objectState().completedObject(a.pasture);
            if (a.pasture && !pasture)
            {
                a.pasture = {};
                a.herdCenter = a.tilePosition;
            }
            if (!move)
            {
                continue;
            }
            if (a.tender)
            {
                const auto* worker = citizens.citizen(a.tender);
                if (worker && worker->health > 0 &&
                    worker->task.kind == CitizenTaskKind::Work &&
                    worker->task.animal == a.id &&
                    worker->task.object == a.pasture &&
                    minute < a.tendingReservedUntil)
                {
                    continue;
                }
                a.tender = {};
            }
            if (a.handler)
            {
                const auto* c = citizens.citizen(a.handler);
                if (!c || c->task.animal != a.id || c->health <= 0)
                {
                    a.handler = {};
                    a.reservedPasture = {};
                }
                continue;
            }
            if (a.order != AnimalOrder::None || minute < a.nextWanderMinute)
            {
                continue;
            }
            a.nextWanderMinute = minute + std::max(1.0, policy.wanderMinutes);
            const auto random =
                GenerationNoise::mix(seed_ ^ a.id.value() ^ ++a.sequence);
            const std::array<SettlementTilePosition, 4> directions{
                {{1, 0}, {-1, 0}, {0, 1}, {0, -1}}
            };
            if (a.visualProgress < 1)
            {
                continue;
            }
            const auto& center = centers[herdKey(a)];
            SettlementTilePosition next = a.tilePosition;
            double best = -1e30;
            for (std::size_t index = 0; index < directions.size(); ++index)
            {
                const auto d = directions[(index + random) % directions.size()];
                const SettlementTilePosition candidate{
                    a.tilePosition.x + d.x,
                    a.tilePosition.y + d.y
                };
                if (occupied.contains(tileKey(candidate)) ||
                    (pasture ? !pasture->footprint.contains(candidate)
                             : distance(candidate, a.herdCenter) >
                                   policy.roamingRadius) ||
                    !navigation.canStep(map, a.tilePosition, candidate))
                {
                    continue;
                }
                const double separation = std::hypot(
                    candidate.x - center.x / center.count,
                    candidate.y - center.y / center.count
                );
                double score = -std::max(0.0, separation - policy.herdRadius) *
                               policy.cohesionWeight;
                // Prefer a little breathing room, while retaining random
                // wandering.
                for (const auto neighbor : directions)
                {
                    if (occupied.contains(tileKey(
                            {candidate.x + neighbor.x, candidate.y + neighbor.y}
                        )))
                    {
                        score -= .2;
                    }
                }
                score +=
                    double(GenerationNoise::mix(random + index) % 1000) / 1000;
                if (score > best)
                {
                    best = score;
                    next = candidate;
                }
            }
            if (next == a.tilePosition)
            {
                continue;
            }
            ++occupied[tileKey(next)];
            a.previousTile = a.tilePosition;
            a.tilePosition = next;
            a.visualProgress = 0;
        }
        lifeElapsed_ += elapsed;
        if (lifeElapsed_ >= 1)
        {
            breed(map, minute, lifeElapsed_);
            lifeElapsed_ = 0;
        }
    }
    void SettlementAnimals::breed(
        SettlementMap& map,
        double minute,
        double elapsed
    )
    {
        struct Birth
        {
            SettlementObjectId pasture;
            std::string species;
            EntityId mother;
        };
        std::vector<Birth> births;
        struct Herd
        {
            int used = 0;
            std::unordered_set<std::string> males;
        };
        std::unordered_map<SettlementObjectId, Herd, StrongIdHash> herds;
        for (const auto& a : animals_)
        {
            if (a.health <= 0)
            {
                continue;
            }
            if (a.pasture)
            {
                auto& herd = herds[a.pasture];
                herd.used += animalSpecies(a.species)->pastureSpace;
                if (!a.female && !a.juvenile)
                {
                    herd.males.insert(a.species);
                }
            }
            if (a.reservedPasture)
            {
                herds[a.reservedPasture].used +=
                    animalSpecies(a.species)->pastureSpace;
            }
        }
        for (auto& a : animals_)
        {
            if (a.health <= 0 || a.juvenile || !a.female || !a.pasture)
            {
                continue;
            }
            const auto* pasture = map.objectState().completedObject(a.pasture);
            const auto* d = animalSpecies(a.species);
            if (!pasture || herds[a.pasture].used + d->pastureSpace >
                                capacity(pasture->footprint))
            {
                continue;
            }
            const bool mate = herds[a.pasture].males.contains(a.species);
            if (!mate)
            {
                continue;
            }
            if (a.breedingTarget <= 0)
            {
                const auto roll =
                    GenerationNoise::mix(seed_ ^ a.id.value() ^ ++a.sequence);
                a.breedingTarget =
                    -std::log((double(roll >> 11) + 1) / 9007199254740993.0);
            }
            a.breedingExposure +=
                -std::log1p(-d->breedingChancePerDay) * elapsed / 1440;
            if (a.breedingExposure >= a.breedingTarget)
            {
                births.push_back({a.pasture, a.species, a.id});
                a.breedingExposure = 0;
                a.breedingTarget = 0;
            }
        }
        for (const auto& birth : births)
        {
            const auto* object =
                map.objectState().completedObject(birth.pasture);
            if (!object || usedSpace(birth.pasture) +
                                   animalSpecies(birth.species)->pastureSpace >
                               capacity(object->footprint))
            {
                continue;
            }
            const auto f = object->footprint;
            for (int attempt = 0; attempt < 32; ++attempt)
            {
                const auto r = GenerationNoise::mix(
                    seed_ ^ birth.mother.value() ^ std::uint64_t(minute) ^
                    std::uint64_t(attempt)
                );
                const auto id = spawn(
                    map,
                    birth.species,
                    {f.topLeft.x + int(r % f.width),
                     f.topLeft.y + int((r >> 32) % f.height)}
                );
                if (!id)
                {
                    continue;
                }
                auto* baby = find(id);
                baby->pasture = birth.pasture;
                baby->juvenile = true;
                break;
            }
        }
    }
    void SettlementAnimals::produce(
        SettlementMap& map,
        SettlementObjectId pasture,
        int workers,
        double minute,
        double elapsed
    )
    {
        double load = 0;
        for (const auto& a : animals_)
        {
            if (a.health > 0 && a.pasture == pasture)
            {
                load += animalSpecies(a.species)->handlerLoad;
            }
        }
        if (load <= 0 || workers <= 0 || policy.productiveWorkdayMinutes <= 0)
        {
            return;
        }
        const double attended =
            std::min(1.0, workers * policy.handlerCapacity / load);
        const auto* object = map.objectState().completedObject(pasture);
        if (!object)
        {
            return;
        }
        double adultOutput = 0;
        for (const auto& a : animals_)
        {
            if (a.health > 0 && !a.juvenile && a.pasture == pasture)
            {
                adultOutput += animalSpecies(a.species)->foodPerWorkday;
            }
        }
        const auto storage = map.logistics.forObject(pasture);
        const int space = map.logistics.freeSpace(storage);
        if (space <= 0 || elapsed <= 0)
        {
            return;
        }
        // One accumulator per pasture, so fractional output from different
        // animals combines into individual goods rather than herd-sized bursts.
        const double output = std::min(
            double(space),
            elapsed * attended * adultOutput / policy.productiveWorkdayMinutes
        );
        const int amount = std::min(
            space,
            int(map.objectState().accrueProduction(pasture, output))
        );
        if (amount > 0 && map.logistics.add(storage, "meat", amount, minute))
        {
            map.commerce.recordFlow({}, storage, "meat", amount);
        }
    }
} // namespace Paladin

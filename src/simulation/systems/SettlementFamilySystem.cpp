#include "simulation/systems/SettlementActivitySystem.h"
#include "world/generation/GenerationNoise.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include "world/settlements/objects/SettlementDoor.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include <array>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace Paladin
{
    void SettlementCitizenState::matchSingles()
    {
        std::array<std::vector<std::size_t>, 2> singles;
        std::unordered_map<CitizenId, std::size_t, StrongIdHash> dependents;
        for (const auto& c : citizens_)
        {
            if (c.child && c.health > 0)
            {
                if (c.motherId)
                {
                    ++dependents[c.motherId];
                }
                if (c.fatherId)
                {
                    ++dependents[c.fatherId];
                }
            }
        }
        for (std::size_t i = 0; i < citizens_.size(); ++i)
        {
            auto& person = citizens_[i];
            if (person.child || person.spouseId || person.health <= 0)
            {
                continue;
            }
            auto& opposite = singles[person.sex == CitizenSex::Male ? 1 : 0];
            const auto match = std::find_if(
                opposite.begin(),
                opposite.end(),
                [&](std::size_t j)
                {
                    const auto& other = citizens_[j];
                    // A remarriage must not make both existing families
                    // homeless by creating a household larger than a house.
                    return dependents[person.id] + dependents[other.id] <= 2 &&
                           person.birthMotherId != other.id &&
                           person.birthFatherId != other.id &&
                           other.birthMotherId != person.id &&
                           other.birthFatherId != person.id &&
                           !(person.birthMotherId &&
                             person.birthMotherId == other.birthMotherId) &&
                           !(person.birthFatherId &&
                             person.birthFatherId == other.birthFatherId);
                }
            );
            if (match == opposite.end())
            {
                singles[person.sex == CitizenSex::Male ? 0 : 1].push_back(i);
                continue;
            }
            auto& partner = citizens_[*match];
            person.spouseId = partner.id;
            partner.spouseId = person.id;
            *match = opposite.back();
            opposite.pop_back();
            ++familyVersion_;
        }
    }

    void SettlementFamilySystem::update(
        SettlementMap& map,
        SettlementCitizenState& citizens,
        const CitizenSimulationPolicy& policy,
        SettlementActivitySystem& activities,
        double minute,
        double elapsed
    )
    {
        currentMinute_ = minute;
        familyElapsed_ += elapsed;
        if (familyElapsed_ < 1)
        {
            return;
        }
        const double dt = familyElapsed_;
        familyElapsed_ = 0;
        auto& people = citizens.citizens_;
        std::unordered_map<CitizenId, std::size_t, StrongIdHash> index;
        index.reserve(people.size());
        for (std::size_t i = 0; i < people.size(); ++i)
        {
            if (people[i].health > 0)
            {
                index.emplace(people[i].id, i);
            }
        }
        bool changed = false;
        for (auto& c : people)
        {
            if (c.health <= 0)
            {
                continue;
            }
            c.ageMinutes += dt;
            if (!c.birthMotherId)
            {
                c.birthMotherId = c.motherId;
            }
            if (!c.birthFatherId)
            {
                c.birthFatherId = c.fatherId;
            }
            if (c.child)
            {
                if (c.ageMinutes >= policy.childMaturationMinutes)
                {
                    c.child = false;
                    c.ageMinutes -= policy.childMaturationMinutes;
                    c.ageYears = policy.adulthoodAge;
                    c.motherId = {};
                    c.fatherId = {};
                    c.caregiverId = {};
                    changed = true;
                }
                else
                {
                    c.ageYears = std::uint16_t(
                        policy.adulthoodAge * c.ageMinutes /
                        policy.childMaturationMinutes
                    );
                }
            }
            if (!c.child && c.ageMinutes >= policy.adultYearMinutes)
            {
                const auto years =
                    std::uint64_t(c.ageMinutes / policy.adultYearMinutes);
                c.ageYears = std::uint16_t(
                    std::min<std::uint64_t>(
                        UINT16_MAX,
                        std::uint64_t(c.ageYears) + years
                    )
                );
                c.ageMinutes = std::fmod(c.ageMinutes, policy.adultYearMinutes);
            }
            if (c.spouseId && !index.contains(c.spouseId))
            {
                c.spouseId = {};
                c.fertilityExposure = 0;
                changed = true;
            }
        }
        if (changed)
        {
            ++citizens.familyVersion_;
            citizens.matchSingles();
        }
        assignHomes(map, citizens, activities);
        for (auto& c : people)
        {
            c.youngDependents = 0;
            c.caregiverId = {};
        }
        for (auto& c : people)
        {
            if (!c.child || c.ageYears >= policy.independentEatingAge ||
                c.health <= 0)
            {
                continue;
            }
            const auto mother = index.find(c.motherId);
            const auto father = index.find(c.fatherId);
            const auto guardian = mother != index.end() ? mother : father;
            if (guardian != index.end())
            {
                auto& carer = people[guardian->second];
                c.caregiverId = carer.id;
                ++carer.youngDependents;
                // A feeding transfers a small nutritional demand to the
                // parent. It cannot restore the child when the parent is
                // hungry, absent, or has not physically reached home.
                if (carer.homeId && c.homeId == carer.homeId &&
                    carer.insideHome && c.insideHome && carer.hunger < 50 &&
                    carer.task.kind == CitizenTaskKind::Care)
                {
                    const double fed =
                        std::min(c.hunger, policy.nursingHungerPerMinute * dt);
                    c.hunger -= fed;
                    carer.hunger = std::min(
                        100.0,
                        carer.hunger + fed * policy.nursingFoodShare
                    );
                }
            }
        }
        std::unordered_map<
            SettlementObjectId,
            std::vector<std::size_t>,
            StrongIdHash>
            residents;
        for (std::size_t i = 0; i < people.size(); ++i)
        {
            if (people[i].homeId && people[i].health > 0)
            {
                residents[people[i].homeId].push_back(i);
            }
        }
        std::vector<std::pair<CitizenId, CitizenId>> births;
        const double chance =
            std::isfinite(policy.dailyBirthChance)
                ? std::clamp(policy.dailyBirthChance, 0.0, 1.0)
                : 0;
        // Convert daily probability to a timestep-independent hazard.
        const double hazard = chance >= 1
                                  ? std::numeric_limits<double>::infinity()
                                  : -std::log1p(-chance) / 1440;
        for (auto& mother : people)
        {
            if (mother.child || mother.sex != CitizenSex::Female ||
                mother.ageYears >= policy.fertilityEndAge ||
                mother.health <= policy.parentHealthThreshold || !mother.homeId)
            {
                continue;
            }
            const auto partner = index.find(mother.spouseId);
            if (partner == index.end())
            {
                continue;
            }
            const auto& father = people[partner->second];
            if (father.child || father.sex != CitizenSex::Male ||
                father.spouseId != mother.id ||
                father.homeId != mother.homeId ||
                father.health <= policy.parentHealthThreshold)
            {
                continue;
            }
            const auto& household = residents[mother.homeId];
            const bool room = household.size() < 4 ||
                              std::any_of(
                                  household.begin(),
                                  household.end(),
                                  [&](std::size_t i)
                                  {
                                      return !people[i].child &&
                                             !people[i].spouseId &&
                                             people[i].id != mother.id &&
                                             people[i].id != father.id;
                                  }
                              );
            if (!room)
            {
                continue;
            }
            if (mother.fertilityTarget <= 0)
            {
                const auto random = GenerationNoise::mix(
                    citizens.behaviorSeed_ ^ mother.id.value() ^
                    GenerationNoise::mix(++mother.birthSequence)
                );
                const double u =
                    (double(random >> 11) + .5) / 9007199254740992.0;
                mother.fertilityTarget = -std::log(u);
            }
            mother.fertilityExposure += hazard * dt;
            if (hazard > 0 &&
                mother.fertilityExposure >= mother.fertilityTarget)
            {
                births.emplace_back(mother.id, father.id);
                mother.fertilityExposure = 0;
                mother.fertilityTarget = 0;
            }
        }
        // Appending can relocate the citizen vector. Carry IDs/values only
        // across this boundary; never retain a parent pointer or reference.
        for (const auto& [motherId, fatherId] : births)
        {
            const auto homeId = people[index.at(motherId)].homeId;
            const auto* home = map.objectState().completedObject(homeId);
            if (!home)
            {
                continue;
            }
            const auto position = home->door.value_or(home->footprint.topLeft);
            if (!citizens.appendCitizens(1, true))
            {
                continue;
            }
            auto& baby = people.back();
            baby.motherId = motherId;
            baby.fatherId = fatherId;
            baby.birthMotherId = motherId;
            baby.birthFatherId = fatherId;
            baby.homeId = homeId;
            baby.tilePosition = baby.destination = position;
            baby.insideHome = true;
            baby.task.kind = CitizenTaskKind::Home;
            baby.task.object = homeId;
            baby.activity = CitizenActivity::AtHome;
        }
        if (!births.empty())
        {
            assignHomes(map, citizens, activities);
        }
    }

    void SettlementFamilySystem::assignHomes(
        SettlementMap& map,
        SettlementCitizenState& citizens,
        SettlementActivitySystem& activities
    )
    {
        if (housingTopology_ == map.objectState().navigationVersion() &&
            housedPopulation_ == citizens.citizens_.size() &&
            housingFamilies_ == citizens.familyVersion_)
        {
            return;
        }
        housingTopology_ = map.objectState().navigationVersion();
        housedPopulation_ = citizens.citizens_.size();
        housingFamilies_ = citizens.familyVersion_;
        housingCapacity_ = 0;
        auto& people = citizens.citizens_;
        std::unordered_map<CitizenId, std::size_t, StrongIdHash> index;
        for (std::size_t i = 0; i < people.size(); ++i)
        {
            if (people[i].health > 0)
            {
                index.emplace(people[i].id, i);
            }
        }
        struct Home
        {
            SettlementObjectId id;
            int free = 4;
        };
        std::vector<Home> homes;
        std::unordered_map<SettlementObjectId, std::size_t, StrongIdHash>
            homeIndex;
        std::array<std::vector<std::size_t>, 5> vacancies;
        for (const auto& object : map.objectState().completedObjects())
        {
            if (object.objectTypeId != SettlementObjectTypes::House)
            {
                continue;
            }
            homeIndex.emplace(object.id, homes.size());
            vacancies[4].push_back(homes.size());
            homes.push_back({object.id});
            housingCapacity_ += 4;
        }
        const auto rootForAdult = [&](const SettlementCitizen& c)
        {
            return index.contains(c.spouseId) &&
                           c.spouseId.value() < c.id.value()
                       ? c.spouseId
                       : c.id;
        };
        std::vector<std::vector<std::size_t>> groups;
        std::unordered_map<CitizenId, std::size_t, StrongIdHash> groupIndex;
        for (std::size_t i = 0; i < people.size(); ++i)
        {
            const auto& c = people[i];
            if (c.health <= 0)
            {
                continue;
            }
            auto root = rootForAdult(c);
            if (c.child)
            {
                const auto parent = index.find(
                    index.contains(c.motherId) ? c.motherId : c.fatherId
                );
                root = parent != index.end()
                           ? rootForAdult(people[parent->second])
                           : (c.motherId ? c.motherId : c.id);
            }
            auto [entry, inserted] = groupIndex.emplace(root, groups.size());
            if (inserted)
            {
                groups.emplace_back();
            }
            groups[entry->second].push_back(i);
        }
        std::vector<SettlementObjectId> assigned(people.size());
        // Families reserve their shared space before unrelated adult lodgers.
        for (bool families : {true, false})
        {
            for (const auto& group : groups)
            {
                const bool family =
                    group.size() > 1 || people[group.front()].child;
                if (family != families || group.size() > 4)
                {
                    continue;
                }
                std::size_t chosen = homes.size();
                int bestResidents = -1;
                for (auto member : group)
                {
                    const auto existing = homeIndex.find(people[member].homeId);
                    if (existing == homeIndex.end() ||
                        homes[existing->second].free < int(group.size()) ||
                        (family && homes[existing->second].free != 4))
                    {
                        continue;
                    }
                    const int count = int(std::count_if(
                        group.begin(),
                        group.end(),
                        [&](auto i)
                        { return people[i].homeId == existing->first; }
                    ));
                    if (count > bestResidents)
                    {
                        chosen = existing->second;
                        bestResidents = count;
                    }
                }
                if (chosen == homes.size())
                {
                    // Give families room for children before sharing a home
                    // with another family. Packing two couples into one home
                    // prevented both from having children even with empty
                    // houses available elsewhere.
                    for (int offset = 0; offset <= 4 - int(group.size());
                         ++offset)
                    {
                        const int slots =
                            family ? 4 - offset : int(group.size()) + offset;
                        auto& bucket = vacancies[slots];
                        while (!bucket.empty() &&
                               homes[bucket.back()].free != int(slots))
                        {
                            bucket.pop_back();
                        }
                        if (!bucket.empty())
                        {
                            chosen = bucket.back();
                            bucket.pop_back();
                            break;
                        }
                    }
                }
                if (chosen == homes.size())
                {
                    continue;
                }
                auto& home = homes[chosen];
                home.free -= int(group.size());
                vacancies[home.free].push_back(chosen);
                for (auto member : group)
                {
                    assigned[member] = home.id;
                }
            }
        }
        for (std::size_t i = 0; i < people.size(); ++i)
        {
            auto& c = people[i];
            if (c.homeId == assigned[i])
            {
                continue;
            }
            const auto oldHome = c.homeId;
            const auto* old = map.objectState().completedObject(oldHome);
            if (old && old->door && old->footprint.contains(c.tilePosition))
            {
                const auto footprint = old->footprint;
                const auto door = *old->door;
                const auto outside = outsideDoor(footprint, door);
                const bool midStep =
                    c.pathIndex < c.path.size() && c.stepProgress > 0;
                const auto next =
                    midStep ? c.path[c.pathIndex] : c.tilePosition;
                const double progress = c.stepProgress;
                const double duration = c.stepDuration;
                activities.finish(map, c, currentMinute_);
                auto position = c.tilePosition;
                if (midStep)
                {
                    c.path.push_back(next);
                    c.stepProgress = progress;
                    c.stepDuration = duration;
                    position = next;
                }
                while (footprint.contains(position) && position != door)
                {
                    if (position.x != door.x)
                    {
                        position.x += position.x < door.x ? 1 : -1;
                    }
                    else
                    {
                        position.y += position.y < door.y ? 1 : -1;
                    }
                    c.path.push_back(position);
                }
                if (footprint.contains(position))
                {
                    c.path.push_back(outside);
                    position = outside;
                }
                c.destination = position;
                c.exitingHomeId = oldHome;
                c.explicitMovement = true;
            }
            else if (
                c.task.kind == CitizenTaskKind::Home ||
                c.task.kind == CitizenTaskKind::Sleep
            )
            {
                activities.finish(map, c, currentMinute_);
            }
            c.homeId = assigned[i];
            c.insideHome = false;
            if (c.homeId)
            {
                c.homelessMinutes = 0;
            }
        }
    }
} // namespace Paladin

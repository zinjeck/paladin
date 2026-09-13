#include "simulation/RealmRulerSystem.h"
#include "world/World.h"
#include "world/generation/GenerationNoise.h"
#include <cmath>
#include <limits>
#include <map>
#include <tuple>

namespace Paladin
{
    namespace
    {
        std::uint64_t random(RealmRuler& ruler)
        {
            return ruler.randomState = GenerationNoise::mix(ruler.randomState);
        }
        RulerPersonality personality(RealmRuler& ruler)
        {
            return {
                RulerAxis((random(ruler) % 101) / 100.0),
                RulerAxis((random(ruler) % 101) / 100.0),
                RulerAxis((random(ruler) % 101) / 100.0),
                RulerAxis((random(ruler) % 101) / 100.0)
            };
        }
        using Key = std::pair<std::uint64_t, std::uint64_t>;
        struct Person
        {
            Key father{}, mother{};
            const SettlementCitizen* living = nullptr;
        };
        std::map<Key, int> ancestors(
            Key start,
            const std::map<Key, Person>& people
        )
        {
            std::map<Key, int> result;
            std::vector<std::pair<Key, int>> queue{{start, 0}};
            for (std::size_t i = 0; i < queue.size(); ++i)
            {
                const auto [key, depth] = queue[i];
                if (!key.second || result.contains(key))
                {
                    continue;
                }
                result[key] = depth;
                if (const auto at = people.find(key); at != people.end())
                {
                    queue.push_back({at->second.father, depth + 1});
                    queue.push_back({at->second.mother, depth + 1});
                }
            }
            return result;
        }
        void appendAiPerson(RealmRuler& ruler, std::uint64_t father, double age)
        {
            DynasticPerson person;
            person.id = ruler.nextPerson++;
            person.father = father;
            person.age = age;
            person.lifespan = std::max(age + 2, 58.0 + random(ruler) % 33);
            person.nextChildAge = std::max(22.0, age + 3) + random(ruler) % 7;
            person.name = SettlementCitizenState::maleName(random(ruler));
            ruler.family.push_back(std::move(person));
        }
        void inheritAi(RealmRuler& ruler, std::uint64_t id, bool newDynasty)
        {
            ruler.aiRuler = id;
            ruler.vacant = false;
            ruler.personality = personality(ruler);
            ++ruler.reign;
            if (newDynasty)
            {
                ++ruler.dynasty;
            }
            for (const auto& person : ruler.family)
            {
                if (person.id == id)
                {
                    ruler.name = person.name;
                    ruler.age = person.age;
                    break;
                }
            }
        }
    } // namespace

    void RealmRulerSystem::establishPlayer(
        World& world,
        RealmId id,
        std::string_view name
    )
    {
        auto* realm = world.realm(id);
        if (!realm || !realm->capitalSettlementId())
        {
            return;
        }
        auto& ruler = realm->ruler;
        ruler.randomState = GenerationNoise::mix(
            world.generationSeed() ^ id.value() ^ 0xA17ULL
        );
        auto& citizens = world.settlement(realm->capitalSettlementId())
                             ->simulationState()
                             .citizens();
        citizens.chooseFoundingRulerName(name);
        for (const auto& c : citizens.citizens())
        {
            if (c.sex == CitizenSex::Male && c.health > 0)
            {
                ruler.citizen = {realm->capitalSettlementId(), c.id};
                ruler.name = c.name;
                ruler.age = c.ageYears;
                ruler.personality = personality(ruler);
                ruler.dynasty = ruler.reign = 1;
                ruler.vacant = false;
                return;
            }
        }
    }

    void RealmRulerSystem::updatePlayer(World& world, RealmId id)
    {
        auto* realm = world.realm(id);
        if (!realm || !realm->capitalSettlementId())
        {
            return;
        }
        auto& ruler = realm->ruler;
        if (const auto* settlement = world.settlement(ruler.citizen.settlement);
            settlement && settlement->ownerRealmId() == id)
        {
            if (const auto* c =
                    settlement->simulationState().citizens().citizen(
                        ruler.citizen.citizen
                    );
                c && c->health > 0)
            {
                ruler.age = c->ageYears;
                ruler.name = c->name;
                ruler.vacant = false;
                return;
            }
        }
        std::map<Key, Person> people;
        std::vector<Key> candidates;
        for (const auto& settlement : world.settlements())
        {
            if (settlement.ownerRealmId() != id)
            {
                continue;
            }
            const auto sid = settlement.id().value();
            const auto& citizens = settlement.simulationState().citizens();
            for (const auto& a : citizens.ancestors())
            {
                people[{sid, a.id.value()}] =
                    {{sid, a.father.value()}, {sid, a.mother.value()}, nullptr};
            }
            for (const auto& c : citizens.citizens())
            {
                const Key key{sid, c.id.value()};
                people[key] = {
                    {sid,
                     (c.birthFatherId ? c.birthFatherId : c.fatherId).value()},
                    {sid,
                     (c.birthMotherId ? c.birthMotherId : c.motherId).value()},
                    &c
                };
                if (c.sex == CitizenSex::Male && c.health > 0)
                {
                    candidates.push_back(key);
                }
            }
        }
        ruler.vacant = true;
        if (candidates.empty())
        {
            return; // Wait for an actual eligible citizen; never invent one.
        }
        const Key previous{
            ruler.citizen.settlement.value(),
            ruler.citizen.citizen.value()
        };
        const auto previousAncestors = ancestors(previous, people);
        Key successor{};
        auto best = std::tuple{3, std::numeric_limits<int>::max(), 0.0, Key{}};
        for (const auto& candidate : candidates)
        {
            const auto lineage = ancestors(candidate, people);
            int category = 3, distance = std::numeric_limits<int>::max();
            if (people.at(candidate).father == previous && previous.second)
            {
                category = 0;
                distance = 1;
            }
            else if (
                auto it = lineage.find(previous);
                it != lineage.end() && previous.second
            )
            {
                category = 1;
                distance = it->second;
            }
            else
            {
                for (const auto& [ancestor, depth] : lineage)
                {
                    if (auto it = previousAncestors.find(ancestor);
                        it != previousAncestors.end())
                    {
                        category = 2;
                        distance = std::min(distance, depth + it->second);
                    }
                }
            }
            const auto* c = people.at(candidate).living;
            const double age =
                c->child ? c->ageMinutes / (2.0 * 1440 / 16)
                         : c->ageYears + c->ageMinutes / (18.0 * 1440);
            const auto rank = std::tuple{category, distance, -age, candidate};
            if (category < 3 && rank < best)
            {
                best = rank;
                successor = candidate;
            }
        }
        if (!successor.second)
        {
            successor = candidates[random(ruler) % candidates.size()];
            ++ruler.dynasty;
        }
        const auto* c = people.at(successor).living;
        ruler.citizen = {
            SettlementId(successor.first),
            CitizenId(successor.second)
        };
        ruler.name = c->name;
        ruler.age = c->ageYears;
        ruler.personality = personality(ruler);
        ruler.vacant = false;
        ++ruler.reign;
    }

    void RealmRulerSystem::establishAi(World& world, RealmId id)
    {
        auto* realm = world.realm(id);
        if (!realm)
        {
            return;
        }
        realm->aiControlled = true;
        auto& ruler = realm->ruler;
        ruler.randomState = GenerationNoise::mix(
            world.generationSeed() ^ id.value() ^ 0xC0A7ULL
        );
        const double age = 28.0 + double(random(ruler) % 18);
        appendAiPerson(ruler, 0, age);
        appendAiPerson(ruler, 1, std::max(0.0, age - 23));
        appendAiPerson(ruler, 1, std::max(0.0, age - 28));
        // A younger brother provides a collateral branch.
        ruler.family[0].father = 4;
        appendAiPerson(ruler, 0, age + 25);
        ruler.family.back().alive = false;
        appendAiPerson(ruler, 4, age - 5);
        inheritAi(ruler, 1, true);
    }

    void RealmRulerSystem::updateAi(Realm& realm, double minutes)
    {
        auto& ruler = realm.ruler;
        ruler.pendingMinutes += minutes;
        // One court update per game day, sharing the citizens' accelerated age
        // clock.
        constexpr double day = 1440, year = 18 * day;
        while (ruler.pendingMinutes >= day)
        {
            ruler.pendingMinutes -= day;
            std::vector<std::uint64_t> births;
            std::size_t living = 0;
            for (auto& person : ruler.family)
            {
                if (!person.alive)
                {
                    continue;
                }
                person.age += day / year;
                if (person.age >= person.lifespan)
                {
                    person.alive = false;
                    continue;
                }
                ++living;
                if (person.age >= person.nextChildAge && person.age < 50)
                {
                    person.nextChildAge = person.age + 7 + random(ruler) % 7;
                    births.push_back(person.id);
                }
            }
            for (const auto father : births)
            {
                if (living++ < 24)
                {
                    appendAiPerson(ruler, father, 0);
                }
            }
            auto king = std::find_if(
                ruler.family.begin(),
                ruler.family.end(),
                [&](const auto& p) { return p.id == ruler.aiRuler; }
            );
            if (king != ruler.family.end() && king->alive)
            {
                ruler.age = king->age;
                continue;
            }
            // A 0.1% chance per succession, never per frame or per minute.
            const bool extinction = random(ruler) % 1000 == 0;
            std::map<Key, Person> genealogy;
            for (const auto& p : ruler.family)
            {
                genealogy[{1, p.id}] = {{1, p.father}, {}, nullptr};
            }
            const auto oldAncestors = ancestors({1, ruler.aiRuler}, genealogy);
            auto best = std::tuple{
                3,
                std::numeric_limits<int>::max(),
                0.0,
                std::uint64_t(0)
            };
            std::uint64_t successor = 0;
            if (!extinction)
            {
                for (const auto& p : ruler.family)
                {
                    if (!p.alive)
                    {
                        continue;
                    }
                    const auto lineage = ancestors({1, p.id}, genealogy);
                    int kind = 3, distance = std::numeric_limits<int>::max();
                    if (p.father == ruler.aiRuler)
                    {
                        kind = 0;
                        distance = 1;
                    }
                    else if (
                        auto it = lineage.find({1, ruler.aiRuler});
                        it != lineage.end()
                    )
                    {
                        kind = 1;
                        distance = it->second;
                    }
                    else
                    {
                        for (const auto& [a, depth] : lineage)
                        {
                            if (auto it = oldAncestors.find(a);
                                it != oldAncestors.end())
                            {
                                kind = 2;
                                distance =
                                    std::min(distance, depth + it->second);
                            }
                        }
                    }
                    const auto rank = std::tuple{kind, distance, -p.age, p.id};
                    if (kind < 3 && rank < best)
                    {
                        best = rank;
                        successor = p.id;
                    }
                }
            }
            if (!successor)
            {
                ruler.family.clear();
                appendAiPerson(ruler, 0, 25.0 + double(random(ruler) % 21));
                successor = ruler.family.back().id;
                appendAiPerson(
                    ruler,
                    successor,
                    3.0 + double(random(ruler) % 12)
                );
                inheritAi(ruler, successor, true);
            }
            else
            {
                inheritAi(ruler, successor, false);
            }
            // Keep only the living court and the ancestry that connects it.
            std::map<Key, Person> all;
            for (const auto& p : ruler.family)
            {
                all[{1, p.id}] = {{1, p.father}, {}, nullptr};
            }
            std::map<Key, int> retained;
            for (const auto& p : ruler.family)
            {
                if (p.alive)
                {
                    retained.merge(ancestors({1, p.id}, all));
                }
            }
            std::erase_if(
                ruler.family,
                [&](const auto& p) { return !retained.contains({1, p.id}); }
            );
        }
    }

    void RealmRulerSystem::tick(World& world, RealmId player, double minutes)
    {
        updatePlayer(world, player);
        for (const auto& entry : world.realms())
        {
            if (entry.aiControlled)
            {
                bool inhabited = false;
                for (const auto& city : world.settlements())
                {
                    inhabited |= city.ownerRealmId() == entry.id() &&
                                 city.population() > 0;
                }
                auto& realm = *world.realm(entry.id());
                if (inhabited)
                {
                    updateAi(realm, minutes);
                }
                else
                {
                    realm.ruler.vacant = true;
                }
            }
        }
    }
} // namespace Paladin

#pragma once
#include "rendering/WorldRealmQuery.h"
#include <unordered_map>
#include <vector>
namespace Paladin
{
    struct RealmStatistics
    {
        RealmId realm;
        std::uint64_t soldiers = 0, population = 0, size = 0, cities = 0, fortresses = 0;
        Money gold = 0;
    };
    inline std::vector<RealmStatistics> collectRealmStatistics(const World& world, bool area = true)
    {
        std::vector<RealmStatistics> result;
        std::unordered_map<RealmId, std::size_t, StrongIdHash> index;
        for (const auto& realm : world.realms())
        {
            index.emplace(realm.id(),result.size());
            result.push_back({realm.id(),0,0,0,0,0,realm.treasury ? realm.treasury->balance : 0});
        }
        for (const auto& city : world.settlements())
            if (auto it = index.find(city.ownerRealmId()); it != index.end())
            {
                auto& r = result[it->second]; r.population += city.population();
                if (city.isFortress()) ++r.fortresses; else ++r.cities;
            }
        // A deployed soldier belongs to their unit's realm, not the potentially
        // captured recruitment city. Each personnel record is counted once.
        for (const auto& soldier : world.soldiers())
        {
            const auto* unit = world.army(soldier.unitId());
            const auto* home = world.settlement(soldier.homeSettlementId());
            const auto owner = unit ? unit->ownerRealmId() : home ? home->ownerRealmId() : RealmId{};
            if (auto it=index.find(owner); it!=index.end()) ++result[it->second].soldiers;
        }
        if (area)
        {
            const auto& influence=world.tribalInfluence();
            const auto threshold=world.territoryFoundationPolicy().tribalInfluence.visibleInfluenceThreshold;
            for (int y=0;y<world.grid().height();++y)
                for (int x=0;x<world.grid().width();++x)
                {
                    const auto id=worldSurfaceRealm(worldPoliticalSurfaceAt(world,x+.5,y+.5),influence,threshold);
                    if (auto it=index.find(id);it!=index.end()) ++result[it->second].size;
                }
        }
        return result;
    }
}

#pragma once
#include "rendering/WorldPoliticalSurface.h"
#include "world/territory/TribalInfluencePolicy.h"
namespace Paladin
{
    // Picking, thematic paint and area estimates use the SAME dry surface and
    // visible tribal frontier as political rendering. Never write controllers.
    template<class Influence>
    inline RealmId worldSurfaceRealm(const WorldPoliticalSurfaceSample& surface, const Influence& influence, double threshold = TribalInfluencePolicy{}.visibleInfluenceThreshold)
    {
        if (!surface.land) return {};
        if (surface.civic) return surface.civic;
        const auto tribal = worldTribalSurfaceSample(influence, surface);
        return tribal.primaryInfluence > threshold ? tribal.primaryRealm : RealmId{};
    }
    inline RealmId worldRealmAt(const World& world, double x, double y)
    { return worldSurfaceRealm(worldPoliticalSurfaceAt(world,x,y),world.tribalInfluence(),world.territoryFoundationPolicy().tribalInfluence.visibleInfluenceThreshold); }
}

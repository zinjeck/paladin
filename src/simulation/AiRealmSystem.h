#pragma once
#include <cstddef>
namespace Paladin
{
    class World;
    class Realm;
    class Settlement;
    class AiRealmSystem
    {
    public:
        // Daily strategic decisions are bounded independently of render FPS.
        // This is maintenance/diplomacy, not combat or automatic conquest.
        static void tick(World&,double minute,double elapsed);
        static int garrisonTarget(const World&,const Realm&,const Settlement&);
        static constexpr std::size_t MaximumRealmDecisionsPerTick=8;
    };
}

#pragma once
#include "world/RealmRuler.h"
#include <cstdint>
#include <string_view>

namespace Paladin
{
    class World;
    class Realm;
    class RealmRulerSystem
    {
    public:
        static void establishPlayer(World&, RealmId, std::string_view name);
        static void establishAi(World&, RealmId);
        static void tick(World&, RealmId player, double gameMinutes);
        static void updatePlayer(World&, RealmId);
        static void updateAi(Realm&, double gameMinutes);
    };
} // namespace Paladin

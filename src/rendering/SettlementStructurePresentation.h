#pragma once
#include "rendering/CityPresentation.h"
#include "rendering/SceneSpriteLibrary.h"
#include "rendering/SettlementGroundCache.h"
#include "world/settlements/objects/SettlementObjectState.h"
#include <array>

namespace Paladin
{
    class SettlementMap;
    class SettlementCitizenState;
    class SettlementStructurePresentation
    {
    public:
        void prewarmGround(
            Renderer& renderer,
            const SceneProjection& p,
            const SettlementMap& map,
            const SceneSpriteLibrary& sprites
        ) const
        {
            groundCache_.prewarm(renderer, p, map, sprites);
        }

        void submit(
            SceneDrawQueue&,
            const SceneProjection&,
            const SettlementMap&,
            const CityPresentation&,
            const SceneSpriteLibrary&,
            const SettlementCitizenState* citizens = nullptr,
            Renderer* renderer = nullptr
        ) const;

    private:
        struct BuildingCommands
        {
            std::vector<SceneDrawItem> items;
            std::array<double, 9> key{};
            std::shared_ptr<Texture> art;
            SceneProjection projection;
            std::uint64_t used = 0;
            bool valid = false;
        };
        mutable std::unordered_map<std::uint64_t, BuildingCommands>
            buildingCommands_;
        mutable std::uint64_t commandFrame_ = 0;
        mutable SettlementGroundCache groundCache_;
        struct DoorState
        {
            double openness = 0, until = -1;
        };
        mutable std::unordered_map<std::uint64_t, DoorState> doors_;
        mutable double lastDoorTime_ = -1;
        mutable std::uint64_t instance_ = 0, version_ = ~std::uint64_t(0);
        mutable std::
            unordered_map<std::uint64_t, std::vector<SettlementObjectId>>
                chunks_;
        mutable std::vector<SettlementObjectId> large_;
    };
} // namespace Paladin

#pragma once
#include "rendering/CityPresentation.h"
#include "rendering/SceneSpriteLibrary.h"
#include "rendering/SettlementGroundCache.h"
#include "world/settlements/objects/SettlementObjectState.h"

namespace Paladin
{
    class SettlementMap;
    class SettlementCitizenState;
    class SettlementStructurePresentation
    {
    public:
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

#pragma once
#include "rendering/CityPresentation.h"
#include "rendering/SceneSpriteLibrary.h"
#include "world/settlements/objects/SettlementObjectState.h"

namespace Paladin
{
    class SettlementMap;
    class SettlementStructurePresentation
    {
    public:
        void submit(
            SceneDrawQueue&,
            const SceneProjection&,
            const SettlementMap&,
            const CityPresentation&,
            const SceneSpriteLibrary&
        ) const;

    private:
        mutable std::uint64_t instance_ = 0, version_ = ~std::uint64_t(0);
        mutable std::vector<CompletedSettlementObject> buildings_;
    };
} // namespace Paladin

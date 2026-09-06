#pragma once
#include "rendering/ScenePresentation.h"

namespace Paladin
{
    class SettlementAnimals;
    class Camera2D;
    class Renderer;
    class SettlementCitizenState;
    struct TileRenderMetrics;

    class SettlementCitizenRenderer
    {
    public:
        void render(
            Renderer& renderer,
            const SettlementCitizenState& citizens,
            const Camera2D& camera,
            const TileRenderMetrics& metrics,
            const SettlementAnimals* animals = nullptr
        ) const;

    private:
        mutable SceneDrawQueue drawQueue_;
    };
} // namespace Paladin

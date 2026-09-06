#pragma once
#include "rendering/CityPresentation.h"
#include "rendering/ScenePresentation.h"
#include "rendering/SceneSpriteLibrary.h"

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
            const SettlementAnimals* animals = nullptr,
            double interpolationAlpha = 1,
            SceneDrawQueue* shared = nullptr,
            const SceneSpriteLibrary* sprites = nullptr,
            const CityPresentation* policy = nullptr
        ) const;

        void renderAnnotations(Renderer&, double tilePixels) const;

    private:
        struct FishingLine
        {
            float x, y, dx, dy;
        };
        mutable std::vector<FishingLine> fishing_;
        mutable std::vector<std::pair<float, float>> sleeping_;
        mutable SceneDrawQueue drawQueue_;
    };
} // namespace Paladin

#pragma once
#include "core/StrongId.h"
#include "rendering/CityPresentation.h"
#include "rendering/ScenePresentation.h"
#include "rendering/SceneSpriteLibrary.h"

namespace Paladin
{
    inline constexpr RenderColor CitizenPlaceholderColor{217,199,159,255};
    class SettlementAnimals;
    class Camera2D;
    class Renderer;
    class SettlementCitizenState;
    class SettlementEmploymentState;
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
            const CityPresentation* policy = nullptr,
            const SettlementEmploymentState* employment = nullptr,
            CitizenId selectedCitizen = {}
        ) const;

        void renderAnnotations(Renderer&, double tilePixels) const;

    private:
        struct FishingLine
        {
            float x, y, dx, dy;
        };
        mutable std::vector<FishingLine> fishing_;
        mutable std::vector<std::pair<float, float>> sleeping_;
        mutable double animationSeconds_ = 0;
        mutable SceneDrawQueue drawQueue_;
    };
} // namespace Paladin

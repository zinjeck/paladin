#pragma once

#include "rendering/OverlayRenderer.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Paladin
{
    class Camera2D;
    class Renderer;
    class SceneSpriteLibrary;
    class SettlementMap;
    class SettlementObjectPlacementController;
    class SettlementObjectState;
    class Texture;
    struct TileRenderMetrics;

    class SettlementObjectRenderer
    {
    public:
        SettlementObjectRenderer();
        ~SettlementObjectRenderer();

        SettlementObjectRenderer(const SettlementObjectRenderer&) = delete;
        SettlementObjectRenderer& operator=(const SettlementObjectRenderer&) =
            delete;

        void render(
            Renderer& renderer,
            const SettlementMap& settlementMap,
            const Camera2D& camera,
            const TileRenderMetrics& metrics,
            const SettlementObjectPlacementController& placementController,
            const SceneSpriteLibrary& sprites
        ) const;

        void renderOverlay(
            Renderer&,
            const SettlementMap&,
            const Camera2D&,
            const TileRenderMetrics&,
            const SettlementObjectPlacementController&
        ) const;

    private:
        mutable std::uint64_t cachedMapInstance_ = 0;
        mutable std::uint64_t cachedVersion_ = 0;
        mutable std::vector<RenderColor> infrastructurePixels_;
        mutable std::vector<RenderRectangle> awaitingMaterialLines_;
        mutable std::vector<RenderRectangle> readyToBuildLines_;
        mutable std::uint64_t previewMapInstance_ = 0;
        mutable std::uint64_t previewVersion_ = 0;
        mutable std::array<int, 9> previewBounds_{};
        mutable std::string previewType_;
        mutable std::vector<TileOverlayRenderItem> previewOverlays_;
        mutable std::unique_ptr<Texture> cachedInfrastructureTexture_;
        mutable std::vector<TileOutlineRenderItem>
            cachedInfrastructureOutlines_;
        OverlayRenderer overlayRenderer_;
    };
} // namespace Paladin

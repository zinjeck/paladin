#pragma once

#include "rendering/OverlayRenderer.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace Paladin
{
    class Camera2D;
    class Renderer;
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
        SettlementObjectRenderer& operator=(
            const SettlementObjectRenderer&
        ) = delete;

        void render(
            Renderer& renderer,
            const SettlementMap& settlementMap,
            const Camera2D& camera,
            const TileRenderMetrics& metrics,
            const SettlementObjectPlacementController& placementController
        ) const;

    private:
        mutable const SettlementObjectState* cachedState_ = nullptr;
        mutable std::uint64_t cachedVersion_ = 0;
        mutable std::unique_ptr<Texture> cachedInfrastructureTexture_;
        mutable std::vector<TileOutlineRenderItem>
            cachedInfrastructureOutlines_;
        OverlayRenderer overlayRenderer_;
    };
}

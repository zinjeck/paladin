#pragma once

#include "rendering/SettlementMarkerRenderer.h"
#include "rendering/SpriteRenderer.h"
#include "rendering/WorldObjectPresentation.h"
#include "rendering/WorldSurface.h"
#include "rendering/WorldPresentation.h"

#include <optional>
#include <span>

namespace Paladin
{
    class Camera2D;
    class Renderer;
    class World;

    // Strategic objects intentionally sit above the 16-pixel terrain scene.
    // World roads and sprites share the 32-pixel object lattice. Screen-sized
    // symbols and labels are native plates anchored to the same source camera.
    class WorldObjectRenderer
    {
    public:
        void render(
            Renderer& renderer,
            const World& world,
            const Camera2D& renderCamera,
            double effectiveTilePixels,
            bool globe,
            const WorldPresentationState& presentation,
            bool stabilizePixelPhase,
            std::span<const SpriteRenderItem> fallbackSprites = {},
            std::optional<WorldPlacementMarker> placementMarker = std::nullopt,
            WorldSurface::Point3 rigidResidual = {}
        ) const;

    private:
        SettlementMarkerRenderer settlementMarkerRenderer_;
    };
} // namespace Paladin

#pragma once

#include "rendering/LocalTangentWorldView.h"
#include "rendering/SettlementMarkerRenderer.h"
#include "rendering/SpriteRenderer.h"
#include "rendering/WorldObjectPresentation.h"
#include "rendering/WorldPresentation.h"

#include <optional>
#include <span>

namespace Paladin
{
    class Camera2D;
    class Renderer;
    class World;

    // Strategic objects intentionally sit above the 16-pixel terrain scene.
    // Settlements/markers, armies, roads and temporary placement markers all
    // share the 32-pixel object lattice. Natural features do not enter here.
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
            std::optional<LocalTangentWorldView> localTangentPresentation =
                std::nullopt
        ) const;

    private:
        SettlementMarkerRenderer settlementMarkerRenderer_;
    };
} // namespace Paladin

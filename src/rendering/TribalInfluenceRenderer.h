#pragma once

#include "rendering/WorldPresentation.h"

#include <cstdint>
#include <memory>

namespace Paladin
{
    class Camera2D;
    class Renderer;
    class Texture;
    class World;
    struct TileRenderMetrics;

    // Renders tribal authority independently from civic sovereignty. Tribal
    // color is generated from the continuous influence field, so weak frontiers
    // overlap/fade and strong competing fields sharpen without explicit border
    // segments. A separate civic relief pass only darkens the inside of civic
    // borders; it never changes control.
    class TribalInfluenceRenderer
    {
    public:
        TribalInfluenceRenderer();
        ~TribalInfluenceRenderer();

        TribalInfluenceRenderer(const TribalInfluenceRenderer&) = delete;
        TribalInfluenceRenderer& operator=(const TribalInfluenceRenderer&) =
            delete;

        void reset();

        void renderTribalFlat(
            Renderer& renderer,
            const World& world,
            const Camera2D& camera,
            const TileRenderMetrics& metrics,
            const WorldPresentationState& presentation,
            const WorldPresentationPolicy& policy
        );

        void renderTribalGlobe(
            Renderer& renderer,
            const World& world,
            const Camera2D& camera,
            const WorldPresentationState& presentation,
            const WorldPresentationPolicy& policy
        );

        void renderCivicReliefFlat(
            Renderer& renderer,
            const World& world,
            const Camera2D& camera,
            const TileRenderMetrics& metrics,
            const WorldPresentationState& presentation
        );

        void renderCivicReliefGlobe(
            Renderer& renderer,
            const World& world,
            const Camera2D& camera,
            const WorldPresentationState& presentation
        );

    private:
        struct Cache;
        std::unique_ptr<Cache> cache_;
    };
} // namespace Paladin

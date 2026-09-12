#pragma once

#include "rendering/WorldPresentation.h"
#include <cstddef>
#include <cstdint>
#include <memory>

namespace Paladin
{
    class Camera2D;
    class Renderer;
    class World;
    struct TileRenderMetrics;

    // Both projections consume the same coast-clipped political samples. Fill,
    // civic border and restrained relief are prepared together, never independently
    // offset tile rectangles. The simulation's two territorial models are intact.
    class WorldRealmPresentationRenderer
    {
    public:
        WorldRealmPresentationRenderer();
        ~WorldRealmPresentationRenderer();
        WorldRealmPresentationRenderer(const WorldRealmPresentationRenderer&) = delete;
        WorldRealmPresentationRenderer& operator=(const WorldRealmPresentationRenderer&) = delete;
        void reset();
        void renderFlat(Renderer&, const World&, const Camera2D&,
                        const TileRenderMetrics&, const WorldPresentationState&,
                        const WorldPresentationPolicy&);
        void renderGlobe(Renderer&, const World&, const Camera2D&,
                         const WorldPresentationState&, const WorldPresentationPolicy&);
        std::uint64_t cacheBuilds() const noexcept;
        std::size_t detailCacheBytes() const noexcept;
    private:
        struct Cache;
        std::unique_ptr<Cache> cache_;
    };
} // namespace Paladin

#include "rendering/WorldRenderer.h"

#include "world/World.h"

#include "rendering/Camera2D.h"
#include "rendering/TileRenderMetrics.h"
#include "rendering/WorldPixelGrid.h"
#include <SDL3/SDL.h>

#include <utility>

namespace Paladin
{
    WorldRenderer::WorldRenderer()
        : WorldRenderer(defaultTerritoryPresentationPolicy())
    {
    }


    WorldRenderer::WorldRenderer(
        TerritoryPresentationPolicy territoryPresentationPolicy
    )
        : territoryPresentationPolicy_(std::move(territoryPresentationPolicy))
    {
    }


    void WorldRenderer::render(
        Renderer& renderer,
        const World& world,
        const Camera2D& camera,
        const TileRenderMetrics& metrics,
        std::span<const SpriteRenderItem> sprites,
        std::span<const TileOverlayRenderItem> overlays,
        std::span<const TileOutlineRenderItem> outlines
    ) const
    {
        std::string artRoot = std::string(SDL_GetBasePath()) + "assets/sprites";
#ifdef PALADIN_ART_ROOT
        artRoot = PALADIN_ART_ROOT;
#endif
        artwork_.load(renderer, artRoot);
        WorldPixelScene pixelScene(
            renderer,
            metrics.scaledTilePixels(camera.zoom())
        );
        artwork_.setTime(animationSeconds);

        const double tilePixels = metrics.scaledTilePixels(camera.zoom());

        if (!politicalViewInitialized_)
        {
            politicalViewActive_ =
                tilePixels <=
                territoryPresentationPolicy_.enterPoliticalViewTilePixels;

            politicalViewInitialized_ = true;
        }
        else if (
            politicalViewActive_ &&
            tilePixels >
                territoryPresentationPolicy_.exitPoliticalViewTilePixels
        )
        {
            politicalViewActive_ = false;
        }
        else if (
            !politicalViewActive_ &&
            tilePixels <=
                territoryPresentationPolicy_.enterPoliticalViewTilePixels
        )
        {
            politicalViewActive_ = true;
        }

        if (!politicalViewActive_)
        {
            gridRenderer_
                .render(renderer, world.grid(), camera, metrics, &artwork_);
        }
        {
            cartography_.render(
                renderer,
                world,
                {camera.tileX(),
                 camera.tileY(),
                 tilePixels,
                 renderer.outputWidth(),
                 renderer.outputHeight()},
                politicalViewActive_
            );
        }
        auto presentation = territoryPresentationPolicy_;
        presentation.cartographicBase = politicalViewActive_;
        territoryRenderer_.render(
            renderer,
            world,
            camera,
            metrics,
            politicalViewActive_,
            presentation
        );

        spriteRenderer_.render(renderer, sprites, camera, metrics);

        if (!politicalViewActive_)
        {
            settlementMarkerRenderer_
                .render(renderer, world, camera, metrics, &artwork_);
        }

        overlayRenderer_.render(renderer, overlays, camera, metrics);

        overlayRenderer_.renderOutlines(renderer, outlines, camera, metrics);
    }
} // namespace Paladin

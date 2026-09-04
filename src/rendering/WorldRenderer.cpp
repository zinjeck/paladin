#include "rendering/WorldRenderer.h"

#include "world/World.h"

#include "rendering/Camera2D.h"
#include "rendering/TileRenderMetrics.h"

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
        : territoryPresentationPolicy_(
              std::move(territoryPresentationPolicy)
          )
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
        gridRenderer_.render(
            renderer,
            world.grid(),
            camera,
            metrics
        );

        const double tilePixels =
            metrics.scaledTilePixels(camera.zoom());

        if (!politicalViewInitialized_)
        {
            politicalViewActive_ =
                tilePixels <= territoryPresentationPolicy_
                    .enterPoliticalViewTilePixels;

            politicalViewInitialized_ = true;
        }
        else if (
            politicalViewActive_ &&
            tilePixels > territoryPresentationPolicy_
                .exitPoliticalViewTilePixels
        )
        {
            politicalViewActive_ = false;
        }
        else if (
            !politicalViewActive_ &&
            tilePixels <= territoryPresentationPolicy_
                .enterPoliticalViewTilePixels
        )
        {
            politicalViewActive_ = true;
        }

        territoryRenderer_.render(
            renderer,
            world,
            camera,
            metrics,
            politicalViewActive_,
            territoryPresentationPolicy_
        );

        spriteRenderer_.render(
            renderer,
            sprites,
            camera,
            metrics
        );

        if (!politicalViewActive_)
        {
            settlementMarkerRenderer_.render(
                renderer,
                world,
                camera,
                metrics
            );
        }

        overlayRenderer_.render(
            renderer,
            overlays,
            camera,
            metrics
        );

        overlayRenderer_.renderOutlines(
            renderer,
            outlines,
            camera,
            metrics
        );
    }
}

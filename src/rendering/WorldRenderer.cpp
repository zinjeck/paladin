#include "rendering/WorldRenderer.h"

#include "world/World.h"

#include "rendering/Camera2D.h"
#include "rendering/TileRenderMetrics.h"
#include "rendering/WorldFoliage.h"
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
            globeEnabled ? GlobeView::from(
                               camera,
                               world.grid(),
                               renderer.outputWidth(),
                               renderer.outputHeight()
                           )
                                   .radius *
                               6.283185307 / world.grid().width()
                         : metrics.scaledTilePixels(camera.zoom())
        );
        artwork_.setTime(animationSeconds);

        const double tilePixels = metrics.scaledTilePixels(camera.zoom());

        if (globeEnabled)
        {
            globe_
                .render(renderer, world, camera, artwork_, overlays, outlines);
            const auto view = GlobeView::from(
                camera,
                world.grid(),
                renderer.outputWidth(),
                renderer.outputHeight()
            );
            for (const auto& item : sprites)
            {
                if (!item.texture)
                {
                    continue;
                }
                const auto p = view.project(
                    item.tileX / world.grid().width(),
                    item.tileY / world.grid().height()
                );
                if (p.z <= 0)
                {
                    continue;
                }
                const float sw = item.sourceWidth > 0
                                     ? item.sourceWidth
                                     : float(item.texture->width());
                const float sh = item.sourceHeight > 0
                                     ? item.sourceHeight
                                     : float(item.texture->height());
                const float width = float(
                    (item.displayWidthPixels > 0 ? item.displayWidthPixels
                                                 : sw) *
                    camera.zoom()
                );
                const float height = float(
                    (item.displayHeightPixels > 0 ? item.displayHeightPixels
                                                  : sh) *
                    camera.zoom()
                );
                renderer.drawTexture(
                    *item.texture,
                    item.sourceX,
                    item.sourceY,
                    sw,
                    sh,
                    float(p.x) - width * item.anchorX,
                    float(p.y) - height * item.anchorY,
                    width,
                    height
                );
            }
            return;
        }
        politicalViewActive_ = politicalViewRequested;
        if (!politicalViewActive_)
        {
            gridRenderer_
                .render(renderer, world.grid(), camera, metrics, &artwork_);
        }
        if (politicalViewActive_)
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
        if (!politicalViewActive_)
        {
            worldFoliage(
                renderer,
                world,
                {camera.tileX(),
                 camera.tileY(),
                 tilePixels,
                 renderer.outputWidth(),
                 renderer.outputHeight()},
                artwork_
            );
        }
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

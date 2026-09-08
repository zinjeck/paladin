#include "rendering/WorldRenderer.h"

#include "world/World.h"

#include "rendering/Camera2D.h"
#include "rendering/TileRenderMetrics.h"
#include "rendering/WorldFoliage.h"
#include "rendering/WorldPixelGrid.h"
#include <SDL3/SDL.h>

#include <chrono>
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


    bool WorldRenderer::prepareTerrain(
        Renderer& renderer,
        const World& world
    ) const
    {
        std::string artRoot = std::string(SDL_GetBasePath()) + "assets/sprites";
#ifdef PALADIN_ART_ROOT
        artRoot = PALADIN_ART_ROOT;
#endif
        artwork_.load(renderer, artRoot);

        // This path is shown only while world interaction is blocked. Spend a
        // larger but still responsive slice advancing completed GPU uploads so
        // the old one-slice-per-present cadence does not stretch preparation
        // across dozens of otherwise-empty loading frames. Atlas generation is
        // still asynchronous and the exact same pixels/textures are produced.
        constexpr auto preparationSlice = std::chrono::milliseconds(12);
        const auto deadline = std::chrono::steady_clock::now() + preparationSlice;
        do
        {
            const float before = globe_.preparationProgress();
            globe_.updateAtlas(renderer, world, artwork_);
            if (globe_.detailReady())
            {
                return true;
            }
            const float after = globe_.preparationProgress();
            // CPU generation is still running if no upload/progress work was
            // available. Return to the event loop instead of busy-spinning.
            if (after <= before)
            {
                break;
            }
        } while (std::chrono::steady_clock::now() < deadline);
        return globe_.detailReady();
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
            globe_.renderFlat(renderer, world, camera, artwork_, tilePixels);
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
            worldFoliageProjected(
                renderer,
                world,
                camera,
                tilePixels,
                false,
                artwork_
            );
        }
        presentation.cartographicBase = politicalViewActive_;
        // Both projections share canonical map coordinates. The flat view
        // draws one bounded copy; only the globe joins the longitude seam.
        {
            const Camera2D& flat = camera;
            territoryRenderer_.render(
                renderer,
                world,
                flat,
                metrics,
                politicalViewActive_,
                presentation
            );
            spriteRenderer_.render(renderer, sprites, flat, metrics);
            if (!politicalViewActive_)
            {
                settlementMarkerRenderer_
                    .render(renderer, world, flat, metrics, &artwork_);
            }
            overlayRenderer_.render(renderer, overlays, flat, metrics);
            overlayRenderer_.renderOutlines(renderer, outlines, flat, metrics);
        }
    }
    void WorldRenderer::toggleProjection(
        Camera2D& camera,
        const WorldGrid& grid,
        int width,
        int height,
        const TileRenderMetrics& metrics
    )
    {
        const auto uv = WorldSurface::UV{
            camera.tileX() / grid.width(),
            camera.tileY() / grid.height()
        };
        const double globeScale =
            std::min(width, height) * .40 * 6.283185307 / grid.width();
        if (globeEnabled)
        {
            lastGlobeRotation_ =
                GlobeView::from(camera, grid, width, height).orientation();
            camera.setPosition(camera.tileX(), camera.tileY());
            camera.setWorldZoom(
                camera.zoom() * globeScale / metrics.tilePixels
            );
        }
        else
        {
            const double zoom = camera.zoom() * metrics.tilePixels / globeScale;
            if (lastGlobeRotation_)
            {
                camera.setPlanetRotation(
                    *lastGlobeRotation_,
                    grid.width(),
                    grid.height()
                );
            }
            WorldMapNavigation::focus(camera, grid, width, height, true, uv);
            camera.setZoom(std::clamp(zoom, .65, 24.));
        }
        globeEnabled = !globeEnabled;
    }
    void WorldRenderer::renderNavigator(
        Renderer& r,
        const World& world,
        const Camera2D& c,
        const TileRenderMetrics& metrics,
        const GrayUiRenderer& ui
    ) const
    {
        const auto b =
            WorldMapNavigation::mapBounds(r.outputWidth(), r.outputHeight());
        ui.drawPanel(r, {b.x - 3, b.y - 3, b.width + 6, b.height + 6});
        if (const auto* t = globe_.mapTexture(0))
        {
            r.drawTexture(
                *t,
                0,
                0,
                float(t->width()),
                float(t->height()),
                b.x,
                b.y,
                b.width,
                b.height
            );
        }
        const auto view =
            GlobeView::from(c, world.grid(), r.outputWidth(), r.outputHeight());
        // Shade exactly the unseen surface, sampled at bounded HUD resolution.
        std::vector<RenderRectangle> unseen;
        unseen.reserve(2048);
        constexpr int cols = 64, rows = 32;
        for (int y = 0; y < rows; ++y)
        {
            for (int x = 0; x < cols; ++x)
            {
                const double u = (x + .5) / cols, v = (y + .5) / rows;
                bool visible;
                if (globeEnabled)
                {
                    const auto p = view.project(u, v);
                    visible = p.z > 0 && p.x >= 0 && p.x < r.outputWidth() &&
                              p.y >= 0 && p.y < r.outputHeight();
                }
                else
                {
                    double dx = u * world.grid().width() - c.tileX();
                    dx -= std::round(dx / world.grid().width()) *
                          world.grid().width();
                    visible =
                        std::abs(dx) * metrics.scaledTilePixels(c.zoom()) <
                            r.outputWidth() * .5 &&
                        std::abs(v * world.grid().height() - c.tileY()) *
                                metrics.scaledTilePixels(c.zoom()) <
                            r.outputHeight() * .5;
                }
                if (!visible)
                {
                    unseen.push_back(
                        {b.x + x * b.width / cols,
                         b.y + y * b.height / rows,
                         b.width / cols,
                         b.height / rows}
                    );
                }
            }
        }
        r.fillRectangles(unseen, {8, 15, 27, 145});
        for (const auto& city : world.settlements())
        {
            r.fillRectangle(
                b.x +
                    float((city.position().x + .5) / world.grid().width()) *
                        b.width -
                    1,
                b.y +
                    float((city.position().y + .5) / world.grid().height()) *
                        b.height -
                    1,
                3,
                3,
                {238, 190, 100, 255}
            );
        }
        const float x = b.x + float(c.tileX() / world.grid().width()) * b.width,
                    y = b.y +
                        float(c.tileY() / world.grid().height()) * b.height;
        r.drawLine(x - 4, y, x + 4, y, {255, 245, 200, 255});
        r.drawLine(x, y - 4, x, y + 4, {255, 245, 200, 255});
    }
} // namespace Paladin

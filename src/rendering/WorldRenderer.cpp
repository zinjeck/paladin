#include "rendering/WorldRenderer.h"

#include "world/World.h"

#include "rendering/Camera2D.h"
#include "rendering/TileRenderMetrics.h"
#include "rendering/WorldFoliage.h"
#include "rendering/WorldPixelGrid.h"
#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>
#include <vector>

namespace Paladin
{
    namespace
    {
        constexpr double TwoPi = 6.28318530717958647692;
    }

    WorldRenderer::WorldRenderer()
        : WorldRenderer(WorldPresentationPolicy{})
    {
    }


    WorldRenderer::WorldRenderer(WorldPresentationPolicy worldPresentationPolicy)
        : worldPresentationPolicy_(std::move(worldPresentationPolicy))
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

        // This path is shown only while world interaction is blocked. Advance
        // several of GlobeRenderer's existing bounded upload slices per present
        // instead of forcing a complete loading frame between every slice. The
        // cap keeps event/render cadence responsive, while CPU atlas generation
        // remains asynchronous and all final pixels/textures are unchanged.
        constexpr auto preparationSlice = std::chrono::milliseconds(12);
        const auto deadline = std::chrono::steady_clock::now() + preparationSlice;
        for (int pass = 0;
             pass < 3 && std::chrono::steady_clock::now() < deadline;
             ++pass)
        {
            globe_.updateAtlas(renderer, world, artwork_);
            if (globe_.detailReady())
            {
                return true;
            }
        }
        return globe_.detailReady();
    }

    double WorldRenderer::effectiveTilePixels(
        const Renderer& renderer,
        const World& world,
        const Camera2D& camera,
        const TileRenderMetrics& metrics
    ) const noexcept
    {
        if (!globeEnabled)
        {
            return metrics.scaledTilePixels(camera.zoom());
        }

        if (renderer.outputWidth() <= 0 || renderer.outputHeight() <= 0 ||
            world.grid().width() <= 0 || world.grid().height() <= 0)
        {
            return 0.0;
        }

        const auto view = GlobeView::from(
            camera,
            world.grid(),
            renderer.outputWidth(),
            renderer.outputHeight()
        );
        return view.radius * TwoPi / world.grid().width();
    }

    void WorldRenderer::render(
        Renderer& renderer,
        const World& world,
        const Camera2D& camera,
        const TileRenderMetrics& metrics,
        std::span<const SpriteRenderItem> sprites,
        std::span<const TileOverlayRenderItem> overlays,
        std::span<const TileOutlineRenderItem> outlines,
        std::optional<WorldPlacementMarker> placementMarker
    ) const
    {
        std::string artRoot = std::string(SDL_GetBasePath()) + "assets/sprites";
#ifdef PALADIN_ART_ROOT
        artRoot = PALADIN_ART_ROOT;
#endif
        artwork_.load(renderer, artRoot);

        const double presentationTilePixels =
            effectiveTilePixels(renderer, world, camera, metrics);
        if (!std::isfinite(presentationTilePixels) ||
            presentationTilePixels <= 0.0)
        {
            return;
        }

        const WorldPresentationState presentation = worldPresentationState(
            presentationTilePixels,
            worldPresentationPolicy_
        );

        // The authoritative camera remains perfectly continuous. Once the
        // close/local world band is reached, only the camera used for drawing is
        // snapped to one sixteenth of a tile. With WorldPixelScene that means a
        // pan advances the prepared terrain by whole art pixels rather than
        // continuously changing the nearest-neighbour sampling phase. The
        // enter/exit gap prevents a zoom hovering on the threshold from toggling
        // the stabilization every frame.
        pixelStabilityActive_ = worldPixelStabilityActive(
            pixelStabilityActive_,
            presentationTilePixels,
            pixelStabilityPolicy_
        );
        Camera2D renderCamera = camera;
        if (pixelStabilityActive_)
        {
            renderCamera = pixelStableWorldCamera(
                camera,
                world.grid(),
                renderer.outputWidth(),
                renderer.outputHeight(),
                globeEnabled
            );
        }

        WorldPixelScene pixelScene(renderer, presentationTilePixels);
        artwork_.setTime(animationSeconds);

        const double flatTilePixels =
            metrics.scaledTilePixels(renderCamera.zoom());

        if (globeEnabled)
        {
            globe_.render(
                renderer,
                world,
                renderCamera,
                artwork_,
                overlays,
                outlines
            );

            // Realm presentation is now a globe-native layer rather than a
            // flat political-map special case. Fill and labels fade away as the
            // terrain becomes regional; borders remain faint in local view.
            territoryPresentationRenderer_.renderGlobe(
                renderer,
                world,
                renderCamera,
                presentation,
                worldPresentationPolicy_
            );

            const auto view = GlobeView::from(
                renderCamera,
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
                    renderCamera.zoom()
                );
                const float height = float(
                    (item.displayHeightPixels > 0 ? item.displayHeightPixels
                                                  : sh) *
                    renderCamera.zoom()
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

            // The PR #12 cartographic symbol now belongs only to the regional
            // layer. It is absent from distant realm view and fades away before
            // the future physical settlement miniature layer takes over.
            settlementMarkerRenderer_.renderGlobe(
                renderer,
                world,
                renderCamera,
                presentation.settlementMarkerWeight
            );
            if (placementMarker)
            {
                drawSettlementPlacementMarker(
                    renderer,
                    world,
                    renderCamera,
                    presentationTilePixels,
                    *placementMarker
                );
            }
            return;
        }

        // Flat and globe projections share the same semantic zoom policy and
        // the same close-view pixel phase. Terrain is always available
        // underneath; distant realm color is a presentation layer that
        // crossfades away rather than replacing the world with a separate mode.
        globe_.renderFlat(
            renderer,
            world,
            renderCamera,
            artwork_,
            flatTilePixels
        );
        worldFoliageProjected(
            renderer,
            world,
            renderCamera,
            flatTilePixels,
            false,
            artwork_
        );
        territoryPresentationRenderer_.renderFlat(
            renderer,
            world,
            renderCamera,
            metrics,
            presentation,
            worldPresentationPolicy_
        );

        const Camera2D& flat = renderCamera;
        spriteRenderer_.render(renderer, sprites, flat, metrics);
        settlementMarkerRenderer_.renderFlat(
            renderer,
            world,
            flat,
            metrics,
            presentation.settlementMarkerWeight
        );
        overlayRenderer_.render(renderer, overlays, flat, metrics);
        overlayRenderer_.renderOutlines(renderer, outlines, flat, metrics);
        if (placementMarker)
        {
            drawSettlementPlacementMarker(
                renderer,
                world,
                renderCamera,
                presentationTilePixels,
                *placementMarker
            );
        }
    }

    void WorldRenderer::drawSettlementPlacementMarker(
        Renderer& renderer,
        const World& world,
        const Camera2D& camera,
        double tilePixels,
        const WorldPlacementMarker& marker
    ) const
    {
        if (!world.grid().isValidPosition(marker.position) ||
            !std::isfinite(tilePixels) || tilePixels <= 0.0)
        {
            return;
        }

        float centerX = 0.0F;
        float centerY = 0.0F;
        RenderColor color = marker.color;
        if (globeEnabled)
        {
            const auto view = GlobeView::from(
                camera,
                world.grid(),
                renderer.outputWidth(),
                renderer.outputHeight()
            );
            const auto projected = view.project(
                (double(marker.position.x) + 0.5) / world.grid().width(),
                (double(marker.position.y) + 0.5) / world.grid().height()
            );
            if (!std::isfinite(projected.x) || !std::isfinite(projected.y) ||
                projected.z <= 0.0)
            {
                return;
            }
            centerX = float(projected.x);
            centerY = float(projected.y);
            color.alpha = static_cast<std::uint8_t>(std::clamp(
                std::lround(double(color.alpha) *
                            std::clamp(projected.z * 4.0, 0.0, 1.0)),
                0L,
                255L
            ));
        }
        else
        {
            centerX = float(
                renderer.outputWidth() * 0.5 +
                (double(marker.position.x) + 0.5 - camera.tileX()) * tilePixels
            );
            centerY = float(
                renderer.outputHeight() * 0.5 +
                (double(marker.position.y) + 0.5 - camera.tileY()) * tilePixels
            );
        }

        const float radius = std::clamp(float(tilePixels * 0.24), 5.0F, 10.0F);
        RenderColor shadow{8, 15, 27, color.alpha};
        shadow.alpha = static_cast<std::uint8_t>(
            std::lround(double(shadow.alpha) * 0.8)
        );

        // A small surface marker communicates the chosen point without exposing
        // the rectangular settlement-region implementation on the sphere.
        renderer.drawLine(
            centerX,
            centerY - radius - 1.0F,
            centerX + radius + 1.0F,
            centerY,
            shadow
        );
        renderer.drawLine(
            centerX + radius + 1.0F,
            centerY,
            centerX,
            centerY + radius + 1.0F,
            shadow
        );
        renderer.drawLine(
            centerX,
            centerY + radius + 1.0F,
            centerX - radius - 1.0F,
            centerY,
            shadow
        );
        renderer.drawLine(
            centerX - radius - 1.0F,
            centerY,
            centerX,
            centerY - radius - 1.0F,
            shadow
        );

        renderer.drawLine(
            centerX,
            centerY - radius,
            centerX + radius,
            centerY,
            color
        );
        renderer.drawLine(
            centerX + radius,
            centerY,
            centerX,
            centerY + radius,
            color
        );
        renderer.drawLine(
            centerX,
            centerY + radius,
            centerX - radius,
            centerY,
            color
        );
        renderer.drawLine(
            centerX - radius,
            centerY,
            centerX,
            centerY - radius,
            color
        );
        renderer.fillRectangle(centerX - 1.0F, centerY - 1.0F, 3.0F, 3.0F, color);
    }

    void WorldRenderer::toggleProjection(
        Camera2D& camera,
        const WorldGrid& grid,
        int width,
        int height,
        const TileRenderMetrics& metrics
    )
    {
        if (width <= 0 || height <= 0 || grid.width() <= 0 ||
            grid.height() <= 0 || !std::isfinite(metrics.tilePixels) ||
            metrics.tilePixels <= 0.0)
        {
            return;
        }

        const auto uv = WorldSurface::UV{
            camera.tileX() / grid.width(),
            camera.tileY() / grid.height()
        };
        const double globeScale =
            std::min(width, height) * .40 * 6.283185307 / grid.width();
        if (!std::isfinite(globeScale) || globeScale <= 0.0)
        {
            return;
        }

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
        pixelStabilityActive_ = false;
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
        for (const auto& settlement : world.settlements())
        {
            r.fillRectangle(
                b.x +
                    float(
                        (settlement.position().x + .5) / world.grid().width()
                    ) *
                        b.width -
                    1,
                b.y +
                    float(
                        (settlement.position().y + .5) / world.grid().height()
                    ) *
                        b.height -
                    1,
                3,
                3,
                {255, 215, 131, 255}
            );
        }
        const float x = b.x + float(c.tileX() / world.grid().width()) * b.width,
                    y = b.y +
                        float(c.tileY() / world.grid().height()) * b.height;
        r.drawLine(x - 4, y, x + 4, y, {255, 240, 196, 255});
        r.drawLine(x, y - 4, x, y + 4, {255, 240, 196, 255});
    }
} // namespace Paladin

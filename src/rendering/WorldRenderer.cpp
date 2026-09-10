#include "rendering/WorldRenderer.h"

#include "world/World.h"

#include "rendering/Camera2D.h"
#include "rendering/LocalTangentWorldView.h"
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
        constexpr double Pi = 3.14159265358979323846;
        constexpr double TwoPi = Pi * 2.0;
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

        // PR #14's render-only snapping remains useful after the projection
        // change. It now stabilizes translation on the tangent chart rather than
        // trying to make a deforming spherical UV mesh behave like a flat pixel
        // painting. The authoritative camera is still never mutated.
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

        artwork_.setTime(animationSeconds);

        if (globeEnabled)
        {
            const float localWeight = std::clamp(
                presentation.localWorldWeight,
                0.0F,
                1.0F
            );

            // Keep the true sphere through the far/regional scales and beneath
            // the transition. Once the tangent view is fully opaque there is no
            // reason to pay for a hidden spherical terrain pass.
            if (localWeight < 0.999F)
            {
                WorldPixelScene globeScene(renderer, presentationTilePixels);
                globe_.render(
                    renderer,
                    world,
                    renderCamera,
                    artwork_,
                    overlays,
                    outlines
                );
                territoryPresentationRenderer_.renderGlobe(
                    renderer,
                    world,
                    renderCamera,
                    presentation,
                    worldPresentationPolicy_
                );
            }

            if (localWeight > 0.001F)
            {
                const LocalTangentWorldView tangent = LocalTangentWorldView::from(
                    renderCamera,
                    world.grid(),
                    renderer.outputWidth(),
                    renderer.outputHeight(),
                    presentationTilePixels
                );

                // Render a wider unrotated patch, then rotate/scale the finished
                // pixel surface as one rigid image. The final tile scale is
                // still exactly presentationTilePixels, but the overscan keeps
                // rotated corners filled without sampling individual terrain
                // texels through a changing curved mesh.
                const double overscan = tangent.overscanScale();
                const double planarTilePixels =
                    presentationTilePixels / std::max(1.0, overscan);
                const Camera2D planarCamera = tangent.planarCamera();
                TileRenderMetrics planarMetrics;
                planarMetrics.tilePixels = planarTilePixels;
                const std::uint8_t opacity = static_cast<std::uint8_t>(
                    std::clamp(
                        std::lround(255.0 * double(localWeight)),
                        0L,
                        255L
                    )
                );
                const double rotationDegrees =
                    tangent.rollRadians() * 180.0 / Pi;

                WorldPixelScene tangentScene(
                    renderer,
                    planarTilePixels,
                    opacity,
                    rotationDegrees,
                    overscan
                );
                globe_.renderFlat(
                    renderer,
                    world,
                    planarCamera,
                    artwork_,
                    planarTilePixels
                );
                worldFoliageProjected(
                    renderer,
                    world,
                    planarCamera,
                    planarTilePixels,
                    false,
                    artwork_
                );
                territoryPresentationRenderer_.renderFlat(
                    renderer,
                    world,
                    planarCamera,
                    planarMetrics,
                    presentation,
                    worldPresentationPolicy_
                );
                overlayRenderer_.render(
                    renderer,
                    overlays,
                    planarCamera,
                    planarMetrics
                );
                overlayRenderer_.renderOutlines(
                    renderer,
                    outlines,
                    planarCamera,
                    planarMetrics
                );
            }

            // Strategic objects are deliberately not inside the 16-pixel
            // terrain scene. Settlements/markers, armies, roads and temporary
            // placement markers all share the separate 32-pixel object lattice.
            worldObjectRenderer_.render(
                renderer,
                world,
                renderCamera,
                presentationTilePixels,
                true,
                presentation,
                sprites,
                placementMarker
            );
            return;
        }

        const double flatTilePixels =
            metrics.scaledTilePixels(renderCamera.zoom());
        {
            WorldPixelScene flatScene(renderer, presentationTilePixels);
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
            overlayRenderer_.render(renderer, overlays, renderCamera, metrics);
            overlayRenderer_.renderOutlines(
                renderer,
                outlines,
                renderCamera,
                metrics
            );
        }

        worldObjectRenderer_.render(
            renderer,
            world,
            renderCamera,
            presentationTilePixels,
            false,
            presentation,
            sprites,
            placementMarker
        );
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
            std::min(width, height) * .40 * TwoPi / grid.width();
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

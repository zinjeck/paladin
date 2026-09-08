#include "rendering/CityRenderer.h"
#include "rendering/BuildingView.h"
#include "rendering/GrassPresentation.h"
#include "rendering/WorldPixelGrid.h"
#include "ui/UiTypes.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"

#include "interaction/SettlementInspectionController.h"
#include <SDL3/SDL.h>
#include <algorithm>

#include "interaction/SettlementCommandController.h"
#include "interaction/SettlementObjectPlacementController.h"
#include "rendering/Camera2D.h"
#include "rendering/Renderer.h"
#include "rendering/TileRenderMetrics.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/citizens/SettlementCitizenState.h"

namespace Paladin
{
    void CityRenderer::renderMinimap(
        Renderer& renderer,
        const SettlementMap& settlementMap,
        const Camera2D& camera,
        const TileRenderMetrics& metrics,
        const UiRectangle& bounds
    ) const
    {
        constexpr float border = 3.0F;
        if (bounds.width <= border * 2 || bounds.height <= border * 2)
        {
            return;
        }
        const auto& grid = settlementMap.grid();
        const float scale = std::min(
            (bounds.width - border * 2) / grid.width(),
            (bounds.height - border * 2) / grid.height()
        );
        const float width = grid.width() * scale;
        const float height = grid.height() * scale;
        const float x = bounds.x + (bounds.width - width) * 0.5F;
        const float y = bounds.y + (bounds.height - height) * 0.5F;
        gridRenderer_.renderOverview(renderer, x, y, width, height);

        const double tilePixels = metrics.scaledTilePixels(camera.zoom());
        if (tilePixels <= 0.0)
        {
            return;
        }
        const bool sideways = (camera.cityQuarterTurns() & 1) != 0;
        const double halfWidth =
            (sideways ? renderer.outputHeight() : renderer.outputWidth()) /
            tilePixels * 0.5;
        const double halfHeight =
            (sideways ? renderer.outputWidth() : renderer.outputHeight()) /
            tilePixels * 0.5;
        const float left = x + static_cast<float>(std::clamp(
                                   camera.tileX() - halfWidth,
                                   0.0,
                                   double(grid.width())
                               )) * scale;
        const float right = x + static_cast<float>(std::clamp(
                                    camera.tileX() + halfWidth,
                                    0.0,
                                    double(grid.width())
                                )) * scale;
        const float top = y + static_cast<float>(std::clamp(
                                  camera.tileY() - halfHeight,
                                  0.0,
                                  double(grid.height())
                              )) * scale;
        const float bottom = y + static_cast<float>(std::clamp(
                                     camera.tileY() + halfHeight,
                                     0.0,
                                     double(grid.height())
                                 )) * scale;
        if (right <= left || bottom <= top)
        {
            return;
        }
        const RenderColor outline{255, 255, 255, 255};
        const float stroke = std::min({1.5F, right - left, bottom - top});
        renderer.fillRectangle(left, top, right - left, stroke, outline);
        renderer.fillRectangle(
            left,
            bottom - stroke,
            right - left,
            stroke,
            outline
        );
        renderer.fillRectangle(left, top, stroke, bottom - top, outline);
        renderer
            .fillRectangle(right - stroke, top, stroke, bottom - top, outline);
    }

    void CityRenderer::render(
        Renderer& renderer,
        const SettlementMap& settlementMap,
        const Camera2D& camera,
        const TileRenderMetrics& metrics,
        const SettlementObjectPlacementController& placementController,
        const SettlementCommandController& commandController,
        const SettlementCitizenState& citizens,
        const SettlementInspectionController& inspection,
        double interpolationAlpha,
        double hour,
        double sunIncidence
    ) const
    {
        std::string artRoot = std::string(SDL_GetBasePath()) + "assets/sprites";
#ifdef PALADIN_ART_ROOT
        artRoot = PALADIN_ART_ROOT;
#endif
        if (!artRootOverride.empty())
        {
            artRoot = artRootOverride;
        }
        sprites_.load(renderer, artRoot);
        CityPresentation viewPresentation = presentation;
        viewPresentation.viewAzimuthDegrees = camera.cityHeadingDegrees();
        sprites_.setShadowsEnabled(viewPresentation.shadowsVisible);
        WorldPixelScene pixelScene(
            renderer,
            metrics.scaledTilePixels(camera.zoom())
        );
        SceneQuarterTurnScope sceneTurn(camera.cityQuarterTurns());
        struct ResetRendererTurn
        {
            Renderer& renderer;
            ~ResetRendererTurn() { renderer.setQuarterTurnTransform(0); }
        } resetRendererTurn{renderer};

        sprites_.setTime(
            animationTimeOverride >= 0 ? animationTimeOverride
                                       : animationSeconds
        );
        auto timing = SDL_GetTicksNS();
        const auto stage = [&](int index)
        {
            const auto now = SDL_GetTicksNS();
            renderTimings[index] = (now - timing) / 1e6;
            timing = now;
        };

        // Terrain and other map-space raster layers rotate as world geometry.
        // Detailed sprites use SceneProjection below so their anchors rotate
        // while the authored sprites themselves remain upright.
        renderer.setQuarterTurnTransform(camera.cityQuarterTurns());
        gridRenderer_
            .render(renderer, settlementMap.grid(), camera, metrics, &sprites_);
        renderer.setQuarterTurnTransform(0);
        stage(0);

        raised_.clear();
        const SceneProjection projection{
            camera.tileX(),
            camera.tileY(),
            metrics.scaledTilePixels(camera.zoom()),
            renderer.outputWidth(),
            renderer.outputHeight()
        };
        const auto grassStart = raised_.size();
        grass_.submit(
            raised_,
            projection,
            settlementMap,
            sprites_,
            &citizens,
            interpolationAlpha
        );
        raised_.setOpacityFrom(
            grassStart,
            detailBlend(projection.tilePixels, 20, 36)
        );
        stage(1);

        renderer.setQuarterTurnTransform(camera.cityQuarterTurns());
        naturalFeatureRenderer_.render(
            renderer,
            settlementMap,
            camera,
            metrics,
            &raised_,
            &sprites_,
            &viewPresentation
        );
        renderer.setQuarterTurnTransform(0);

        stage(2);
        renderer.setQuarterTurnTransform(camera.cityQuarterTurns());
        objectRenderer_.render(
            renderer,
            settlementMap,
            camera,
            metrics,
            placementController,
            sprites_
        );
        renderer.setQuarterTurnTransform(0);

        const double objectDetail = detailBlend(projection.tilePixels, 16, 28);
        distantObjects_.render(
            renderer,
            projection,
            settlementMap,
            sprites_,
            viewPresentation.roofsVisible,
            1 - objectDetail
        );
        if (objectDetail == 0)
        {
            structures_
                .prewarmGround(renderer, projection, settlementMap, sprites_);
        }
        if (objectDetail > 0)
        {
            const auto start = raised_.size();
            const auto structureStart = SDL_GetTicksNS();
            structures_.submit(
                raised_,
                projection,
                settlementMap,
                viewPresentation,
                sprites_,
                &citizens,
                &renderer
            );
            if (SDL_getenv("PALADIN_CAMERA_PROFILE") &&
                SDL_GetTicksNS() - structureStart > 10000000)
            {
                SDL_Log(
                    "structure submit_ms=%.2f items=%zu",
                    (SDL_GetTicksNS() - structureStart) / 1e6,
                    raised_.size() - start
                );
            }
            raised_.setOpacityFrom(start, objectDetail);
        }
        if (const auto* definition = placementController.activeDefinition())
        {
            if (const auto footprint = placementController.visibleFootprint())
            {
                const auto first = raised_.size();
                tribalBuilding(
                    raised_,
                    projection,
                    sprites_,
                    viewPresentation,
                    std::string(definition->id),
                    *footprint,
                    placementController.visibleDoor(),
                    ~std::uint64_t(0)
                );
                raised_.setOpacityFrom(first, .65);
            }
        }
        citizenRenderer_.render(
            renderer,
            citizens,
            camera,
            metrics,
            &settlementMap.animals,
            interpolationAlpha,
            &raised_,
            &sprites_,
            &viewPresentation
        );
        const auto drawStart = SDL_GetTicksNS();
        raised_.render(renderer, -3, -1);

        renderer.setQuarterTurnTransform(camera.cityQuarterTurns());
        logisticsRenderer_.render(
            renderer,
            settlementMap,
            camera,
            metrics,
            placementController,
            inspection,
            &raised_,
            &sprites_
        );
        renderer.setQuarterTurnTransform(0);
        raised_.render(renderer, 0);
        if (SDL_getenv("PALADIN_CAMERA_PROFILE") &&
            SDL_GetTicksNS() - drawStart > 10000000)
        {
            SDL_Log("queue draw_ms=%.2f", (SDL_GetTicksNS() - drawStart) / 1e6);
        }
        const double weatherTime = animationTimeOverride >= 0
                                       ? animationTimeOverride
                                       : animationSeconds;
        const double weatherDay = solarIllumination(
            std::isfinite(sunIncidence) ? sunIncidence
                                        : globeSunDot(.5, .5, hour * 3600.)
        );
        if (viewPresentation.cloudsEnabled)
        {
            clouds_.render(
                renderer,
                projection,
                weatherTime,
                weatherDay,
                false,
                settlementMap.grid().width(),
                settlementMap.grid().height()
            );
        }
        stage(3);
        lighting_.render(
            renderer,
            projection,
            settlementMap,
            sprites_,
            viewPresentation,
            hour,
            sunIncidence
        );
        if (viewPresentation.cloudsEnabled)
        {
            clouds_.render(
                renderer,
                projection,
                weatherTime,
                weatherDay,
                true,
                settlementMap.grid().width(),
                settlementMap.grid().height()
            );
        }
        stage(4);

        renderer.setQuarterTurnTransform(camera.cityQuarterTurns());
        objectRenderer_.renderOverlay(
            renderer,
            settlementMap,
            camera,
            metrics,
            placementController
        );
        commandRenderer_.render(
            renderer,
            settlementMap.commandState(),
            commandController,
            camera,
            metrics
        );
        renderer.setQuarterTurnTransform(0);

        citizenRenderer_.renderAnnotations(
            renderer,
            metrics.scaledTilePixels(camera.zoom())
        );
        const auto highlight = [&](const RenderRectangle& b)
        {
            const RenderColor selected{0xFF, 0xD7, 0x83, 255};
            renderer.drawLine(b.x, b.y, b.x + b.width, b.y, selected);
            renderer.drawLine(
                b.x,
                b.y + b.height,
                b.x + b.width,
                b.y + b.height,
                selected
            );
            renderer.drawLine(b.x, b.y, b.x, b.y + b.height, selected);
            renderer.drawLine(
                b.x + b.width,
                b.y,
                b.x + b.width,
                b.y + b.height,
                selected
            );
        };
        const auto footprintHighlight = [&](const auto& f)
        {
            highlight(projection.groundBounds(
                double(f.topLeft.x),
                double(f.topLeft.y),
                double(f.width),
                double(f.height)
            ));
        };
        if (const auto* object =
                inspection.selectedObject(settlementMap.objectState()))
        {
            footprintHighlight(object->footprint);
        }
        else if (
            const auto* site =
                inspection.selectedConstructionSite(settlementMap.objectState())
        )
        {
            footprintHighlight(site->footprint);
        }
        else if (
            const auto* pile =
                inspection.selectedInventory(settlementMap.logistics)
        )
        {
            footprintHighlight(pile->footprint);
        }
        else if (const auto* c = inspection.selectedCitizen(citizens))
        {
            highlight(projection.bounds(
                {c->renderX(c->visualX(), interpolationAlpha) + .5,
                 c->renderY(c->visualY(), interpolationAlpha) + .5,
                 0,
                 .65,
                 .65,
                 .5,
                 .5}
            ));
        }
    }
} // namespace Paladin

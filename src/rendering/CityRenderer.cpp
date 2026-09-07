#include "rendering/CityRenderer.h"
#include "rendering/BuildingView.h"
#include "rendering/GrassPresentation.h"
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
        const double halfWidth = renderer.outputWidth() / tilePixels * 0.5;
        const double halfHeight = renderer.outputHeight() / tilePixels * 0.5;
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
        double hour
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
        sprites_.setTime(
            animationTimeOverride >= 0 ? animationTimeOverride
                                       : animationSeconds
        );
        gridRenderer_
            .render(renderer, settlementMap.grid(), camera, metrics, &sprites_);
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
        naturalFeatureRenderer_.render(
            renderer,
            settlementMap,
            camera,
            metrics,
            &raised_,
            &sprites_,
            &presentation
        );

        objectRenderer_.render(
            renderer,
            settlementMap,
            camera,
            metrics,
            placementController,
            sprites_
        );

        const double objectDetail = detailBlend(projection.tilePixels, 16, 28);
        distantObjects_.render(
            renderer,
            projection,
            settlementMap,
            sprites_,
            presentation.roofsVisible,
            1 - objectDetail
        );
        if (objectDetail > 0)
        {
            const auto start = raised_.size();
            structures_.submit(
                raised_,
                projection,
                settlementMap,
                presentation,
                sprites_,
                &citizens,
                &renderer
            );
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
                    presentation,
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
            &presentation
        );
        raised_.render(renderer, -3, -1);
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
        raised_.render(renderer, 0);
        lighting_.render(
            renderer,
            projection,
            settlementMap,
            sprites_,
            presentation,
            hour
        );
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
            highlight(projection.bounds(
                {double(f.topLeft.x),
                 double(f.topLeft.y),
                 0,
                 double(f.width),
                 double(f.height),
                 0,
                 0}
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

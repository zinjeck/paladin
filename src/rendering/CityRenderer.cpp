#include "rendering/CityRenderer.h"
#include "rendering/BuildingView.h"
#include "rendering/CityPixelView.h"
#include "rendering/GrassPresentation.h"
#include "rendering/MarketPresentation.h"
#include "rendering/SelectionOutline.h"
#include "rendering/TransportPresentation.h"
#include "rendering/WorkplaceCompoundPresentation.h"
#include "rendering/WorldPixelGrid.h"
#include "ui/UiTypes.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"

#include "interaction/SettlementInspectionController.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>

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

        // The minimap is the same settlement under the same sky. Reuse the
        // live city's solar illumination instead of leaving the overview stuck
        // at noon while the main scene darkens around it.
        const double night = std::clamp(1.0 - minimapDaylight_, 0.0, 1.0);
        if (night > 0.0)
        {
            renderer.fillRectangle(
                x,
                y,
                width,
                height,
                {13,
                 20,
                 58,
                 static_cast<std::uint8_t>(std::round(178.0 * night))}
            );
        }

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
        const Camera2D& authoritativeCamera,
        const TileRenderMetrics& authoritativeMetrics,
        const SettlementObjectPlacementController& placementController,
        const SettlementCommandController& commandController,
        const SettlementCitizenState& citizens,
        const SettlementInspectionController& inspection,
        double interpolationAlpha,
        double hour,
        double sunIncidence,
        std::span<const BattleSoldierView> battleSoldiers
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
        sprites_.setShadowsEnabled(presentation.shadowsVisible);
        const CityPixelView view(
            authoritativeCamera,
            authoritativeMetrics.scaledTilePixels(authoritativeCamera.zoom()),
            renderer.outputWidth(),
            renderer.outputHeight()
        );
        const Camera2D& camera = view.source;
        TileRenderMetrics metrics = authoritativeMetrics;
        metrics.tilePixels /= view.scale;
        WorldPixelScene pixelScene(
            renderer,
            metrics.scaledTilePixels(camera.zoom()),
            255,
            0,
            view.scale,
            view.offsetX,
            view.offsetY
        );
        pickedMap_ = settlementMap.instanceId();
        pickedWidth_ = renderer.outputWidth();
        pickedHeight_ = renderer.outputHeight();
        pickedScale_ = view.scale;
        pickedX_ = view.offsetX;
        pickedY_ = view.offsetY;
        pickedPitch_ = renderer.currentPixelPitch();
        pickedPixelScene_ = renderer.pixelSceneActive();
        pickedSoftware_ = renderer.usesSoftwareRasterizer();
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
        gridRenderer_
            .render(renderer, settlementMap.grid(), camera, metrics, &sprites_);
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
            detailBlend(projection.tilePixels, 12, 22)
        );
        stage(1);
        naturalFeatureRenderer_.render(
            renderer,
            settlementMap,
            camera,
            metrics,
            &raised_,
            &sprites_,
            &presentation
        );

        stage(2);
        objectRenderer_.render(
            renderer,
            settlementMap,
            camera,
            metrics,
            placementController,
            sprites_
        );

        // Keep authored close-city presentation alive much farther out. The
        // distant layer still crossfades smoothly, but players can frame a
        // large district without dropping immediately to strategic blobs.
        const double objectDetail = detailBlend(projection.tilePixels, 9, 16);
        distantObjects_.render(
            renderer,
            projection,
            settlementMap,
            sprites_,
            presentation.roofsVisible,
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
                presentation,
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
        preview_.clear();
        if (const auto* definition = placementController.activeDefinition())
        {
            if (const auto footprint = placementController.visibleFootprint();
                footprint && placementController.hasDrawablePreview())
            {
                // An adjustable building begins as ONE hover tile. Do not feed
                // that undersized tile to a recipe that expands a barracks to
                // its minimum dimensions, creating a detached carpet beside it.
                const auto first = preview_.size();
                if (definition->id == SettlementObjectTypes::Market)
                {
                    marketStalls(
                        preview_,
                        projection,
                        sprites_,
                        *footprint,
                        ~std::uint64_t(0)
                    );
                }
                else if (workplaceCompound(definition->id))
                {
                    compoundWorkplace(
                        preview_,
                        projection,
                        sprites_,
                        presentation,
                        definition->id,
                        *footprint,
                        ~std::uint64_t(0)
                    );
                }
                else
                {
                    tribalBuilding(
                        preview_,
                        projection,
                        sprites_,
                        presentation,
                        std::string(definition->id),
                        *footprint,
                        placementController.visibleDoor(),
                        ~std::uint64_t(0)
                    );
                }
                preview_.setOpacityFrom(first, .85);
            }
        }
        localTradeCaravans(
            raised_,
            projection,
            sprites_,
            settlementMap.trade,
            gameMinute
        );
        citizenRenderer_.render(
            renderer,
            citizens,
            camera,
            metrics,
            &settlementMap.animals,
            interpolationAlpha,
            &raised_,
            &sprites_,
            &presentation,
            &settlementMap.employment(),
            inspection.selectedCitizen(citizens)
                ? inspection.selectedCitizen(citizens)->id
                : CitizenId{}
        );
        for (const auto& soldier : battleSoldiers)
        {
            static const std::string sprites[2][2] = {
                {"citizen.militia.male.front", "citizen.militia.male.back"},
                {"citizen.militia.female.front", "citizen.militia.female.back"}
            };
            sprites_.submit(
                raised_,
                projection,
                sprites[soldier.female][soldier.player],
                soldier.x + .5,
                soldier.y + .5,
                (std::uint64_t(3) << 61) | soldier.soldier.value(),
                1,
                0
            );
        }
        const auto drawStart = SDL_GetTicksNS();
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
        minimapDaylight_ = weatherDay;
        if (presentation.cloudsEnabled)
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
            presentation,
            hour,
            sunIncidence
        );
        if (presentation.cloudsEnabled)
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
        if (resourcesVisible)
        {
            resourceMap_.render(renderer, settlementMap, projection, sprites_);
        }
        objectRenderer_.renderOverlay(
            renderer,
            settlementMap,
            camera,
            metrics,
            placementController
        );
        preview_.render(renderer);
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
        { drawSelectionBorder(renderer, b, {235, 196, 107, 255}); };
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
            const std::uint64_t actorKey =
                c->soldierId ? (std::uint64_t(3) << 61) | c->soldierId.value()
                             : (std::uint64_t(1) << 62) | c->id.value();
            for (const auto& item : raised_.items())
            {
                if (item.stableId == actorKey && item.layer == 0 &&
                    item.part == 0 && item.texture)
                {
                    sprites_.renderSelection(renderer, item);
                    break;
                }
            }
        }
    }
    CitizenId CityRenderer::citizenAtScreen(
        float x,
        float y,
        const SettlementMap& map,
        const SettlementCitizenState& citizens
    ) const
    {
        if (pickedMap_ != map.instanceId())
        {
            return {};
        }
        // Invert the exact last-frame composite, then sample its snapped scene
        // lattice. Use the submitted body frame, never tile occupancy or
        // shadows.
        const auto sceneSample = [&](double screen, int extent, double offset)
        {
            const double source = std::ceil(extent / pickedPitch_);
            const float destination =
                float(source * pickedPitch_ * pickedScale_);
            const float start =
                float(extent * (1 - pickedScale_) * .5 + offset);
            if (pickedPixelScene_ && pickedSoftware_)
            {
                const auto width = std::max(1, int(destination));
                const auto step =
                    (std::uint64_t(source) << 16) / std::uint64_t(width);
                const auto pixel =
                    std::int64_t(std::floor(screen)) - int(start);
                return double(
                           (std::int64_t(step) * pixel +
                            std::int64_t(step / 2)) >>
                           16
                       ) +
                       .5;
            }
            return std::floor((screen - start) / destination * source) + .5;
        };
        const double sx = sceneSample(x, pickedWidth_, pickedX_);
        const double sy = sceneSample(y, pickedHeight_, pickedY_);
        const auto endpoint = [&](float value)
        {
            if (pickedPixelScene_)
            {
                return std::floor(value / pickedPitch_ + .50001);
            }
            return pickedSoftware_ ? std::trunc(double(value)) : double(value);
        };
        for (auto it = raised_.items().rbegin(); it != raised_.items().rend();
             ++it)
        {
            const auto& item = *it;
            const auto category = item.stableId >> 61;
            if ((category != 2 && category != 3) || item.part != 0 ||
                item.layer != 0)
            {
                continue;
            }
            const double x0 = endpoint(item.bounds.x),
                         y0 = endpoint(item.bounds.y);
            const double x1 = endpoint(item.bounds.x + item.bounds.width),
                         y1 = endpoint(item.bounds.y + item.bounds.height);
            if (sx < x0 || sy < y0 || sx >= x1 || sy >= y1 || x1 <= x0 ||
                y1 <= y0)
            {
                continue;
            }
            if (item.texture)
            {
                const auto sample =
                    [&](double pos, double start, double extent, float source)
                {
                    if (pickedSoftware_)
                    {
                        const auto step = (std::uint64_t(source) << 16) /
                                          std::uint64_t(extent);
                        return int(
                            (step * std::uint64_t(pos - start - .5) +
                             step / 2) >>
                            16
                        );
                    }
                    return std::min(
                        int(source) - 1,
                        int((pos - start) * source / extent)
                    );
                };
                const int tx = int(item.atlasFrame.x) +
                               sample(sx, x0, x1 - x0, item.atlasFrame.width);
                const int ty = int(item.atlasFrame.y) +
                               sample(sy, y0, y1 - y0, item.atlasFrame.height);
                if (!sprites_.opaqueAt(item, tx, ty))
                {
                    continue;
                }
            }
            else if (item.color.alpha < 128)
            {
                continue;
            }
            const auto id = item.stableId & ((std::uint64_t(1) << 61) - 1);
            if (category == 2)
            {
                const auto* person = citizens.citizen(CitizenId{id});
                if (person && !person->militaryDeployed && person->health > 0)
                {
                    return person->id;
                }
            }
            else
            {
                for (const auto& person : citizens.citizens())
                {
                    if (person.soldierId == SoldierId{id} &&
                        !person.militaryDeployed && person.health > 0)
                    {
                        return person.id;
                    }
                }
            }
        }
        return {};
    }
} // namespace Paladin

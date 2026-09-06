#include "rendering/SettlementObjectRenderer.h"
#include "world/settlements/objects/SettlementDoor.h"
#include <SDL3/SDL.h>

#include "interaction/SettlementObjectPlacementController.h"
#include "rendering/Camera2D.h"
#include "rendering/Renderer.h"
#include "rendering/SettlementPlacementPalette.h"
#include "rendering/Texture.h"
#include "rendering/TileRenderMetrics.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include "world/settlements/objects/SettlementObjectState.h"
#include "world/settlements/objects/jobs/market/MarketJob.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace Paladin
{
    namespace
    {
        RenderColor doorColor(
            const SettlementObjectDefinition& definition,
            std::uint8_t alpha = 255
        )
        {
            const auto& color = definition.visual.fillColor;
            return {
                std::uint8_t(color[0] * .78),
                std::uint8_t(color[1] * .78),
                std::uint8_t(color[2] * .78),
                alpha
            };
        }
        bool materialsReady(const SettlementConstructionSite& site)
        {
            return std::all_of(
                site.resourceDeliveries.begin(),
                site.resourceDeliveries.end(),
                [](const auto& cost)
                { return cost.deliveredAmount >= cost.requiredAmount; }
            );
        }
        RenderColor renderColor(
            const std::array<std::uint8_t, 3>& color,
            std::uint8_t alpha
        ) noexcept
        {
            return {color[0], color[1], color[2], alpha};
        }

        void paintFootprint(
            std::vector<RenderColor>& pixels,
            std::int32_t mapWidth,
            const SettlementObjectFootprint& footprint,
            RenderColor fillColor
        )
        {
            for (std::int32_t y = footprint.topLeft.y;
                 y < footprint.topLeft.y + footprint.height;
                 ++y)
            {
                for (std::int32_t x = footprint.topLeft.x;
                     x < footprint.topLeft.x + footprint.width;
                     ++x)
                {
                    pixels
                        [static_cast<std::size_t>(y) *
                             static_cast<std::size_t>(mapWidth) +
                         static_cast<std::size_t>(x)] = fillColor;
                }
            }
        }

        TileOutlineRenderItem footprintOutline(
            const SettlementObjectFootprint& footprint,
            RenderColor color
        ) noexcept
        {
            return {
                static_cast<double>(footprint.topLeft.x),
                static_cast<double>(footprint.topLeft.y),
                static_cast<double>(footprint.width),
                static_cast<double>(footprint.height),
                2.5F,
                color
            };
        }

        bool appendConstructionGrid(
            std::vector<RenderRectangle>& lines,
            const SettlementObjectFootprint& footprint,
            const Camera2D& camera,
            const TileRenderMetrics& metrics,
            const Renderer& renderer,
            std::size_t lineBudget
        )
        {
            const double tilePixels = metrics.scaledTilePixels(camera.zoom());
            const float lineWidth = static_cast<float>(
                std::min(1.0, std::max(0.5, tilePixels * 0.25))
            );
            const double left =
                static_cast<double>(renderer.outputWidth()) * 0.5 +
                (static_cast<double>(footprint.topLeft.x) - camera.tileX()) *
                    tilePixels;
            const double top =
                static_cast<double>(renderer.outputHeight()) * 0.5 +
                (static_cast<double>(footprint.topLeft.y) - camera.tileY()) *
                    tilePixels;
            const double width = footprint.width * tilePixels;
            const double height = footprint.height * tilePixels;

            if (left + width < 0.0 || top + height < 0.0 ||
                left > renderer.outputWidth() || top > renderer.outputHeight())
            {
                return true;
            }

            // Below two screen pixels per tile, individual grid lines cannot
            // be distinguished. The retained fill still presents the site,
            // while skipping thousands of sub-pixel rectangles at far zoom.
            if (tilePixels < 2.0)
            {
                return false;
            }

            const std::int32_t firstColumn = std::clamp(
                static_cast<std::int32_t>(std::floor(-left / tilePixels)),
                0,
                footprint.width
            );
            const std::int32_t lastColumn = std::clamp(
                static_cast<std::int32_t>(
                    std::ceil((renderer.outputWidth() - left) / tilePixels)
                ),
                0,
                footprint.width
            );
            const std::int32_t firstRow = std::clamp(
                static_cast<std::int32_t>(std::floor(-top / tilePixels)),
                0,
                footprint.height
            );
            const std::int32_t lastRow = std::clamp(
                static_cast<std::int32_t>(
                    std::ceil((renderer.outputHeight() - top) / tilePixels)
                ),
                0,
                footprint.height
            );
            const std::size_t newLineCount =
                static_cast<std::size_t>(lastColumn - firstColumn + 1) +
                static_cast<std::size_t>(lastRow - firstRow + 1);
            if (lines.size() + newLineCount > lineBudget)
            {
                return false;
            }

            for (std::int32_t x = firstColumn; x <= lastColumn; ++x)
            {
                lines.push_back(
                    {static_cast<float>(left + x * tilePixels) -
                         lineWidth * 0.5F,
                     static_cast<float>(top),
                     lineWidth,
                     static_cast<float>(height)}
                );
            }

            for (std::int32_t y = firstRow; y <= lastRow; ++y)
            {
                lines.push_back(
                    {static_cast<float>(left),
                     static_cast<float>(top + y * tilePixels) -
                         lineWidth * 0.5F,
                     static_cast<float>(width),
                     lineWidth}
                );
            }

            return true;
        }

        void appendConstructionOuterOutline(
            std::vector<RenderRectangle>& lines,
            const SettlementObjectFootprint& footprint,
            const Camera2D& camera,
            const TileRenderMetrics& metrics,
            const Renderer& renderer
        )
        {
            const double tilePixels = metrics.scaledTilePixels(camera.zoom());
            const float lineWidth = 1.0F;
            const float left = static_cast<float>(
                renderer.outputWidth() * 0.5 +
                (footprint.topLeft.x - camera.tileX()) * tilePixels
            );
            const float top = static_cast<float>(
                renderer.outputHeight() * 0.5 +
                (footprint.topLeft.y - camera.tileY()) * tilePixels
            );
            const float width =
                static_cast<float>(footprint.width * tilePixels);
            const float height =
                static_cast<float>(footprint.height * tilePixels);

            if (left + width < 0.0F || top + height < 0.0F ||
                left > renderer.outputWidth() || top > renderer.outputHeight())
            {
                return;
            }

            lines.push_back({left, top, width, lineWidth});
            lines.push_back({left, top + height - lineWidth, width, lineWidth});
            lines.push_back({left, top, lineWidth, height});
            lines.push_back({left + width - lineWidth, top, lineWidth, height});
        }
    } // namespace

    SettlementObjectRenderer::SettlementObjectRenderer() = default;

    SettlementObjectRenderer::~SettlementObjectRenderer() = default;

    void SettlementObjectRenderer::render(
        Renderer& renderer,
        const SettlementMap& settlementMap,
        const Camera2D& camera,
        const TileRenderMetrics& metrics,
        const SettlementObjectPlacementController& placementController
    ) const
    {
        const SettlementObjectState& state = settlementMap.objectState();

        if (cachedMapInstance_ != settlementMap.instanceId() ||
            cachedVersion_ != state.navigationVersion())
        {
            cachedMapInstance_ = settlementMap.instanceId();
            cachedVersion_ = state.navigationVersion();

            auto& pixels = infrastructurePixels_;
            pixels.assign(
                settlementMap.grid().tileCount(),
                RenderColor{0, 0, 0, 0}
            );

            cachedInfrastructureOutlines_.clear();
            cachedInfrastructureOutlines_.reserve(
                state.completedObjects().size()
            );

            for (const SettlementConstructionSite& site :
                 state.constructionSites())
            {
                const SettlementObjectDefinition* definition =
                    SettlementObjectCatalog::definition(site.objectTypeId);

                if (!definition)
                {
                    continue;
                }

                paintFootprint(
                    pixels,
                    settlementMap.grid().width(),
                    site.footprint,
                    renderColor(definition->visual.fillColor, 105)
                );
                if (site.door && site.footprint.contains(*site.door))
                {
                    auto color = renderColor(definition->visual.fillColor, 105);
                    color = doorColor(*definition, color.alpha);
                    paintFootprint(
                        pixels,
                        settlementMap.grid().width(),
                        {site.door.value(), 1, 1},
                        color
                    );
                }
                if (definition->id != SettlementObjectTypes::Road)
                {
                    cachedInfrastructureOutlines_.push_back(footprintOutline(
                        site.footprint,
                        settlementPlacementOutlineColor(
                            materialsReady(site)
                                ? SettlementPlacementVisualState::ReadyToBuild
                                : SettlementPlacementVisualState::
                                      AwaitingMaterials
                        )
                    ));
                }
            }

            for (const CompletedSettlementObject& object :
                 state.completedObjects())
            {
                const SettlementObjectDefinition* definition =
                    SettlementObjectCatalog::definition(object.objectTypeId);

                if (!definition)
                {
                    continue;
                }

                paintFootprint(
                    pixels,
                    settlementMap.grid().width(),
                    object.footprint,
                    renderColor(definition->visual.fillColor, 255)
                );
                if (object.door && object.footprint.contains(*object.door))
                {
                    auto color = renderColor(definition->visual.fillColor, 255);
                    color = doorColor(*definition, color.alpha);
                    paintFootprint(
                        pixels,
                        settlementMap.grid().width(),
                        {object.door.value(), 1, 1},
                        color
                    );
                }
                if (object.objectTypeId == SettlementObjectTypes::Market)
                {
                    const auto& f = object.footprint;
                    for (int slot = 0;
                         slot < marketStallCount(f.width, f.height);
                         ++slot)
                    {
                        const auto p =
                            marketStallTile(f.topLeft, f.width, f.height, slot);
                        paintFootprint(
                            pixels,
                            settlementMap.grid().width(),
                            {{p.x - 1, p.y - 1},
                             std::min(3, f.topLeft.x + f.width - p.x + 1),
                             1},
                            {150, 49, 38, 255}
                        );
                        paintFootprint(
                            pixels,
                            settlementMap.grid().width(),
                            {{p.x, p.y - 1}, 1, 1},
                            {232, 202, 143, 255}
                        );
                    }
                }

                if (definition->id != SettlementObjectTypes::Road)
                {
                    cachedInfrastructureOutlines_.push_back(footprintOutline(
                        object.footprint,
                        renderColor(definition->visual.frameColor, 255)
                    ));
                }
            }

            const int width = settlementMap.grid().width(),
                      height = settlementMap.grid().height();
            if (!cachedInfrastructureTexture_ ||
                cachedInfrastructureTexture_->width() != width ||
                cachedInfrastructureTexture_->height() != height ||
                !renderer.updateTexturePixels(
                    *cachedInfrastructureTexture_,
                    pixels
                ))
            {
                cachedInfrastructureTexture_ =
                    renderer.createTextureFromPixels(width, height, pixels);
            }
        }

        if (cachedInfrastructureTexture_)
        {
            const double tilePixels = metrics.scaledTilePixels(camera.zoom());

            const double viewportWidth =
                static_cast<double>(renderer.outputWidth());

            const double viewportHeight =
                static_cast<double>(renderer.outputHeight());

            renderer.drawTexture(
                *cachedInfrastructureTexture_,
                0.0F,
                0.0F,
                static_cast<float>(settlementMap.grid().width()),
                static_cast<float>(settlementMap.grid().height()),
                static_cast<float>(
                    viewportWidth * 0.5 - camera.tileX() * tilePixels
                ),
                static_cast<float>(
                    viewportHeight * 0.5 - camera.tileY() * tilePixels
                ),
                static_cast<float>(
                    static_cast<double>(settlementMap.grid().width()) *
                    tilePixels
                ),
                static_cast<float>(
                    static_cast<double>(settlementMap.grid().height()) *
                    tilePixels
                )
            );
        }

        overlayRenderer_.renderOutlines(
            renderer,
            cachedInfrastructureOutlines_,
            camera,
            metrics
        );

        auto& awaitingMaterialLines = awaitingMaterialLines_;
        auto& readyToBuildLines = readyToBuildLines_;
        awaitingMaterialLines.clear();
        readyToBuildLines.clear();
        constexpr std::size_t constructionGridLineBudget = 20'000;
        bool awaitingDetailedGrid = true;
        bool readyDetailedGrid = true;

        for (const SettlementConstructionSite& site : state.constructionSites())
        {
            std::vector<RenderRectangle>& lines = materialsReady(site)
                                                      ? readyToBuildLines
                                                      : awaitingMaterialLines;

            bool& detailedGrid =
                materialsReady(site) ? readyDetailedGrid : awaitingDetailedGrid;
            if (!detailedGrid)
            {
                continue;
            }

            detailedGrid = appendConstructionGrid(
                lines,
                site.footprint,
                camera,
                metrics,
                renderer,
                constructionGridLineBudget
            );
        }

        if (!awaitingDetailedGrid || !readyDetailedGrid)
        {
            if (!awaitingDetailedGrid)
            {
                awaitingMaterialLines.clear();
            }
            if (!readyDetailedGrid)
            {
                readyToBuildLines.clear();
            }

            for (const SettlementConstructionSite& site :
                 state.constructionSites())
            {
                const bool ready = materialsReady(site);
                if ((ready && readyDetailedGrid) ||
                    (!ready && awaitingDetailedGrid))
                {
                    continue;
                }

                appendConstructionOuterOutline(
                    ready ? readyToBuildLines : awaitingMaterialLines,
                    site.footprint,
                    camera,
                    metrics,
                    renderer
                );
            }
        }

        renderer.fillRectangles(
            awaitingMaterialLines,
            settlementPlacementOutlineColor(
                SettlementPlacementVisualState::AwaitingMaterials
            )
        );
        renderer.fillRectangles(
            readyToBuildLines,
            settlementPlacementOutlineColor(
                SettlementPlacementVisualState::ReadyToBuild
            )
        );

        const double doorPixels = metrics.scaledTilePixels(camera.zoom());
        const auto paintDoor = [&](SettlementTilePosition p,
                                   const SettlementObjectDefinition& style)
        {
            const float x = float(
                renderer.outputWidth() * .5 +
                (p.x - camera.tileX()) * doorPixels
            );
            const float y = float(
                renderer.outputHeight() * .5 +
                (p.y - camera.tileY()) * doorPixels
            );
            if (x + doorPixels < 0 || y + doorPixels < 0 ||
                x > renderer.outputWidth() || y > renderer.outputHeight())
            {
                return;
            }
            const float border =
                std::clamp(float(doorPixels) * .09F, 1.0F, 3.0F);
            renderer.fillRectangle(
                x,
                y,
                float(doorPixels),
                float(doorPixels),
                doorColor(style)
            );
            if (doorPixels > border * 2)
            {
                renderer.fillRectangle(
                    x + border,
                    y + border,
                    float(doorPixels) - border * 2,
                    float(doorPixels) - border * 2,
                    doorColor(style)
                );
            }
        };
        for (const auto& object :
             settlementMap.objectState().completedObjects())
        {
            if (const auto* style =
                    SettlementObjectCatalog::definition(object.objectTypeId);
                object.door && style && style->hasDoor)
            {
                paintDoor(*object.door, *style);
            }
        }
        for (const auto& site : settlementMap.objectState().constructionSites())
        {
            if (const auto* style =
                    SettlementObjectCatalog::definition(site.objectTypeId);
                site.door && style && style->hasDoor)
            {
                paintDoor(*site.door, *style);
            }
        }
        const auto drawDoor = [&]()
        {
            const auto footprint = placementController.visibleFootprint();
            if (!footprint)
            {
                return;
            }
            const auto door = placementController.visibleDoor();
            const double pixels = metrics.scaledTilePixels(camera.zoom());
            if (door)
            {
                const float x = float(
                    renderer.outputWidth() * .5 +
                    (door->x - camera.tileX()) * pixels
                );
                const float y = float(
                    renderer.outputHeight() * .5 +
                    (door->y - camera.tileY()) * pixels
                );
                if (const auto* style = placementController.activeDefinition())
                {
                    paintDoor(*door, *style);
                }
                const auto outside = outsideDoor(*footprint, *door);
                const float dx = float(outside.x - door->x),
                            dy = float(outside.y - door->y);
                for (float offset : {0.0F, .28F})
                {
                    const float cx = x + float(pixels) * (.5F + dx * offset),
                                cy = y + float(pixels) * (.5F + dy * offset);
                    const float size = float(pixels) * .22F;
                    renderer.drawLine(
                        cx - dx * size - dy * size,
                        cy - dy * size + dx * size,
                        cx + dx * size,
                        cy + dy * size,
                        {220, 245, 220, 255}
                    );
                    renderer.drawLine(
                        cx - dx * size + dy * size,
                        cy - dy * size - dx * size,
                        cx + dx * size,
                        cy + dy * size,
                        {220, 245, 220, 255}
                    );
                }
            }
            if (placementController.choosingDoor())
            {
                float x = 0, y = 0;
                SDL_GetMouseState(&x, &y);
                renderer
                    .fillRectangle(x + 12, y + 12, 12, 17, {127, 79, 36, 255});
                renderer.drawLine(
                    x + 12,
                    y + 12,
                    x + 24,
                    y + 12,
                    {62, 35, 17, 255}
                );
                renderer.drawLine(
                    x + 12,
                    y + 12,
                    x + 12,
                    y + 29,
                    {62, 35, 17, 255}
                );
                renderer.drawLine(
                    x + 24,
                    y + 12,
                    x + 24,
                    y + 29,
                    {62, 35, 17, 255}
                );
                renderer
                    .fillRectangle(x + 20, y + 21, 2, 2, {225, 196, 100, 255});
            }
        };
        const std::optional<SettlementObjectFootprint> preview =
            placementController.visibleFootprint();

        const SettlementObjectDefinition* definition =
            placementController.activeDefinition();

        if (!preview || !definition)
        {
            return;
        }

        if (placementController.choosingDoor())
        {
            std::vector<TileOverlayRenderItem> tiles;
            tiles.push_back(
                {double(preview->topLeft.x),
                 double(preview->topLeft.y),
                 double(preview->width),
                 double(preview->height),
                 settlementPlacementFillColor(
                     SettlementPlacementVisualState::Valid
                 )}
            );
            if (const auto hover = placementController.hoveredTile())
            {
                const bool valid =
                    placementController.doorTileIsValid(*hover, settlementMap);
                tiles.push_back(
                    {double(hover->x),
                     double(hover->y),
                     1,
                     1,
                     valid ? RenderColor{72, 220, 112, 240}
                           : RenderColor{232, 70, 70, 240}}
                );
            }
            overlayRenderer_.render(renderer, tiles, camera, metrics);
            drawDoor();
            return;
        }
        if (definition->allowsPartialPlacement)
        {
            const double tilePixels = metrics.scaledTilePixels(camera.zoom());
            const std::int32_t visibleLeft = std::max(
                preview->topLeft.x,
                static_cast<std::int32_t>(std::floor(
                    camera.tileX() - renderer.outputWidth() * 0.5 / tilePixels
                )) - 1
            );
            const std::int32_t visibleTop = std::max(
                preview->topLeft.y,
                static_cast<std::int32_t>(std::floor(
                    camera.tileY() - renderer.outputHeight() * 0.5 / tilePixels
                )) - 1
            );
            const std::int32_t visibleRight = std::min(
                preview->topLeft.x + preview->width,
                static_cast<std::int32_t>(std::ceil(
                    camera.tileX() + renderer.outputWidth() * 0.5 / tilePixels
                )) + 1
            );
            const std::int32_t visibleBottom = std::min(
                preview->topLeft.y + preview->height,
                static_cast<std::int32_t>(std::ceil(
                    camera.tileY() + renderer.outputHeight() * 0.5 / tilePixels
                )) + 1
            );
            auto& tileOverlays = previewOverlays_;
            const std::array<int, 9> bounds{
                preview->topLeft.x,
                preview->topLeft.y,
                preview->width,
                preview->height,
                visibleLeft,
                visibleTop,
                visibleRight,
                visibleBottom,
                tilePixels >= 2.0
            };
            if (previewMapInstance_ != settlementMap.instanceId() ||
                previewVersion_ != state.navigationVersion() ||
                previewBounds_ != bounds || previewType_ != definition->id)
            {
                previewMapInstance_ = settlementMap.instanceId();
                previewVersion_ = state.navigationVersion();
                previewBounds_ = bounds;
                previewType_ = definition->id;
                tileOverlays.clear();
                tileOverlays.reserve(
                    static_cast<std::size_t>(
                        std::max(0, visibleBottom - visibleTop)
                    ) +
                    1U
                );

                // Paint the selectable area once, then cover only contiguous
                // blocked runs. This keeps a large road drag independent of its
                // valid tile count instead of issuing one draw per tile.
                tileOverlays.push_back(
                    {static_cast<double>(preview->topLeft.x),
                     static_cast<double>(preview->topLeft.y),
                     static_cast<double>(preview->width),
                     static_cast<double>(preview->height),
                     settlementPlacementFillColor(
                         SettlementPlacementVisualState::Valid
                     )}
                );

                const std::size_t visibleTileCount =
                    static_cast<std::size_t>(
                        std::max(0, visibleRight - visibleLeft)
                    ) *
                    static_cast<std::size_t>(
                        std::max(0, visibleBottom - visibleTop)
                    );
                if (tilePixels >= 2.0 && visibleTileCount <= 50'000U)
                {
                    for (std::int32_t y = visibleTop; y < visibleBottom; ++y)
                    {
                        std::optional<std::int32_t> blockedRunStart;

                        for (std::int32_t x = visibleLeft; x <= visibleRight;
                             ++x)
                        {
                            const bool inFootprint = x < visibleRight;
                            const bool blocked =
                                inFootprint &&
                                settlementMap.objectState().placementStatusAt(
                                    settlementMap.grid(),
                                    *definition,
                                    {x, y}
                                ) != SettlementTilePlacementStatus::Buildable;

                            if (blocked && !blockedRunStart)
                            {
                                blockedRunStart = x;
                            }
                            else if (!blocked && blockedRunStart)
                            {
                                tileOverlays.push_back(
                                    {static_cast<double>(*blockedRunStart),
                                     static_cast<double>(y),
                                     static_cast<double>(x - *blockedRunStart),
                                     1.0,
                                     settlementPlacementFillColor(
                                         SettlementPlacementVisualState::Invalid
                                     )}
                                );
                                blockedRunStart.reset();
                            }
                        }
                    }
                }
            }
            const SettlementPlacementAreaEvaluation evaluation =
                settlementMap.objectState().evaluatePlacementArea(
                    settlementMap.grid(),
                    *definition,
                    *preview
                );

            const std::array<TileOutlineRenderItem, 1> selectionOutline{
                {{static_cast<double>(preview->topLeft.x),
                  static_cast<double>(preview->topLeft.y),
                  static_cast<double>(preview->width),
                  static_cast<double>(preview->height),
                  2.0F,
                  settlementPlacementOutlineColor(
                      placementController.hasLockedFootprint()
                          ? SettlementPlacementVisualState::Valid
                          : (evaluation.footprintAllowed &&
                                     !evaluation.hasObstructions()
                                 ? SettlementPlacementVisualState::Valid
                                 : SettlementPlacementVisualState::Invalid)
                  )}}
            };

            overlayRenderer_.render(renderer, tileOverlays, camera, metrics);

            overlayRenderer_
                .renderOutlines(renderer, selectionOutline, camera, metrics);

            drawDoor();
            return;
        }

        const bool valid =
            placementController.visibleFootprintIsValid(settlementMap);

        const RenderColor outlineColor = settlementPlacementOutlineColor(
            valid ? SettlementPlacementVisualState::Valid
                  : SettlementPlacementVisualState::Invalid
        );

        const std::array<TileOverlayRenderItem, 1> overlays{
            {{static_cast<double>(preview->topLeft.x),
              static_cast<double>(preview->topLeft.y),
              static_cast<double>(preview->width),
              static_cast<double>(preview->height),
              settlementPlacementFillColor(
                  valid ? SettlementPlacementVisualState::Valid
                        : SettlementPlacementVisualState::Invalid
              )}}
        };

        const std::array<TileOutlineRenderItem, 1> outlines{
            {{static_cast<double>(preview->topLeft.x),
              static_cast<double>(preview->topLeft.y),
              static_cast<double>(preview->width),
              static_cast<double>(preview->height),
              2.0F,
              outlineColor}}
        };

        overlayRenderer_.render(renderer, overlays, camera, metrics);
        overlayRenderer_.renderOutlines(renderer, outlines, camera, metrics);
        drawDoor();
    }
} // namespace Paladin

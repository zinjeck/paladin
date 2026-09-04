#include "rendering/SettlementObjectRenderer.h"

#include "interaction/SettlementObjectPlacementController.h"
#include "rendering/Camera2D.h"
#include "rendering/Renderer.h"
#include "rendering/SettlementPlacementPalette.h"
#include "rendering/Texture.h"
#include "rendering/TileRenderMetrics.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include "world/settlements/objects/SettlementObjectState.h"

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
            for (
                std::int32_t y = footprint.topLeft.y;
                y < footprint.topLeft.y + footprint.height;
                ++y
            )
            {
                for (
                    std::int32_t x = footprint.topLeft.x;
                    x < footprint.topLeft.x + footprint.width;
                    ++x
                )
                {
                    pixels[
                        static_cast<std::size_t>(y)
                            * static_cast<std::size_t>(mapWidth)
                        + static_cast<std::size_t>(x)
                    ] = fillColor;
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
            const double tilePixels =
                metrics.scaledTilePixels(camera.zoom());
            const float lineWidth = static_cast<float>(
                std::min(1.0, std::max(0.5, tilePixels * 0.25))
            );
            const double left =
                static_cast<double>(renderer.outputWidth()) * 0.5 +
                (static_cast<double>(footprint.topLeft.x) - camera.tileX())
                    * tilePixels;
            const double top =
                static_cast<double>(renderer.outputHeight()) * 0.5 +
                (static_cast<double>(footprint.topLeft.y) - camera.tileY())
                    * tilePixels;
            const double width = footprint.width * tilePixels;
            const double height = footprint.height * tilePixels;

            if (
                left + width < 0.0 || top + height < 0.0 ||
                left > renderer.outputWidth() ||
                top > renderer.outputHeight()
            )
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
                static_cast<std::int32_t>(
                    std::floor(-left / tilePixels)
                ),
                0,
                footprint.width
            );
            const std::int32_t lastColumn = std::clamp(
                static_cast<std::int32_t>(
                    std::ceil(
                        (renderer.outputWidth() - left) / tilePixels
                    )
                ),
                0,
                footprint.width
            );
            const std::int32_t firstRow = std::clamp(
                static_cast<std::int32_t>(
                    std::floor(-top / tilePixels)
                ),
                0,
                footprint.height
            );
            const std::int32_t lastRow = std::clamp(
                static_cast<std::int32_t>(
                    std::ceil(
                        (renderer.outputHeight() - top) / tilePixels
                    )
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
                lines.push_back({
                    static_cast<float>(left + x * tilePixels) -
                        lineWidth * 0.5F,
                    static_cast<float>(top),
                    lineWidth,
                    static_cast<float>(height)
                });
            }

            for (std::int32_t y = firstRow; y <= lastRow; ++y)
            {
                lines.push_back({
                    static_cast<float>(left),
                    static_cast<float>(top + y * tilePixels) -
                        lineWidth * 0.5F,
                    static_cast<float>(width),
                    lineWidth
                });
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
            const float width = static_cast<float>(footprint.width * tilePixels);
            const float height = static_cast<float>(footprint.height * tilePixels);

            if (
                left + width < 0.0F || top + height < 0.0F ||
                left > renderer.outputWidth() || top > renderer.outputHeight()
            )
            {
                return;
            }

            lines.push_back({left, top, width, lineWidth});
            lines.push_back({left, top + height - lineWidth, width, lineWidth});
            lines.push_back({left, top, lineWidth, height});
            lines.push_back({left + width - lineWidth, top, lineWidth, height});
        }
    }


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

        if (
            cachedState_ != &state ||
            cachedVersion_ != state.presentationVersion()
        )
        {
            cachedState_ = &state;
            cachedVersion_ = state.presentationVersion();

            std::vector<RenderColor> pixels(
                settlementMap.grid().tileCount(),
                RenderColor{0, 0, 0, 0}
            );

            cachedInfrastructureOutlines_.clear();
            cachedInfrastructureOutlines_.reserve(
                state.completedObjects().size()
            );

            for (const SettlementConstructionSite& site : state.constructionSites())
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
                if (definition->id != SettlementObjectTypes::Road)
                    cachedInfrastructureOutlines_.push_back(footprintOutline(
                        site.footprint, settlementPlacementOutlineColor(
                            site.phase == ConstructionSitePhase::ReadyToBuild
                                ? SettlementPlacementVisualState::ReadyToBuild
                                : SettlementPlacementVisualState::AwaitingMaterials)));

            }

            for (const CompletedSettlementObject& object : state.completedObjects())
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

                if (definition->id != SettlementObjectTypes::Road)
                {
                    cachedInfrastructureOutlines_.push_back(
                        footprintOutline(
                            object.footprint,
                            renderColor(
                                definition->visual.frameColor,
                                255
                            )
                        )
                    );
                }
            }

            cachedInfrastructureTexture_ =
                renderer.createTextureFromPixels(
                    settlementMap.grid().width(),
                    settlementMap.grid().height(),
                    pixels
                );
        }

        if (cachedInfrastructureTexture_)
        {
            const double tilePixels =
                metrics.scaledTilePixels(camera.zoom());

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
                    static_cast<double>(settlementMap.grid().width())
                        * tilePixels
                ),
                static_cast<float>(
                    static_cast<double>(settlementMap.grid().height())
                        * tilePixels
                )
            );
        }

        overlayRenderer_.renderOutlines(
            renderer,
            cachedInfrastructureOutlines_,
            camera,
            metrics
        );

        std::vector<RenderRectangle> awaitingMaterialLines;
        std::vector<RenderRectangle> readyToBuildLines;
        constexpr std::size_t constructionGridLineBudget = 20'000;
        bool awaitingDetailedGrid = true;
        bool readyDetailedGrid = true;

        for (const SettlementConstructionSite& site : state.constructionSites())
        {
            std::vector<RenderRectangle>& lines =
                site.phase == ConstructionSitePhase::ReadyToBuild
                    ? readyToBuildLines
                    : awaitingMaterialLines;

            bool& detailedGrid =
                site.phase == ConstructionSitePhase::ReadyToBuild
                    ? readyDetailedGrid
                    : awaitingDetailedGrid;
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

            for (const SettlementConstructionSite& site : state.constructionSites())
            {
                const bool ready =
                    site.phase == ConstructionSitePhase::ReadyToBuild;
                if ((ready && readyDetailedGrid) || (!ready && awaitingDetailedGrid))
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

        const std::optional<SettlementObjectFootprint> preview =
            placementController.visibleFootprint();

        const SettlementObjectDefinition* definition =
            placementController.activeDefinition();

        if (!preview || !definition)
        {
            return;
        }

        if (definition->allowsPartialPlacement)
        {
            const double tilePixels =
                metrics.scaledTilePixels(camera.zoom());
            const std::int32_t visibleLeft = std::max(
                preview->topLeft.x,
                static_cast<std::int32_t>(std::floor(
                    camera.tileX() -
                    renderer.outputWidth() * 0.5 / tilePixels
                )) - 1
            );
            const std::int32_t visibleTop = std::max(
                preview->topLeft.y,
                static_cast<std::int32_t>(std::floor(
                    camera.tileY() -
                    renderer.outputHeight() * 0.5 / tilePixels
                )) - 1
            );
            const std::int32_t visibleRight = std::min(
                preview->topLeft.x + preview->width,
                static_cast<std::int32_t>(std::ceil(
                    camera.tileX() +
                    renderer.outputWidth() * 0.5 / tilePixels
                )) + 1
            );
            const std::int32_t visibleBottom = std::min(
                preview->topLeft.y + preview->height,
                static_cast<std::int32_t>(std::ceil(
                    camera.tileY() +
                    renderer.outputHeight() * 0.5 / tilePixels
                )) + 1
            );
            std::vector<TileOverlayRenderItem> tileOverlays;
            tileOverlays.reserve(
                static_cast<std::size_t>(
                    std::max(0, visibleBottom - visibleTop)
                ) + 1U
            );

            // Paint the selectable area once, then cover only contiguous
            // blocked runs. This keeps a large road drag independent of its
            // valid tile count instead of issuing one draw per tile.
            tileOverlays.push_back({
                static_cast<double>(preview->topLeft.x),
                static_cast<double>(preview->topLeft.y),
                static_cast<double>(preview->width),
                static_cast<double>(preview->height),
                settlementPlacementFillColor(
                    SettlementPlacementVisualState::Valid
                )
            });

            const std::size_t visibleTileCount =
                static_cast<std::size_t>(
                    std::max(0, visibleRight - visibleLeft)
                ) *
                static_cast<std::size_t>(
                    std::max(0, visibleBottom - visibleTop)
                );
            if (tilePixels >= 2.0 && visibleTileCount <= 50'000U)
            {
                for (
                    std::int32_t y = visibleTop;
                    y < visibleBottom;
                    ++y
                )
                {
                    std::optional<std::int32_t> blockedRunStart;

                    for (
                        std::int32_t x = visibleLeft;
                        x <= visibleRight;
                        ++x
                    )
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
                            tileOverlays.push_back({
                                static_cast<double>(*blockedRunStart),
                                static_cast<double>(y),
                                static_cast<double>(x - *blockedRunStart),
                                1.0,
                                settlementPlacementFillColor(
                                    SettlementPlacementVisualState::Invalid
                                )
                            });
                            blockedRunStart.reset();
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

            const std::array<TileOutlineRenderItem, 1> selectionOutline{{
                {
                    static_cast<double>(preview->topLeft.x),
                    static_cast<double>(preview->topLeft.y),
                    static_cast<double>(preview->width),
                    static_cast<double>(preview->height),
                    2.0F,
                    settlementPlacementOutlineColor(
                        placementController.hasLockedFootprint()
                            ? SettlementPlacementVisualState::Valid
                            : (
                                evaluation.footprintAllowed &&
                                !evaluation.hasObstructions()
                                    ? SettlementPlacementVisualState::Valid
                                    : SettlementPlacementVisualState::Invalid
                            )
                    )
                }
            }};

            overlayRenderer_.render(
                renderer,
                tileOverlays,
                camera,
                metrics
            );

            overlayRenderer_.renderOutlines(
                renderer,
                selectionOutline,
                camera,
                metrics
            );

            return;
        }

        const bool valid =
            placementController.visibleFootprintIsValid(settlementMap);

        const RenderColor outlineColor =
            settlementPlacementOutlineColor(
                valid
                    ? SettlementPlacementVisualState::Valid
                    : SettlementPlacementVisualState::Invalid
            );

        const std::array<TileOverlayRenderItem, 1> overlays{{
            {
                static_cast<double>(preview->topLeft.x),
                static_cast<double>(preview->topLeft.y),
                static_cast<double>(preview->width),
                static_cast<double>(preview->height),
                renderColor(definition->visual.fillColor, 100)
            }
        }};

        const std::array<TileOutlineRenderItem, 1> outlines{{
            {
                static_cast<double>(preview->topLeft.x),
                static_cast<double>(preview->topLeft.y),
                static_cast<double>(preview->width),
                static_cast<double>(preview->height),
                2.0F,
                outlineColor
            }
        }};

        overlayRenderer_.render(renderer, overlays, camera, metrics);
        overlayRenderer_.renderOutlines(renderer, outlines, camera, metrics);
    }
}

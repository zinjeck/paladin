#include "rendering/SettlementObjectRenderer.h"

#include "interaction/SettlementObjectPlacementController.h"
#include "rendering/Camera2D.h"
#include "rendering/Renderer.h"
#include "rendering/Texture.h"
#include "rendering/TileRenderMetrics.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include "world/settlements/objects/SettlementObjectState.h"

#include <algorithm>
#include <array>
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
                1.5F,
                color
            };
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
                state.constructionSites().size()
                    + state.completedObjects().size()
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

                // Roads are stored as independent one-tile sites. Keep them
                // as solid retained tiles rather than creating a draw call
                // for every road tile each frame.
                if (definition->id != SettlementObjectTypes::Road)
                {
                    cachedInfrastructureOutlines_.push_back(
                        footprintOutline(
                            site.footprint,
                            {205, 205, 210, 205}
                        )
                    );
                }
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

        const std::optional<SettlementObjectFootprint> preview =
            placementController.visibleFootprint();

        const SettlementObjectDefinition* definition =
            placementController.activeDefinition();

        if (!preview || !definition)
        {
            return;
        }

        const bool valid =
            placementController.visibleFootprintIsValid(settlementMap);

        const RenderColor outlineColor =
            placementController.hasLockedFootprint()
                ? RenderColor{242, 202, 78, 245}
                : (
                    valid
                        ? RenderColor{72, 220, 112, 240}
                        : RenderColor{232, 70, 70, 245}
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

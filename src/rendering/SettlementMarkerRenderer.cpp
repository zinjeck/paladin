#include "rendering/SettlementMarkerRenderer.h"

#include "rendering/Camera2D.h"
#include "rendering/GlobeView.h"
#include "rendering/Renderer.h"
#include "rendering/TileRenderMetrics.h"
#include "world/Settlement.h"
#include "world/World.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Paladin
{
    namespace
    {
        RenderColor visibleColor(RenderColor color, float visibility) noexcept
        {
            const float clamped = std::clamp(visibility, 0.0F, 1.0F);
            color.alpha = static_cast<std::uint8_t>(
                std::clamp(
                    std::lround(double(color.alpha) * clamped),
                    0L,
                    255L
                )
            );
            return color;
        }
    } // namespace

    void SettlementMarkerRenderer::drawMarker(
        Renderer& renderer,
        const World& world,
        const Settlement& settlement,
        float centerX,
        float centerY,
        float visibility
    ) const
    {
        if (visibility <= 0.0F)
        {
            return;
        }

        const SettlementWorldPresentation presentation =
            settlementWorldPresentation(settlement.population(), policy_);
        const float size = presentation.markerDiameterPixels;
        const float border = presentation.borderPixels;

        // The marker is intentionally procedural. It is a stable cartographic
        // symbol whose size comes from settlement data, not a sprite or a
        // village/town/city type. Base colors are from the approved palette;
        // realm color is an ownership annotation.
        const RenderColor shadow =
            visibleColor({8, 15, 27, 205}, visibility);       // #080F1B
        const RenderColor parchment =
            visibleColor({244, 243, 232, 255}, visibility);   // #F4F3E8
        const RenderColor ink =
            visibleColor({32, 44, 67, 255}, visibility);      // #202C43

        RenderColor ownership{255, 215, 131, 255}; // #FFD783
        if (const Realm* realm = world.realm(settlement.ownerRealmId()))
        {
            const MapColor mapColor = realm->mapColor();
            ownership =
                {mapColor.red, mapColor.green, mapColor.blue, 255};
        }
        ownership = visibleColor(ownership, visibility);

        const float top = centerY - size * 0.5F;
        const float bodyTop = top + size * 0.25F;
        const float bodyHeight = size * 0.67F;
        const float towerWidth = std::max(3.0F, size * 0.34F);
        const float towerHeight = size * 0.38F;
        const float bodyLeft = centerX - size * 0.5F;

        renderer.fillRectangle(
            bodyLeft - 1.0F,
            bodyTop + 1.0F,
            size + 2.0F,
            bodyHeight + 1.0F,
            shadow
        );
        renderer.fillRectangle(
            centerX - towerWidth * 0.5F - 1.0F,
            top + 1.0F,
            towerWidth + 2.0F,
            towerHeight + 1.0F,
            shadow
        );

        renderer.fillRectangle(bodyLeft, bodyTop, size, bodyHeight, parchment);
        renderer.fillRectangle(
            centerX - towerWidth * 0.5F,
            top,
            towerWidth,
            towerHeight,
            parchment
        );

        renderer.fillRectangle(
            bodyLeft + border,
            bodyTop + border,
            std::max(1.0F, size - border * 2.0F),
            std::max(1.0F, bodyHeight - border * 2.0F),
            ink
        );
        renderer.fillRectangle(
            centerX - std::max(1.0F, towerWidth - border * 2.0F) * 0.5F,
            top + border,
            std::max(1.0F, towerWidth - border * 2.0F),
            std::max(1.0F, towerHeight - border * 1.5F),
            ink
        );

        const float ownershipWidth = std::max(4.0F, size * 0.58F);
        const float ownershipHeight = std::max(2.0F, size * 0.12F);
        renderer.fillRectangle(
            centerX - ownershipWidth * 0.5F,
            bodyTop + bodyHeight - border - ownershipHeight,
            ownershipWidth,
            ownershipHeight,
            ownership
        );

        if (settlement.name().empty())
        {
            return;
        }

        constexpr float maximumLabelWidth = 160.0F;
        const float preferredPixelSize = presentation.labelPixelSize;
        const float preferredLabelWidth = fontRenderer_.measureWidth(
            settlement.name(),
            preferredPixelSize
        );
        const float pixelSize = preferredLabelWidth > maximumLabelWidth
                                    ? preferredPixelSize * maximumLabelWidth /
                                          preferredLabelWidth
                                    : preferredPixelSize;
        const float labelWidth =
            fontRenderer_.measureWidth(settlement.name(), pixelSize);
        const float labelX = centerX - labelWidth * 0.5F;
        const float labelY = top - 7.0F * pixelSize - 5.0F;

        fontRenderer_.drawText(
            renderer,
            settlement.name(),
            labelX + 1.0F,
            labelY + 1.0F,
            pixelSize,
            visibleColor({8, 15, 27, 220}, visibility)
        );
        fontRenderer_.drawText(
            renderer,
            settlement.name(),
            labelX,
            labelY,
            pixelSize,
            visibleColor({244, 243, 232, 255}, visibility)
        );
    }

    void SettlementMarkerRenderer::renderFlat(
        Renderer& renderer,
        const World& world,
        const Camera2D& camera,
        const TileRenderMetrics& metrics,
        float visibility
    ) const
    {
        if (visibility <= 0.0F)
        {
            return;
        }

        const double tilePixels = metrics.scaledTilePixels(camera.zoom());
        if (!std::isfinite(tilePixels) || tilePixels <= 0.0)
        {
            return;
        }

        const double viewportWidth =
            static_cast<double>(renderer.outputWidth());
        const double viewportHeight =
            static_cast<double>(renderer.outputHeight());
        const float cullMargin =
            std::max(32.0F, policy_.maximumMarkerDiameterPixels);

        for (const Settlement& settlement : world.settlements())
        {
            const WorldTilePosition position = settlement.position();
            const float centerX = static_cast<float>(
                viewportWidth * 0.5 +
                (static_cast<double>(position.x) + 0.5 - camera.tileX()) *
                    tilePixels
            );
            const float centerY = static_cast<float>(
                viewportHeight * 0.5 +
                (static_cast<double>(position.y) + 0.5 - camera.tileY()) *
                    tilePixels
            );

            if (centerX < -cullMargin || centerY < -cullMargin ||
                centerX > viewportWidth + cullMargin ||
                centerY > viewportHeight + cullMargin)
            {
                continue;
            }

            drawMarker(
                renderer,
                world,
                settlement,
                centerX,
                centerY,
                visibility
            );
        }
    }

    void SettlementMarkerRenderer::renderGlobe(
        Renderer& renderer,
        const World& world,
        const Camera2D& camera,
        float visibility
    ) const
    {
        if (visibility <= 0.0F || renderer.outputWidth() <= 0 ||
            renderer.outputHeight() <= 0)
        {
            return;
        }

        const auto view = GlobeView::from(
            camera,
            world.grid(),
            renderer.outputWidth(),
            renderer.outputHeight()
        );

        struct ProjectedSettlement
        {
            const Settlement* settlement = nullptr;
            float x = 0.0F;
            float y = 0.0F;
            float visibility = 0.0F;
            double depth = 0.0;
        };

        std::vector<ProjectedSettlement> visibleSettlements;
        visibleSettlements.reserve(world.settlements().size());

        for (const Settlement& settlement : world.settlements())
        {
            const auto position = settlement.position();
            const auto projected = view.project(
                (double(position.x) + 0.5) / world.grid().width(),
                (double(position.y) + 0.5) / world.grid().height()
            );
            if (!std::isfinite(projected.x) || !std::isfinite(projected.y) ||
                !std::isfinite(projected.z) || projected.z <= 0.0)
            {
                continue;
            }

            const float limbVisibility =
                std::clamp(static_cast<float>(projected.z * 4.0), 0.0F, 1.0F);
            visibleSettlements.push_back(
                {&settlement,
                 static_cast<float>(projected.x),
                 static_cast<float>(projected.y),
                 std::clamp(visibility * limbVisibility, 0.0F, 1.0F),
                 projected.z}
            );
        }

        // When markers overlap near the limb, the settlement closer to the
        // viewer should win. This remains independent of settlement identity.
        std::sort(
            visibleSettlements.begin(),
            visibleSettlements.end(),
            [](const ProjectedSettlement& a, const ProjectedSettlement& b)
            { return a.depth < b.depth; }
        );

        for (const ProjectedSettlement& projected : visibleSettlements)
        {
            drawMarker(
                renderer,
                world,
                *projected.settlement,
                projected.x,
                projected.y,
                projected.visibility
            );
        }
    }
} // namespace Paladin

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
        float visibility,
        float labelClearance,
        bool showSymbol
    ) const
    {
        if (visibility <= 0.0F)
        {
            return;
        }

        const SettlementWorldPresentation presentation =
            settlementWorldPresentation(settlement.population(), policy_);
        centerX = std::round(centerX);
        centerY = std::round(centerY);
        // A single immutable 18-pixel building for every city and a single
        // immutable fortress. Population, culture and zoom never change these.
        constexpr float size=SettlementMapIconDiameterPixels;
        const float top=std::round(centerY-size*.5F), left=std::round(centerX-size*.5F);
        const auto rect=[&](float x,float y,float w,float h,RenderColor color)
        { renderer.fillRectangle(left+x,top+y,w,h,visibleColor(color,visibility)); };
        constexpr RenderColor ink{8,15,27,255}, wall{239,226,207,255}, shade{169,148,120,255};
        if (showSymbol)
        {
            if (settlement.isFortress())
            {
                constexpr RenderColor stone{154,167,175,255}, light{215,224,227,255}, dark{89,102,121,255};
                rect(0,4,18,14,ink); rect(1,5,16,12,stone);
                rect(6,9,6,8,dark); rect(7,12,4,6,ink);
                for (float x:{0.F,12.F})
                {
                    rect(x,1,6,16,ink); rect(x+1,3,4,13,stone); rect(x+1,3,1,13,light);
                    rect(x,0,2,4,ink); rect(x+4,0,2,4,ink);
                    rect(x+1,1,1,2,light); rect(x+4,1,1,2,light);
                    rect(x+2,7,2,3,ink);
                }
                rect(6,7,6,2,light); rect(1,16,16,1,dark);
            }
            else
            {
                rect(2,7,14,11,ink); rect(3,8,12,9,wall); rect(12,8,3,9,shade);
                for (int row=0;row<7;++row)
                {
                    rect(8-row,float(row),2+2*row,2,ink);
                    if (row>0) rect(9-row,float(row)+1,2*row,1,{183,106,54,255});
                    if (row>1) rect(9-row,float(row)+1,float(row),1,{243,174,69,255});
                }
                rect(1,7,16,1,ink); rect(5,10,2,2,{32,44,67,255}); rect(12,10,2,2,{32,44,67,255});
                rect(8,12,3,5,{73,53,47,255}); rect(8,12,1,5,{183,131,80,255});
                rect(3,16,4,1,shade);
            }
        }

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
        const float labelX = std::round(centerX - labelWidth * 0.5F);
        const float labelY = std::round(std::min(top,centerY-labelClearance) - 7.0F * pixelSize - 5.0F);

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
            std::max(32.0F, SettlementMapIconDiameterPixels);

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

#include "ui/SettlementInspectionPanel.h"

#include "interaction/SettlementInspectionController.h"
#include "rendering/Camera2D.h"
#include "rendering/Renderer.h"
#include "rendering/TileRenderMetrics.h"
#include "ui/GrayUiRenderer.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include "world/settlements/objects/SettlementObjectState.h"

#include <algorithm>
#include <string>

namespace Paladin
{
    namespace
    {
        constexpr float panelWidth = 340.0F;
        constexpr float objectPanelHeight = 50.0F;
        constexpr float panelGap = 6.0F;
        constexpr float normalLineHeight = 25.0F;
        constexpr float constructionVerticalPadding = 12.0F;

        std::string progressLabel(
            const SettlementConstructionSite& site
        )
        {
            const std::uint16_t wholePercent =
                site.progressPermille / 10U;
            const std::uint16_t decimal =
                site.progressPermille % 10U;

            std::string label = "Construction Progress: ";
            label += std::to_string(wholePercent);

            if (decimal != 0)
            {
                label += '.';
                label += std::to_string(decimal);
            }

            label += '%';
            return label;
        }


        std::string deliveryLabel(
            const ConstructionResourceDelivery& delivery
        )
        {
            const SettlementResourceDefinition* definition =
                SettlementResourceCatalog::definition(
                    delivery.resourceId
                );

            std::string label = definition
                ? std::string(definition->displayName)
                : delivery.resourceId;

            label += ": ";
            label += std::to_string(delivery.deliveredAmount);
            label += '/';
            label += std::to_string(delivery.requiredAmount);
            return label;
        }
    }


    void SettlementInspectionPanel::render(
        Renderer& renderer,
        GrayUiRenderer& grayUiRenderer,
        const SettlementInspectionController& controller,
        const SettlementMap& settlementMap,
        const Camera2D& camera,
        const TileRenderMetrics& metrics
    )
    {
        const SettlementObjectState& objectState =
            settlementMap.objectState();

        const CompletedSettlementObject* object =
            controller.selectedObject(objectState);
        const SettlementConstructionSite* constructionSite =
            controller.selectedConstructionSite(objectState);

        if (!object && !constructionSite)
        {
            clearLayout();
            return;
        }

        const std::string_view objectTypeId = object
            ? std::string_view(object->objectTypeId)
            : std::string_view(constructionSite->objectTypeId);
        const SettlementObjectDefinition* definition =
            SettlementObjectCatalog::definition(objectTypeId);

        if (!definition)
        {
            clearLayout();
            return;
        }

        const SettlementObjectFootprint& footprint = object
            ? object->footprint
            : constructionSite->footprint;

        float constructionPanelHeight = 0.0F;

        if (constructionSite)
        {
            const bool showDeliveries =
                constructionSite->phase !=
                    ConstructionSitePhase::UnderConstruction;
            const std::size_t deliveryCount = showDeliveries
                ? constructionSite->resourceDeliveries.size()
                : 0U;

            constructionPanelHeight =
                constructionVerticalPadding * 2.0F
                + normalLineHeight
                    * static_cast<float>(1U + deliveryCount);
        }

        const float totalHeight = objectPanelHeight
            + (constructionSite
                ? panelGap + constructionPanelHeight
                : 0.0F);

        renderedBounds_ = anchoredBounds(
            footprint,
            controller.placePanelOnRight(),
            panelWidth,
            totalHeight,
            renderer,
            camera,
            metrics
        );
        hasRenderedBounds_ = true;

        const UiRectangle objectBounds{
            renderedBounds_.x,
            renderedBounds_.y,
            renderedBounds_.width,
            objectPanelHeight
        };
        grayUiRenderer.drawPanel(renderer, objectBounds);

        constexpr float objectNamePixelSize = 3.0F;
        const float objectNameWidth = retroFontRenderer_.measureWidth(
            definition->displayName,
            objectNamePixelSize
        );
        retroFontRenderer_.drawText(
            renderer,
            definition->displayName,
            objectBounds.x
                + (objectBounds.width - objectNameWidth) * 0.5F,
            objectBounds.y + 14.0F,
            objectNamePixelSize,
            {242, 242, 244, 255}
        );

        if (!constructionSite)
        {
            return;
        }

        const UiRectangle constructionBounds{
            renderedBounds_.x,
            renderedBounds_.y + objectPanelHeight + panelGap,
            renderedBounds_.width,
            constructionPanelHeight
        };
        grayUiRenderer.drawPanel(renderer, constructionBounds);

        float textY = constructionBounds.y
            + constructionVerticalPadding;
        normalFontRenderer_.drawText(
            renderer,
            progressLabel(*constructionSite),
            constructionBounds.x + 14.0F,
            textY
        );

        if (
            constructionSite->phase ==
                ConstructionSitePhase::UnderConstruction
        )
        {
            return;
        }

        for (const ConstructionResourceDelivery& delivery :
            constructionSite->resourceDeliveries)
        {
            textY += normalLineHeight;
            normalFontRenderer_.drawText(
                renderer,
                deliveryLabel(delivery),
                constructionBounds.x + 14.0F,
                textY
            );
        }
    }


    bool SettlementInspectionPanel::containsPoint(
        float x,
        float y
    ) const noexcept
    {
        return hasRenderedBounds_ && renderedBounds_.contains(x, y);
    }


    void SettlementInspectionPanel::clearLayout() noexcept
    {
        renderedBounds_ = {};
        hasRenderedBounds_ = false;
    }


    UiRectangle SettlementInspectionPanel::anchoredBounds(
        const SettlementObjectFootprint& footprint,
        bool placeOnRight,
        float requestedPanelWidth,
        float panelHeight,
        const Renderer& renderer,
        const Camera2D& camera,
        const TileRenderMetrics& metrics
    ) const noexcept
    {
        constexpr float viewportMargin = 10.0F;
        constexpr float objectGap = 12.0F;

        const float viewportWidth =
            static_cast<float>(renderer.outputWidth());
        const float viewportHeight =
            static_cast<float>(renderer.outputHeight());
        const float tilePixels = static_cast<float>(
            metrics.scaledTilePixels(camera.zoom())
        );
        const float objectLeft = viewportWidth * 0.5F
            + static_cast<float>(
                static_cast<double>(footprint.topLeft.x)
                - camera.tileX()
            ) * tilePixels;
        const float objectTop = viewportHeight * 0.5F
            + static_cast<float>(
                static_cast<double>(footprint.topLeft.y)
                - camera.tileY()
            ) * tilePixels;
        const float objectRight = objectLeft
            + static_cast<float>(footprint.width) * tilePixels;

        const float maximumPanelWidth = std::max(
            1.0F,
            viewportWidth - viewportMargin * 2.0F
        );
        const float actualPanelWidth = std::min(
            requestedPanelWidth,
            maximumPanelWidth
        );
        const float desiredX = placeOnRight
            ? objectRight + objectGap
            : objectLeft - actualPanelWidth - objectGap;

        return {
            std::clamp(
                desiredX,
                viewportMargin,
                std::max(
                    viewportMargin,
                    viewportWidth
                        - actualPanelWidth
                        - viewportMargin
                )
            ),
            std::clamp(
                objectTop,
                viewportMargin,
                std::max(
                    viewportMargin,
                    viewportHeight - panelHeight - viewportMargin
                )
            ),
            actualPanelWidth,
            panelHeight
        };
    }
}

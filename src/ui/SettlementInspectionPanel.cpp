#include "ui/SettlementInspectionPanel.h"

#include "interaction/SettlementInspectionController.h"
#include "rendering/Camera2D.h"
#include "rendering/Renderer.h"
#include "rendering/TileRenderMetrics.h"
#include "ui/GrayUiRenderer.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/SettlementResourceDefinition.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include "world/settlements/objects/SettlementObjectState.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace Paladin
{
    namespace
    {
        constexpr float panelWidth = 340.0F;
        constexpr float objectPanelHeight = 50.0F;
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
        const SettlementCitizenState& citizenState,
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
        const SettlementCitizen* citizen =
            controller.selectedCitizen(citizenState);

        const auto* pile =
            controller.selectedInventory(settlementMap.logistics);
        if (pile && pile->used() <= 0)
        {
            pile = nullptr;
        }
        if (!object && !constructionSite && !citizen && !pile)
        {
            clearLayout();
            return;
        }

        const SettlementObjectDefinition* definition = nullptr;
        if (!citizen && !pile)
        {
            const std::string_view objectTypeId = object
                ? std::string_view(object->objectTypeId)
                : std::string_view(constructionSite->objectTypeId);
            definition = SettlementObjectCatalog::definition(objectTypeId);

            if (!definition)
            {
                clearLayout();
                return;
            }
        }

        const SettlementObjectFootprint footprint =
            citizen  ? SettlementObjectFootprint{citizen->tilePosition, 1, 1}
            : pile   ? pile->footprint
            : object ? object->footprint
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

        const WorkplaceId selectedWorkplace = object
            ? settlementMap.employment().forObject(object->id)
            : WorkplaceId{};
        if (selectedWorkplace != workplaceId_)
        {
            nameField_.setFocused(false);
            workplaceId_ = selectedWorkplace;
        }
        const auto* workplace = settlementMap.employment().workplace(workplaceId_);
        const auto* inventory =
            pile     ? pile
            : object ? settlementMap.logistics.inventory(
                           settlementMap.logistics.forObject(object->id)
                       )
                     : nullptr;
        const bool house =
            object && object->objectTypeId == SettlementObjectTypes::House;
        const float storageHeight =
            inventory ? 34.0F + float(inventory->goods.size()) * 22
            : house   ? 36.0F
                      : 0;
        const float detailsHeight =
            citizen ? 300.0F : (workplace ? 78.0F : 0.0F) + storageHeight;
        const float totalHeight = objectPanelHeight
            + constructionPanelHeight + detailsHeight;

        Camera2D panelCamera = camera;
        if (citizen)
        {
            panelCamera.move(citizen->tilePosition.x - citizen->visualX(),
                citizen->tilePosition.y - citizen->visualY());
        }
        renderedBounds_ = anchoredBounds(
            footprint,
            controller.placePanelOnRight(),
            panelWidth,
            totalHeight,
            renderer,
            panelCamera,
            metrics
        );
        hasRenderedBounds_ = true;

        grayUiRenderer.drawPanel(renderer, renderedBounds_);

        constexpr float preferredNamePixelSize = 3.0F;
        const std::string_view title = citizen ? std::string_view(citizen->name)
                                       : pile  ? std::string_view("Groundpile")
                                       : workplace
                                           ? std::string_view(workplace->name)
                                           : definition->displayName;
        const float objectNamePixelSize = std::min(preferredNamePixelSize,
            (renderedBounds_.width - 24) / std::max(1.0F, float(title.size()) * 6 - 1));
        const float objectNameWidth = retroFontRenderer_.measureWidth(
            title,
            objectNamePixelSize
        );
        if (workplace)
        {
            const UiRectangle titleBounds{renderedBounds_.x + 5, renderedBounds_.y + 5,
                renderedBounds_.width - 10, 39};
            nameButton_.setBounds(titleBounds);
            nameButton_.setText(workplace->name);
            nameField_.setBounds(titleBounds);
            if (nameField_.focused()) nameField_.render(renderer, grayUiRenderer);
            else nameButton_.render(renderer, grayUiRenderer);
        }
        else retroFontRenderer_.drawText(
            renderer,
            title,
            renderedBounds_.x
                + (renderedBounds_.width - objectNameWidth) * 0.5F,
            renderedBounds_.y + 14.0F,
            objectNamePixelSize,
            {242, 242, 244, 255}
        );

        if (citizen)
        {
            const auto* job = settlementMap.employment().workplace(citizen->workplaceId);
            const auto label = [&](const std::string& text, float y)
            {
                const float scale = std::min(
                    1.7F,
                    (renderedBounds_.width - 26) /
                        std::max(1.0F, float(text.size()) * 6 - 1)
                );
                grayUiRenderer.drawLabel(
                    renderer,
                    text,
                    renderedBounds_.x + 13,
                    renderedBounds_.y + y,
                    scale
                );
            };
            label("Age: " + std::to_string(citizen->ageYears), 56);
            const auto meter =
                [&](const char* name, double value, float y, bool hunger)
            {
                const float fraction = float(std::clamp(value / 100, 0.0, 1.0));
                const float severity = hunger ? fraction : 1 - fraction;
                constexpr std::array<RenderColor, 4> colors{
                    {{75, 191, 84, 255},
                     {226, 200, 61, 255},
                     {235, 138, 44, 255},
                     {220, 53, 49, 255}}
                };
                const float progress = severity * 3;
                const int index = std::min(2, int(progress));
                const float t = progress - index;
                const auto blend = [&](std::uint8_t a, std::uint8_t b)
                { return std::uint8_t(float(a) + (float(b) - a) * t); };
                const RenderColor color{
                    blend(colors[index].red, colors[index + 1].red),
                    blend(colors[index].green, colors[index + 1].green),
                    blend(colors[index].blue, colors[index + 1].blue),
                    255
                };
                const float x = renderedBounds_.x + 13;
                const float top = renderedBounds_.y + y;
                const float width = renderedBounds_.width - 26;
                renderer.fillRectangle(
                    x - 1,
                    top - 1,
                    width + 2,
                    24,
                    {30, 30, 31, 255}
                );
                renderer.fillRectangle(
                    x,
                    top,
                    width,
                    22,
                    {std::uint8_t(color.red * .32F),
                     std::uint8_t(color.green * .32F),
                     std::uint8_t(color.blue * .32F),
                     255}
                );
                renderer.fillRectangle(
                    x + (hunger ? 0 : width * (1 - fraction)),
                    top,
                    width * fraction,
                    22,
                    color
                );
                const auto text = std::string(name) + ": " +
                                  std::to_string(int(std::round(value))) + "%";
                retroFontRenderer_.drawText(
                    renderer,
                    text,
                    x + 7,
                    top + 5,
                    1.5F,
                    {245, 245, 245, 255}
                );
            };
            meter("Health", citizen->health, 83, false);
            meter("Hunger", citizen->hunger, 113, true);
            meter("Happiness", citizen->happiness, 143, false);
            label(
                std::string("Sex: ") +
                    (citizen->sex == CitizenSex::Male ? "Male" : "Female"),
                180
            );
            label("Job: " + (job ? job->name : std::string("Unemployed")), 208);
            label(citizen->homeId ? "Home: House" : "Home: Homeless", 236);
            label(
                std::string("Activity: ") +
                    SettlementActivitySystem::activityLabel(*citizen),
                264
            );
            if (citizen->carriedAmount > 0)
            {
                const auto* definition = SettlementResourceCatalog::definition(
                    citizen->carriedResource
                );
                label(
                    "Carrying: " + std::to_string(citizen->carriedAmount) +
                        " " +
                        (definition ? std::string(definition->displayName)
                                    : citizen->carriedResource),
                    292
                );
            }
        }
        if (workplace)
        {
            const auto employed = settlementMap.employment().employed(workplace->id, citizenState);
            grayUiRenderer.drawLabel(renderer, "Employment: " + std::to_string(employed)
                + "/" + std::to_string(workplace->capacity),
                renderedBounds_.x + 13, renderedBounds_.y + 62, 1.5F);
            const auto y = renderedBounds_.y + 52;
            decreaseButton_.setBounds({renderedBounds_.x + renderedBounds_.width - 78, y, 28, 28});
            increaseButton_.setBounds({renderedBounds_.x + renderedBounds_.width - 43, y, 28, 28});
            decreaseButton_.setEnabled(employed > 0);
            increaseButton_.setEnabled(workplace->capacity < workplace->maximumCapacity
                && settlementMap.employment().unemployed(citizenState) > 0);
            decreaseButton_.render(renderer, grayUiRenderer);
            increaseButton_.render(renderer, grayUiRenderer);
            grayUiRenderer.drawLabel(renderer,
                nameField_.focused() ? "Enter saves / Escape cancels"
                    : workplace->operational ? "Workplace ready" : "Awaiting construction",
                renderedBounds_.x + 13, renderedBounds_.y + 98, 1.25F);
        }
        if (inventory)
        {
            float y = renderedBounds_.y + (workplace ? 138 : 60);
            grayUiRenderer.drawLabel(
                renderer,
                "Storage: " + std::to_string(inventory->used()) + "/" +
                    std::to_string(inventory->capacity),
                renderedBounds_.x + 13,
                y,
                1.5F
            );
            for (const auto& goods : inventory->goods)
            {
                y += 22;
                const auto* resource =
                    SettlementResourceCatalog::definition(goods.resource);
                grayUiRenderer.drawLabel(
                    renderer,
                    (resource ? std::string(resource->displayName)
                              : goods.resource) +
                        ": " + std::to_string(goods.amount),
                    renderedBounds_.x + 13,
                    y,
                    1.5F
                );
            }
        }
        else if (house)
        {
            const auto count = std::count_if(
                citizenState.citizens().begin(),
                citizenState.citizens().end(),
                [&](const auto& c) { return c.homeId == object->id; }
            );
            grayUiRenderer.drawLabel(
                renderer,
                "Residents: " + std::to_string(count) + "/4",
                renderedBounds_.x + 13,
                renderedBounds_.y + 60,
                1.5F
            );
        }
        if (!constructionSite) return;

        const UiRectangle constructionBounds{
            renderedBounds_.x,
            renderedBounds_.y + objectPanelHeight + detailsHeight,
            renderedBounds_.width,
            constructionPanelHeight
        };

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
        workplaceId_ = {};
        nameField_.setFocused(false);
    }


    void SettlementInspectionPanel::pointerMoved(float x, float y)
    {
        nameButton_.pointerMoved(x, y);
        decreaseButton_.pointerMoved(x, y);
        increaseButton_.pointerMoved(x, y);
    }
    bool SettlementInspectionPanel::pointerPressed(float x, float y)
    {
        if (!containsPoint(x, y)) return false;
        if (workplaceId_)
        {
            static_cast<void>(nameButton_.pointerPressed(x, y));
            static_cast<void>(decreaseButton_.pointerPressed(x, y));
            static_cast<void>(increaseButton_.pointerPressed(x, y));
        }
        return true;
    }
    void SettlementInspectionPanel::pointerReleased(float x, float y,
        SettlementMap& map, SettlementCitizenState& citizens, double minute)
    {
        const bool title = nameButton_.pointerReleased(x, y);
        const bool less = decreaseButton_.pointerReleased(x, y);
        const bool more = increaseButton_.pointerReleased(x, y);
        const auto* workplace = map.employment().workplace(workplaceId_);
        if (!workplace) return;
        if (title && !nameField_.focused())
        {
            nameField_.setText(workplace->name);
            nameField_.setFocused(true);
        }
        if (less || more)
        {
            map.employment().adjust(workplaceId_, more ? 1 : -1, citizens);
            map.employment().record(minute, citizens);
        }
    }
    void SettlementInspectionPanel::finishRename(SettlementMap& map, bool commit)
    {
        if (!commit || map.employment().rename(workplaceId_, nameField_.text()))
            nameField_.setFocused(false);
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

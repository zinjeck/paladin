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

        std::string progressLabel(const SettlementConstructionSite& site)
        {
            const std::uint16_t wholePercent = site.progressPermille / 10U;
            const std::uint16_t decimal = site.progressPermille % 10U;

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

        std::string deliveryLabel(const ConstructionResourceDelivery& delivery)
        {
            const SettlementResourceDefinition* definition =
                SettlementResourceCatalog::definition(delivery.resourceId);

            std::string label = definition
                                    ? std::string(definition->displayName)
                                    : delivery.resourceId;

            label += ": ";
            label += std::to_string(delivery.deliveredAmount);
            label += '/';
            label += std::to_string(delivery.requiredAmount);
            return label;
        }
    } // namespace

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
        const SettlementObjectState& objectState = settlementMap.objectState();
        spouseId_ = {};
        tradeContent_ = {};
        tradeOrders_ = {};

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
            const std::string_view objectTypeId =
                object ? std::string_view(object->objectTypeId)
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
            const std::size_t deliveryCount =
                showDeliveries ? constructionSite->resourceDeliveries.size()
                               : 0U;

            constructionPanelHeight =
                constructionVerticalPadding * 2.0F +
                normalLineHeight * static_cast<float>(1U + deliveryCount);
        }

        const WorkplaceId selectedWorkplace =
            object ? settlementMap.employment().forObject(object->id)
                   : WorkplaceId{};
        if (selectedWorkplace != workplaceId_)
        {
            nameField_.setFocused(false);
            workplaceId_ = selectedWorkplace;
        }
        const auto* workplace =
            settlementMap.employment().workplace(workplaceId_);
        const auto* inventory =
            pile     ? pile
            : object ? settlementMap.logistics.inventory(
                           settlementMap.logistics.forObject(object->id)
                       )
                     : nullptr;
        const bool house =
            object && object->objectTypeId == SettlementObjectTypes::House;
        const bool tradeDepot =
            object && object->objectTypeId == SettlementObjectTypes::TradeDepot;
        const float panelWidth =
            tradeDepot ? std::min(850.F, float(renderer.outputWidth()) - 20)
                       : 340.F;
        showingHouse_ = house;
        const bool pasture = object && object->objectTypeId ==
                                           SettlementObjectTypes::Pastureland;
        const float storageHeight =
            house       ? 195.0F
            : inventory ? 34.0F + float(inventory->goods.size()) * 22
                        : 0;
        const float detailsHeight =
            citizen ? 386.0F
                    : (workplace ? 78.0F : 0.0F) + storageHeight +
                          (tradeDepot ? 32 : 0) + (pasture ? 48 : 0) +
                          (object && (object->objectTypeId ==
                                          SettlementObjectTypes::CityKeep ||
                                      workplace)
                               ? 46
                               : 0);
        showingKeep_ =
            object && object->objectTypeId == SettlementObjectTypes::CityKeep;
        const float totalHeight =
            tradeDepot
                ? std::min(620.F, float(renderer.outputHeight()) - 40)
                : objectPanelHeight + constructionPanelHeight + detailsHeight;

        Camera2D panelCamera = camera;
        if (citizen)
        {
            panelCamera.move(
                citizen->tilePosition.x - citizen->visualX(),
                citizen->tilePosition.y - citizen->visualY()
            );
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
        viewportWidth_ = float(renderer.outputWidth());
        viewportHeight_ = float(renderer.outputHeight());
        if (detached_)
        {
            if (!dragging_)
            {
                const float x = std::clamp(
                    positionX_,
                    8.0F,
                    std::max(8.0F, viewportWidth_ - renderedBounds_.width - 8)
                );
                const float y = std::clamp(
                    positionY_,
                    8.0F,
                    std::max(8.0F, viewportHeight_ - renderedBounds_.height - 8)
                );
                positionX_ += (x - positionX_) * .35F;
                positionY_ += (y - positionY_) * .35F;
            }
            renderedBounds_.x = positionX_;
            renderedBounds_.y = positionY_;
        }
        hasRenderedBounds_ = true;

        grayUiRenderer.drawPanel(renderer, renderedBounds_);
        const float contentWidth =
            tradeDepot ? std::min(340.F, renderedBounds_.width * .42F)
                       : renderedBounds_.width;
        if (tradeDepot)
        {
            tradeContent_ = {
                renderedBounds_.x + contentWidth,
                renderedBounds_.y + 8,
                renderedBounds_.width - contentWidth - 8,
                renderedBounds_.height - 16
            };
            renderer.drawLine(
                tradeContent_.x - 5,
                tradeContent_.y,
                tradeContent_.x - 5,
                tradeContent_.y + tradeContent_.height,
                {57, 70, 88, 255}
            );
        }
        if (showingKeep_)
        {
            grayUiRenderer.drawLabel(
                renderer,
                "Founding supplies - withdrawals only",
                renderedBounds_.x + 12,
                renderedBounds_.y + renderedBounds_.height - 28,
                1
            );
        }
        if (object && workplace)
        {
            if (pasture)
            {
                grayUiRenderer.drawLabel(
                    renderer,
                    "Animals: " +
                        std::to_string(
                            settlementMap.animals.containedCount(object->id)
                        ),
                    renderedBounds_.x + 13,
                    renderedBounds_.y + renderedBounds_.height - 76,
                    1.5F
                );
                grayUiRenderer.drawLabel(
                    renderer,
                    "Space: " +
                        std::to_string(
                            settlementMap.animals.usedSpace(object->id)
                        ) +
                        "/" +
                        std::to_string(
                            settlementMap.animals.capacity(object->footprint)
                        ),
                    renderedBounds_.x + 13,
                    renderedBounds_.y + renderedBounds_.height - 54,
                    1.5F
                );
            }
            grayUiRenderer.drawLabel(
                renderer,
                (tradeDepot ? "Realm treasury: " : "Operating cash: ") +
                    goldText(
                        tradeDepot
                            ? settlementMap.commerce.treasury->balance
                            : settlementMap.commerce.businessCash(object->id)
                    ),
                renderedBounds_.x + 13,
                renderedBounds_.y + renderedBounds_.height - 30,
                1.5F
            );
        }

        constexpr float preferredNamePixelSize = 3.3F;
        const std::string_view title = citizen ? std::string_view(citizen->name)
                                       : pile  ? std::string_view("Groundpile")
                                       : workplace
                                           ? std::string_view(workplace->name)
                                           : definition->displayName;
        const float objectNamePixelSize = std::min(
            preferredNamePixelSize,
            (contentWidth - 24) / std::max(1.0F, float(title.size()) * 6 - 1)
        );
        const float objectNameWidth =
            retroFontRenderer_.measureWidth(title, objectNamePixelSize);
        if (workplace)
        {
            const UiRectangle titleBounds{
                renderedBounds_.x + 5,
                renderedBounds_.y + 5,
                contentWidth - 10,
                39
            };
            nameButton_.setBounds(titleBounds);
            nameButton_.setText(workplace->name);
            nameField_.setBounds(titleBounds);
            if (nameField_.focused())
            {
                nameField_.render(renderer, grayUiRenderer);
            }
            else
            {
                nameButton_.render(renderer, grayUiRenderer);
            }
        }
        else
        {
            retroFontRenderer_.drawText(
                renderer,
                title,
                renderedBounds_.x + (contentWidth - objectNameWidth) * 0.5F,
                renderedBounds_.y + 14.0F,
                objectNamePixelSize,
                {242, 242, 244, 255}
            );
        }

        if (house)
        {
            const float x = renderedBounds_.x + 10, y = renderedBounds_.y + 8;
            homeUpgradeButton_.setBounds({x, y, 30, 30});
            homeUpgradeButton_.render(renderer, grayUiRenderer);
            const RenderColor green{0xA6, 0xCD, 0x59, 255};
            renderer.fillRectangle(x + 13, y + 12, 5, 13, green);
            for (int row = 0; row < 8; ++row)
            {
                renderer.fillRectangle(
                    x + 15 - row,
                    y + 5 + row,
                    1 + row * 2,
                    1,
                    green
                );
            }
            grayUiRenderer.drawLabel(
                renderer,
                "Level " + std::to_string(object->homeLevel),
                x + 38,
                y + 8,
                1.3F
            );
        }
        if (citizen)
        {
            const auto* job =
                settlementMap.employment().workplace(citizen->workplaceId);
            const auto label = [&](const std::string& text, float y)
            {
                const float scale = std::min(
                    1.7F,
                    (contentWidth - 26) /
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
            label(
                "Age: " + std::to_string(citizen->ageYears) +
                    (citizen->child ? " (Child)" : " (Adult)"),
                56
            );
            const auto meter =
                [&](const char* name, double value, float y, bool hunger)
            {
                const float fraction = float(std::clamp(value / 100, 0.0, 1.0));
                const float severity =
                    hunger ? std::max(0.0F, (fraction - .5F) * 2)
                           : 1 - fraction;
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
                const float width = contentWidth - 26;
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
                renderer.fillRectangle(x, top, width * fraction, 22, color);
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
            meter("Happiness", citizen->happiness, 113, false);
            meter("Hunger", citizen->hunger, 143, true);
            meter("Energy", citizen->energy, 173, false);
            label(
                std::string("Sex: ") +
                    (citizen->sex == CitizenSex::Male ? "Male" : "Female"),
                210
            );
            const auto* spouse = citizenState.citizen(citizen->spouseId);
            spouseId_ = spouse ? spouse->id : CitizenId{};
            if (spouse)
            {
                label("Married:", 238);
                const float prefixWidth =
                    retroFontRenderer_.measureWidth("Married: ", 1.7F);
                const float left = renderedBounds_.x + 13 + prefixWidth + 8;
                spouseButton_.setBounds(
                    {left,
                     renderedBounds_.y + 234,
                     std::min(
                         renderedBounds_.x + contentWidth - 13 - left,
                         retroFontRenderer_.measureWidth(spouse->name, 1.5F) +
                             20
                     ),
                     26}
                );
                spouseButton_.setText(spouse->name);
                spouseButton_.render(renderer, grayUiRenderer);
            }
            else
            {
                label("Not married", 238);
            }
            label(
                "Job: " + (citizen->child ? std::string("Child")
                           : job          ? job->name
                                          : std::string("Unemployed")),
                266
            );
            label(citizen->homeId ? "Home: House" : "Home: Homeless", 294);
            label(
                citizen->child
                    ? "No personal money"
                    : std::string(
                          citizen->spouseId ? "Shared gold: " : "Gold: "
                      ) +
                          goldText(settlementMap.commerce.spendingBalance(
                              *citizen,
                              citizenState
                          )),
                378
            );
            label(
                std::string("Activity: ") +
                    SettlementActivitySystem::activityLabel(*citizen),
                322
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
                    350
                );
            }
        }
        if (workplace)
        {
            const auto employed = settlementMap.employment().employed(
                workplace->id,
                citizenState
            );
            grayUiRenderer.drawLabel(
                renderer,
                "Employment: " + std::to_string(employed) + "/" +
                    std::to_string(workplace->capacity),
                renderedBounds_.x + 13,
                renderedBounds_.y + 62,
                1.5F
            );
            const auto y = renderedBounds_.y + 52;
            decreaseButton_.setBounds(
                {renderedBounds_.x + contentWidth - 78, y, 28, 28}
            );
            increaseButton_.setBounds(
                {renderedBounds_.x + contentWidth - 43, y, 28, 28}
            );
            decreaseButton_.setEnabled(workplace->capacity > 0);
            increaseButton_.setEnabled(
                workplace->capacity < workplace->maximumCapacity &&
                (workplace->objectTypeId != SettlementObjectTypes::Barracks ||
                 settlementMap.employment().unemployed(citizenState) > 0)
            );
            decreaseButton_.render(renderer, grayUiRenderer);
            increaseButton_.render(renderer, grayUiRenderer);
            grayUiRenderer.drawLabel(
                renderer,
                nameField_.focused()     ? "Enter saves / Escape cancels"
                : workplace->operational ? "Workplace ready"
                                         : "Awaiting construction",
                renderedBounds_.x + 13,
                renderedBounds_.y + 98,
                1.25F
            );
        }
        if (object && miningJob(object->objectTypeId))
        {
            const auto* progress = settlementMap.mining.find(object->id);
            const auto depth =
                int(std::lround(settlementMap.mining.depth(*object) * 100));
            grayUiRenderer.drawLabel(
                renderer,
                progress && progress->exhausted
                    ? "Deposit exhausted"
                    : "Excavation: " + std::to_string(depth) + "% depth",
                renderedBounds_.x + 13,
                renderedBounds_.y + 119,
                1.25F
            );
        }
        if (inventory && !house)
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
            int shown = 0;
            for (const auto& goods : inventory->goods)
            {
                if (goods.amount <= 0) { continue; }
                if (tradeDepot && shown++ >= 3) { continue; }
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
            if (tradeDepot)
            {
                const auto* imports = settlementMap.logistics.inventory(
                    settlementMap.logistics.importsForObject(object->id));
                y += 24;
                grayUiRenderer.drawLabel(renderer,
                    "Imports for city use: " + std::to_string(imports ? imports->used() : 0),
                    renderedBounds_.x + 13, y, 1.25F);
                if (imports)
                {
                    int index=0;
                    for (const auto& goods : imports->goods)
                    {
                        if (goods.amount<=0) continue;
                        const auto* d = SettlementResourceCatalog::definition(goods.resource);
                        grayUiRenderer.drawLabel(renderer,
                            (d ? std::string(d->displayName) : goods.resource) + ": " + std::to_string(goods.amount),
                            renderedBounds_.x+13+(index%2)*(contentWidth-26)/2, y+22+(index/2)*19,1.1F);
                        ++index;
                    }
                    y += ((index+1)/2)*19;
                }
                tradeOrders_ = {renderedBounds_.x + 12, y + 27,
                    contentWidth - 24,
                    std::max(0.F, renderedBounds_.y + renderedBounds_.height - 43 - (y + 27))};
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
            float residentY = renderedBounds_.y + 86;
            for (const auto& resident : citizenState.citizens())
            {
                if (resident.homeId != object->id)
                {
                    continue;
                }
                const float size = std::min(
                    1.4F,
                    (contentWidth - 26) /
                        std::max(1.0F, float(resident.name.size()) * 6)
                );
                grayUiRenderer.drawLabel(
                    renderer,
                    resident.name,
                    renderedBounds_.x + 13,
                    residentY,
                    size
                );
                residentY += 22;
            }
            grayUiRenderer.drawLabel(
                renderer,
                "Vacant spaces: " + std::to_string(std::max(0, 4 - int(count))),
                renderedBounds_.x + 13,
                residentY,
                1.2F
            );
            grayUiRenderer.drawLabel(
                renderer,
                std::string(
                    settlementMap.heating.heated(object->id)
                        ? "Warm - fire burning"
                    : count ? "Cold - needs firewood"
                            : "Unoccupied - fire out"
                ),
                renderedBounds_.x + 13,
                residentY + 24,
                1.2F
            );
            grayUiRenderer.drawLabel(
                renderer,
                "Firewood: " +
                    std::to_string(
                        inventory
                            ? inventory->amount(SettlementResourceTypes::Lumber)
                            : 0
                    ) +
                    "/8 (2/day; winter 4)",
                renderedBounds_.x + 13,
                residentY + 46,
                1.1F
            );
        }
        if (!constructionSite)
        {
            return;
        }

        const UiRectangle constructionBounds{
            renderedBounds_.x,
            renderedBounds_.y + objectPanelHeight + detailsHeight,
            contentWidth,
            constructionPanelHeight
        };

        float textY = constructionBounds.y + constructionVerticalPadding;
        normalFontRenderer_.drawText(
            renderer,
            progressLabel(*constructionSite),
            constructionBounds.x + 14.0F,
            textY
        );

        if (constructionSite->phase == ConstructionSitePhase::UnderConstruction)
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
        tradeContent_ = {};
        tradeOrders_ = {};
        spouseId_ = {};
        navigateCitizen_ = {};
        spouseButton_.cancelPress();
        renderedBounds_ = {};
        hasRenderedBounds_ = false;
        dragCandidate_ = dragging_ = detached_ = false;
        workplaceId_ = {};
        showingKeep_ = false;
        showingHouse_ = false;
        homeUpgradeButton_.cancelPress();
        nameField_.setFocused(false);
    }

    void SettlementInspectionPanel::pointerMoved(float x, float y)
    {
        if (dragCandidate_ && std::hypot(x - pressX_, y - pressY_) >= 4)
        {
            dragging_ = detached_ = true;
            positionX_ = std::clamp(
                x - offsetX_,
                64 - renderedBounds_.width,
                std::max(64.0F, viewportWidth_ - 64)
            );
            positionY_ = std::clamp(
                y - offsetY_,
                0.0F,
                std::max(0.0F, viewportHeight_ - 40)
            );
            nameButton_.cancelPress();
            decreaseButton_.cancelPress();
            increaseButton_.cancelPress();
        }
        nameButton_.pointerMoved(x, y);
        homeUpgradeButton_.pointerMoved(x, y);
        spouseButton_.pointerMoved(x, y);
        decreaseButton_.pointerMoved(x, y);
        increaseButton_.pointerMoved(x, y);
    }
    bool SettlementInspectionPanel::pointerPressed(float x, float y)
    {
        if (!containsPoint(x, y))
        {
            return false;
        }
        dragCandidate_ = !nameField_.focused();
        if (showingHouse_ && homeUpgradeButton_.pointerPressed(x, y))
        {
            dragCandidate_ = false;
            return true;
        }
        if (spouseId_ && spouseButton_.pointerPressed(x, y))
        {
            dragCandidate_ = false;
            return true;
        }
        pressX_ = x;
        pressY_ = y;
        offsetX_ = x - renderedBounds_.x;
        offsetY_ = y - renderedBounds_.y;
        if (workplaceId_)
        {
            static_cast<void>(nameButton_.pointerPressed(x, y));
            static_cast<void>(decreaseButton_.pointerPressed(x, y));
            static_cast<void>(increaseButton_.pointerPressed(x, y));
        }
        return true;
    }
    void SettlementInspectionPanel::pointerReleased(
        float x,
        float y,
        SettlementMap& map,
        SettlementCitizenState& citizens,
        double minute
    )
    {
        dragCandidate_ = false;
        if (showingHouse_)
        {
            static_cast<void>(homeUpgradeButton_.pointerReleased(x, y));
        }
        if (dragging_)
        {
            dragging_ = false;
            return;
        }
        const bool title = nameButton_.pointerReleased(x, y);
        if (spouseId_ && spouseButton_.pointerReleased(x, y) &&
            citizens.citizen(spouseId_))
        {
            navigateCitizen_ = spouseId_;
        }
        const bool less = decreaseButton_.pointerReleased(x, y);
        const bool more = increaseButton_.pointerReleased(x, y);
        const auto* workplace = map.employment().workplace(workplaceId_);
        if (!workplace)
        {
            return;
        }
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
    void SettlementInspectionPanel::finishRename(
        SettlementMap& map,
        bool commit
    )
    {
        if (!commit || map.employment().rename(workplaceId_, nameField_.text()))
        {
            nameField_.setFocused(false);
        }
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

        const float viewportWidth = static_cast<float>(renderer.outputWidth());
        const float viewportHeight =
            static_cast<float>(renderer.outputHeight());
        const float tilePixels =
            static_cast<float>(metrics.scaledTilePixels(camera.zoom()));
        const float objectLeft =
            viewportWidth * 0.5F +
            static_cast<float>(
                static_cast<double>(footprint.topLeft.x) - camera.tileX()
            ) * tilePixels;
        const float objectTop =
            viewportHeight * 0.5F +
            static_cast<float>(
                static_cast<double>(footprint.topLeft.y) - camera.tileY()
            ) * tilePixels;
        const float objectRight =
            objectLeft + static_cast<float>(footprint.width) * tilePixels;

        const float maximumPanelWidth =
            std::max(1.0F, viewportWidth - viewportMargin * 2.0F);
        const float actualPanelWidth =
            std::min(requestedPanelWidth, maximumPanelWidth);
        const float desiredX = placeOnRight
                                   ? objectRight + objectGap
                                   : objectLeft - actualPanelWidth - objectGap;

        return {
            std::clamp(
                desiredX,
                viewportMargin,
                std::max(
                    viewportMargin,
                    viewportWidth - actualPanelWidth - viewportMargin
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
} // namespace Paladin

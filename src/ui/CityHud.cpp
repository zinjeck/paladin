#include "ui/CityHud.h"
#include "ui/SimulationSpeedControls.h"
#include <cmath>
#include <sstream>
#include <iomanip>

#include "rendering/Renderer.h"
#include "ui/GrayUiRenderer.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include "world/settlements/commands/SettlementCommandDefinition.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace Paladin
{
    namespace
    {
        constexpr float categoryButtonWidth = 76.0F;
        constexpr float categoryButtonHeight = 64.0F;
        constexpr float optionButtonGap = 6.0F;

        struct MenuOptionDefinition
        {
            std::size_t category = 0;
            std::string_view objectTypeId;
            std::string_view commandTypeId;
            std::string_view commandLabel;
            std::string_view firstLine;
            std::string_view secondLine;
        };

        constexpr std::array<MenuOptionDefinition, 14> menuOptions{{
            {
                0, SettlementObjectTypes::CityKeep, "", "", "City", "Keep"
            },
            {
                1, SettlementObjectTypes::Road, "", "", "Road", ""
            },
            {
                2, SettlementObjectTypes::House, "", "", "House", ""
            },
            {
                3, SettlementObjectTypes::Stockpile, "", "", "Stockpile", ""
            },
            {
                4, SettlementObjectTypes::FishingGrounds, "", "", "Fishing", "Grounds"
            },
            {
                4, SettlementObjectTypes::WheatFarm, "", "", "Wheat", "Farm"
            },
            {
                4, SettlementObjectTypes::Pastureland, "", "", "Pastureland", ""
            },
            {
                4, SettlementObjectTypes::Bakery, "", "", "Bakery", ""
            },
            {5, "", SettlementCommandTypes::Cancel, "Cancel Task", "", ""},
            {5, "", SettlementCommandTypes::Demolish, "Demolish", "", ""},
            {5, "", SettlementCommandTypes::Hunt, "Hunt", "", ""},
            {5, "", SettlementCommandTypes::Gather, "Gather", "", ""},
            {5, "", SettlementCommandTypes::ChopTree, "Chop Trees", "", ""},
            {5, "", SettlementCommandTypes::CollectRock, "Collect Rocks", "", ""}
        }};

        float centeredLabelX(
            const UiRectangle& bounds,
            std::string_view text,
            float pixelSize
        ) noexcept
        {
            const float width = text.empty()
                ? 0.0F
                : (
                    static_cast<float>(text.size()) * 6.0F - 1.0F
                ) * pixelSize;

            return bounds.x + (bounds.width - width) * 0.5F;
        }
    }


    CityHud::CityHud()
        : backButton_("Back"),
          topButtons_{
              UiButton("Employment"),
              UiButton("Technology"),
              UiButton("Military"),
              UiButton("Economy")
          },
          bottomButtons_{
              UiButton("Rule"),
              UiButton("Roads"),
              UiButton("Housing"),
              UiButton("Logistics"),
              UiButton("Food"),
              UiButton("Command")
          }
    {
        optionButtons_.reserve(menuOptions.size());
        optionBounds_.resize(menuOptions.size());

        for (const MenuOptionDefinition& option : menuOptions)
        {
            optionButtons_.emplace_back(
                !option.objectTypeId.empty()
                    ? std::string()
                    : std::string(option.commandLabel)
            );
        }
    }


    void CityHud::layout(
        int viewportWidth,
        int viewportHeight
    ) noexcept
    {
        constexpr float backWidth = 140.0F;
        constexpr float backHeight = 44.0F;
        constexpr float informationWidth = 300.0F;
        constexpr float informationTopHeight = 52.0F;
        constexpr float informationBottomHeight = 48.0F;

        const float rowWidth =
            categoryButtonWidth
            * static_cast<float>(bottomButtons_.size());

        const float rowX =
            (static_cast<float>(viewportWidth) - rowWidth) * 0.5F;

        const float rowY =
            static_cast<float>(viewportHeight) - categoryButtonHeight;

        for (std::size_t index = 0; index < bottomButtons_.size(); ++index)
        {
            bottomButtons_[index].setBounds({
                rowX + static_cast<float>(index) * categoryButtonWidth,
                rowY,
                categoryButtonWidth,
                categoryButtonHeight
            });
        }

        std::array<std::size_t, CategoryCount> stackOffsets{};

        for (std::size_t index = 0; index < menuOptions.size(); ++index)
        {
            const std::size_t category = menuOptions[index].category;
            const std::size_t stackOffset = ++stackOffsets[category];

            optionBounds_[index] = {
                rowX + static_cast<float>(category) * categoryButtonWidth,
                rowY - (
                    categoryButtonHeight + optionButtonGap
                ) * static_cast<float>(stackOffset),
                categoryButtonWidth,
                categoryButtonHeight
            };

            optionButtons_[index].setBounds(optionBounds_[index]);
        }

        backButton_.setBounds({
            0.0F,
            static_cast<float>(viewportHeight) - backHeight,
            backWidth,
            backHeight
        });

        cityNamePanel_ = {
            0.0F,
            0.0F,
            informationWidth,
            informationTopHeight
        };

        dayTimePanel_ = {
            0.0F,
            informationTopHeight,
            informationWidth * 0.5F,
            informationBottomHeight
        };

        const float topButtonWidth = std::clamp(
            (static_cast<float>(viewportWidth) - informationWidth - SimulationSpeedControls::RowWidth)
                / static_cast<float>(topButtons_.size()),
            0.0F, 140.0F
        );
        for (std::size_t index = 0; index < topButtons_.size(); ++index)
        {
            topButtons_[index].setBounds({
                informationWidth + static_cast<float>(index) * topButtonWidth,
                0.0F, topButtonWidth, 44.0F
            });
        }

        const float goodsWidth = 144.0F;
        const float goodsX = static_cast<float>(viewportWidth) - goodsWidth;
        goodsButton_.setBounds({goodsX, SimulationSpeedControls::ButtonSide, goodsWidth, 32.0F});
        for (std::size_t i = 0; i < goodsCells_.size(); ++i)
        {
            goodsCells_[i] = {
                goodsX + static_cast<float>(i % 2) * 72.0F,
                SimulationSpeedControls::ButtonSide + 32.0F
                    + static_cast<float>(i / 2) * 58.0F,
                72.0F, 58.0F
            };
        }

        // Reserve the corner without covering the construction toolbar.
        const float minimapSide = std::max(0.0F, std::min({
            220.0F, rowX - 8.0F,
            static_cast<float>(viewportHeight) - 100.0F
        }));
        minimapPanel_ = {
            static_cast<float>(viewportWidth) - minimapSide,
            static_cast<float>(viewportHeight) - minimapSide,
            minimapSide, minimapSide
        };

        reservedPanel_ = {
            informationWidth * 0.5F,
            informationTopHeight,
            informationWidth * 0.5F,
            informationBottomHeight
        };
    }


    void CityHud::setCityInformation(
        std::string cityName,
        std::uint64_t day,
        int hour,
        int minute
    )
    {
        cityName_ = std::move(cityName);
        day_ = day;
        hour_ = std::clamp(hour, 0, 23);
        minute_ = std::clamp(minute, 0, 59);
    }


    void CityHud::pointerMoved(float x, float y) noexcept
    {
        backButton_.pointerMoved(x, y);
        goodsButton_.pointerMoved(x, y);
        for (UiButton& button : topButtons_)
        {
            button.pointerMoved(x, y);
        }

        for (UiButton& button : bottomButtons_)
        {
            button.pointerMoved(x, y);
        }

        for (std::size_t index = 0; index < optionButtons_.size(); ++index)
        {
            if (optionIsVisible(index))
            {
                optionButtons_[index].pointerMoved(x, y);
            }
        }
    }


    bool CityHud::pointerPressed(float x, float y) noexcept
    {
        const bool goodsCaptured = goodsButton_.pointerPressed(x, y);
        bool captured = goodsCaptured || (goodsOpen_ && std::any_of(goodsCells_.begin(), goodsCells_.end(),
            [=](const auto& cell) { return cell.contains(x, y); })) || backButton_.pointerPressed(x, y)
            || cityNamePanel_.contains(x, y)
            || dayTimePanel_.contains(x, y)
            || reservedPanel_.contains(x, y)
            || minimapPanel_.contains(x, y);
        for (UiButton& button : topButtons_)
        {
            captured = button.pointerPressed(x, y) || captured;
        }

        for (UiButton& button : bottomButtons_)
        {
            captured = button.pointerPressed(x, y) || captured;
        }

        for (std::size_t index = 0; index < optionButtons_.size(); ++index)
        {
            if (optionIsVisible(index))
            {
                captured =
                    optionButtons_[index].pointerPressed(x, y) || captured;
            }
        }

        return captured;
    }


    bool CityHud::containsInteractivePoint(float x, float y) const noexcept
    {
        for (const UiButton& button : topButtons_)
        {
            if (button.containsPoint(x, y))
            {
                return true;
            }
        }

        if (goodsButton_.containsPoint(x, y)
            || (goodsOpen_ && std::any_of(goodsCells_.begin(), goodsCells_.end(),
                [=](const auto& cell) { return cell.contains(x, y); }))
            || backButton_.containsPoint(x, y)
            || cityNamePanel_.contains(x, y)
            || dayTimePanel_.contains(x, y)
            || reservedPanel_.contains(x, y)
            || minimapPanel_.contains(x, y))
        {
            return true;
        }

        for (const UiButton& button : bottomButtons_)
        {
            if (button.containsPoint(x, y))
            {
                return true;
            }
        }

        for (std::size_t index = 0; index < optionButtons_.size(); ++index)
        {
            if (
                optionIsVisible(index) &&
                optionButtons_[index].containsPoint(x, y)
            )
            {
                return true;
            }
        }

        return false;
    }


    CityHudAction CityHud::pointerReleased(float x, float y) noexcept
    {
        const bool backClicked = backButton_.pointerReleased(x, y);
        if (goodsButton_.pointerReleased(x, y))
        {
            goodsOpen_ = !goodsOpen_;
            goodsButton_.setSelected(goodsOpen_);
        }
        for (UiButton& button : topButtons_)
        {
            // Connect destinations when these gameplay screens exist.
            if (button.pointerReleased(x, y))
            {
                closeCategoryMenus();
            }
        }

        for (std::size_t index = 0; index < bottomButtons_.size(); ++index)
        {
            if (bottomButtons_[index].pointerReleased(x, y))
            {
                openCategory_ = openCategory_ == index
                    ? CategoryCount
                    : index;

                for (
                    std::size_t buttonIndex = 0;
                    buttonIndex < bottomButtons_.size();
                    ++buttonIndex
                )
                {
                    bottomButtons_[buttonIndex].setSelected(
                        buttonIndex == openCategory_
                    );
                }
            }
        }

        selectedObjectTypeId_.clear();
        selectedCommandTypeId_.clear();

        for (std::size_t index = 0; index < optionButtons_.size(); ++index)
        {
            if (optionIsVisible(index))
            {
                const bool clicked =
                    optionButtons_[index].pointerReleased(x, y);

                if (
                    clicked &&
                    !menuOptions[index].objectTypeId.empty()
                )
                {
                    selectedObjectTypeId_ =
                        menuOptions[index].objectTypeId;
                }
                else if (
                    clicked &&
                    !menuOptions[index].commandTypeId.empty()
                )
                {
                    selectedCommandTypeId_ =
                        menuOptions[index].commandTypeId;
                }
            }
            else
            {
                optionButtons_[index].cancelPress();
                optionButtons_[index].pointerMoved(-1.0F, -1.0F);
            }
        }

        if (backClicked)
        {
            closeCategoryMenus();
            return CityHudAction::Back;
        }

        if (!selectedObjectTypeId_.empty())
        {
            closeCategoryMenus();
            return CityHudAction::BeginObjectPlacement;
        }

        if (!selectedCommandTypeId_.empty())
        {
            closeCategoryMenus();
            return CityHudAction::BeginCommand;
        }

        return CityHudAction::None;
    }


    std::string_view CityHud::selectedObjectTypeId() const noexcept
    {
        return selectedObjectTypeId_;
    }


    std::string_view CityHud::selectedCommandTypeId() const noexcept
    {
        return selectedCommandTypeId_;
    }


    void CityHud::render(
        Renderer& renderer,
        const GrayUiRenderer& uiRenderer
    ) const
    {
        uiRenderer.drawPanel(renderer, cityNamePanel_);
        uiRenderer.drawPanel(renderer, dayTimePanel_);
        uiRenderer.drawPanel(renderer, reservedPanel_);

        const std::string visibleCityName = cityName_.empty()
            ? std::string("Unnamed City")
            : cityName_;

        const float cityNamePixelSize = std::min(
            4.0F,
            (cityNamePanel_.width - 20.0F)
                / std::max(
                    1.0F,
                    static_cast<float>(visibleCityName.size()) * 6.0F
                        - 1.0F
                )
        );

        uiRenderer.drawLabel(
            renderer,
            visibleCityName,
            centeredLabelX(
                cityNamePanel_,
                visibleCityName,
                cityNamePixelSize
            ),
            cityNamePanel_.y
                + (cityNamePanel_.height - 7.0F * cityNamePixelSize) * 0.5F,
            cityNamePixelSize
        );

        const std::string dayAndTime =
            "Day " + std::to_string(day_)
            + " "
            + (hour_ < 10 ? "0" : "")
            + std::to_string(hour_)
            + ":"
            + (minute_ < 10 ? "0" : "")
            + std::to_string(minute_);

        constexpr float preferredTimePixelSize = 2.0F;
        constexpr float timeHorizontalPadding = 16.0F;

        const float timeWidthAtPreferredSize =
            dayAndTime.empty()
                ? 0.0F
                : (
                    static_cast<float>(dayAndTime.size()) * 6.0F
                    - 1.0F
                ) * preferredTimePixelSize;

        const float availableTimeWidth = std::max(
            1.0F,
            dayTimePanel_.width - timeHorizontalPadding
        );

        const float timePixelSize =
            timeWidthAtPreferredSize > availableTimeWidth
                ? preferredTimePixelSize
                    * availableTimeWidth
                    / timeWidthAtPreferredSize
                : preferredTimePixelSize;

        uiRenderer.drawLabel(
            renderer,
            dayAndTime,
            centeredLabelX(
                dayTimePanel_,
                dayAndTime,
                timePixelSize
            ),
            dayTimePanel_.y
                + (dayTimePanel_.height - 7.0F * timePixelSize) * 0.5F,
            timePixelSize
        );

        backButton_.render(renderer, uiRenderer);
        for (const UiButton& button : topButtons_)
        {
            button.render(renderer, uiRenderer);
        }
        uiRenderer.drawPanel(renderer, minimapPanel_);
        goodsButton_.render(renderer, uiRenderer);
        if (goodsOpen_)
        {
            for (const auto& cell : goodsCells_) uiRenderer.drawPanel(renderer, cell);
            for (std::size_t i = 0; i < 2; ++i)
            {
                const auto& cell = goodsCells_[i];
                const float x = cell.x + cell.width * .5F;
                const float y = cell.y + 9.0F;
                const RenderColor edge = i == 0 ? RenderColor{67, 69, 73, 255}
                    : RenderColor{75, 43, 23, 255};
                const RenderColor fill = i == 0 ? RenderColor{164, 168, 174, 255}
                    : RenderColor{157, 105, 54, 255};
                renderer.fillRectangle(x - 10, y, 20, 20, edge);
                renderer.fillRectangle(x - 7, y + 3, 14, 14, fill);
                if (i == 1)
                {
                    renderer.fillRectangle(x - 4, y + 4, 2, 12, edge);
                    renderer.fillRectangle(x + 3, y + 4, 2, 12, edge);
                }
                std::ostringstream amount;
                amount << std::fixed << std::setprecision(0)
                    << std::floor(std::max(0.0, i == 0 ? stoneAmount_ : woodAmount_));
                const auto label = amount.str();
                const float size = std::min(2.0F, (cell.width - 8) /
                    std::max(1.0F, float(label.size()) * 6 - 1));
                uiRenderer.drawLabel(renderer, label,
                    centeredLabelX(cell, label, size), cell.y + 38, size);
            }
        }

        for (const UiButton& button : bottomButtons_)
        {
            button.render(renderer, uiRenderer);
        }

        for (std::size_t index = 0; index < optionButtons_.size(); ++index)
        {
            if (!optionIsVisible(index))
            {
                continue;
            }

            optionButtons_[index].render(renderer, uiRenderer);

            const MenuOptionDefinition& definition = menuOptions[index];

            const SettlementObjectDefinition* objectDefinition =
                SettlementObjectCatalog::definition(
                    definition.objectTypeId
                );

            if (!objectDefinition)
            {
                continue;
            }

            const UiRectangle& bounds = optionBounds_[index];
            const bool isFoodWorkplace = definition.category == 4;
            const bool isRoad = definition.category == 1;
            const float maximumIconWidth = isFoodWorkplace
                ? bounds.width - 14.0F
                : (isRoad ? 18.0F : 36.0F);
            const float maximumIconHeight = isFoodWorkplace
                ? bounds.height - 14.0F
                : (isRoad ? 18.0F : 36.0F);
            const float scale = std::min(
                maximumIconWidth / objectDefinition->visual.iconWidth,
                maximumIconHeight / objectDefinition->visual.iconHeight
            );

            const float iconWidth =
                objectDefinition->visual.iconWidth * scale;
            const float iconHeight =
                objectDefinition->visual.iconHeight * scale;
            const float iconX = bounds.x + (bounds.width - iconWidth) * 0.5F;
            const float iconY = bounds.y + (bounds.height - iconHeight) * 0.5F;

            renderer.fillRectangle(
                iconX,
                iconY,
                iconWidth,
                iconHeight,
                {
                    objectDefinition->visual.frameColor[0],
                    objectDefinition->visual.frameColor[1],
                    objectDefinition->visual.frameColor[2],
                    255
                }
            );

            constexpr float iconBorder = 2.0F;

            if (
                iconWidth > iconBorder * 2.0F &&
                iconHeight > iconBorder * 2.0F
            )
            {
                renderer.fillRectangle(
                    iconX + iconBorder,
                    iconY + iconBorder,
                    iconWidth - iconBorder * 2.0F,
                    iconHeight - iconBorder * 2.0F,
                    {
                        objectDefinition->visual.fillColor[0],
                        objectDefinition->visual.fillColor[1],
                        objectDefinition->visual.fillColor[2],
                        255
                    }
                );
            }

            const bool hasSecondLine = !definition.secondLine.empty();
            const float firstLineY = bounds.y + bounds.height * 0.5F
                - (hasSecondLine ? 8.5F : 3.5F);

            const auto labelPixelSize = [&bounds](
                std::string_view text
            ) noexcept
            {
                constexpr float preferredSize = 1.45F;
                const float preferredWidth = text.empty()
                    ? 0.0F
                    : (
                        static_cast<float>(text.size()) * 6.0F - 1.0F
                    ) * preferredSize;

                return preferredWidth > bounds.width - 4.0F
                    ? preferredSize
                        * (bounds.width - 4.0F)
                        / preferredWidth
                    : preferredSize;
            };

            const float firstLinePixelSize =
                labelPixelSize(definition.firstLine);

            uiRenderer.drawLabel(
                renderer,
                definition.firstLine,
                centeredLabelX(
                    bounds,
                    definition.firstLine,
                    firstLinePixelSize
                ),
                firstLineY,
                firstLinePixelSize,
                {250, 250, 250, 255}
            );

            if (hasSecondLine)
            {
                const float secondLinePixelSize =
                    labelPixelSize(definition.secondLine);

                uiRenderer.drawLabel(
                    renderer,
                    definition.secondLine,
                    centeredLabelX(
                        bounds,
                        definition.secondLine,
                        secondLinePixelSize
                    ),
                    firstLineY + 11.0F,
                    secondLinePixelSize,
                    {250, 250, 250, 255}
                );
            }
        }
    }


    bool CityHud::optionIsVisible(
        std::size_t optionIndex
    ) const noexcept
    {
        return
            optionIndex < menuOptions.size() &&
            menuOptions[optionIndex].category == openCategory_;
    }


    void CityHud::closeCategoryMenus() noexcept
    {
        openCategory_ = CategoryCount;

        for (UiButton& button : bottomButtons_)
        {
            button.setSelected(false);
        }

        for (UiButton& button : optionButtons_)
        {
            button.cancelPress();
            button.pointerMoved(-1.0F, -1.0F);
        }
    }
}

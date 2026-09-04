#include "ui/CityHud.h"

#include "rendering/Renderer.h"
#include "ui/GrayUiRenderer.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"

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
            std::string_view commandLabel;
            std::string_view firstLine;
            std::string_view secondLine;
        };

        constexpr std::array<MenuOptionDefinition, 14> menuOptions{{
            {
                0, SettlementObjectTypes::CityKeep, "", "City", "Keep"
            },
            {
                1, SettlementObjectTypes::Road, "", "Road", ""
            },
            {
                2, SettlementObjectTypes::House, "", "House", ""
            },
            {
                3, SettlementObjectTypes::Stockpile, "", "Stockpile", ""
            },
            {
                4, SettlementObjectTypes::FishingGrounds, "", "Fishing", "Grounds"
            },
            {
                4, SettlementObjectTypes::WheatFarm, "", "Wheat", "Farm"
            },
            {
                4, SettlementObjectTypes::Pastureland, "", "Pastureland", ""
            },
            {
                4, SettlementObjectTypes::Bakery, "", "Bakery", ""
            },
            {5, "", "Cancel Task", "", ""},
            {5, "", "Demolish", "", ""},
            {5, "", "Hunt", "", ""},
            {5, "", "Gather", "", ""},
            {5, "", "Chop Trees", "", ""},
            {5, "", "Collect Rocks", "", ""}
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
            16.0F,
            static_cast<float>(viewportHeight) - backHeight - 16.0F,
            backWidth,
            backHeight
        });

        cityNamePanel_ = {
            16.0F,
            16.0F,
            informationWidth,
            informationTopHeight
        };

        dayTimePanel_ = {
            16.0F,
            16.0F + informationTopHeight,
            informationWidth * 0.5F,
            informationBottomHeight
        };

        reservedPanel_ = {
            16.0F + informationWidth * 0.5F,
            16.0F + informationTopHeight,
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
        bool captured = backButton_.pointerPressed(x, y);

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
        if (backButton_.containsPoint(x, y))
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

        return CityHudAction::None;
    }


    std::string_view CityHud::selectedObjectTypeId() const noexcept
    {
        return selectedObjectTypeId_;
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

        constexpr float timePixelSize = 2.25F;

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

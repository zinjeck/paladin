#include "ui/CityHud.h"
#include "rendering/SceneSpriteLibrary.h"
#include "ui/SimulationSpeedControls.h"
#include "world/Season.h"
#include "world/settlements/SettlementCommerce.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <iomanip>
#include <sstream>

#include "rendering/Renderer.h"
#include "ui/GrayUiRenderer.h"
#include "world/settlements/commands/SettlementCommandDefinition.h"
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
            std::string_view commandTypeId;
            std::string_view commandLabel;
            std::string_view firstLine;
            std::string_view secondLine;
        };

        constexpr std::array<MenuOptionDefinition, 16> menuOptions{
            {{0, SettlementObjectTypes::CityKeep, "", "", "City", "Keep"},
             {1, SettlementObjectTypes::Road, "", "", "Road", ""},
             {2, SettlementObjectTypes::House, "", "", "House", ""},
             {3,
              SettlementObjectTypes::FishingGrounds,
              "",
              "",
              "Fishing",
              "Grounds"},
             {3, SettlementObjectTypes::WheatFarm, "", "", "Wheat", "Farm"},
             {3, SettlementObjectTypes::Pastureland, "", "", "Pastureland", ""},
             {4,
              SettlementObjectTypes::LoggingGrounds,
              "",
              "",
              "Logging",
              "Grounds"},
             {4, SettlementObjectTypes::Bakery, "", "", "Bakery", ""},
             {5, SettlementObjectTypes::Stockpile, "", "", "Stockpile", ""},
             {5, SettlementObjectTypes::Market, "", "", "Market", ""},
             {6, "", SettlementCommandTypes::Cancel, "Cancel Task", "", ""},
             {6, "", SettlementCommandTypes::Demolish, "Demolish", "", ""},
             {6, "", SettlementCommandTypes::Hunt, "Hunt", "", ""},
             {6, "", SettlementCommandTypes::Gather, "Gather", "", ""},
             {6, "", SettlementCommandTypes::ChopTree, "Chop Trees", "", ""},
             {6,
              "",
              SettlementCommandTypes::CollectRock,
              "Collect Rocks",
              "",
              ""}}
        };

        float centeredLabelX(
            const UiRectangle& bounds,
            std::string_view text,
            float pixelSize
        ) noexcept
        {
            const float width =
                text.empty() ? 0.0F
                             : (static_cast<float>(text.size()) * 6.0F - 1.0F) *
                                   pixelSize;

            return bounds.x + (bounds.width - width) * 0.5F;
        }
    } // namespace

    CityHud::CityHud()
        : backButton_("Back"),
          topButtons_{
              UiButton("Laws"),
              UiButton("Employment"),
              UiButton("Technology"),
              UiButton("Military"),
              UiButton("Economy")
          },
          bottomButtons_{
              UiButton("Rule"),
              UiButton("Roads"),
              UiButton("Housing"),
              UiButton("Agriculture"),
              UiButton("Production"),
              UiButton("Logistics"),
              UiButton("Command")
          }
    {
        setSettlementStatus(false, 8);
        goodsButton_.setSelected(goodsOpen_);
        optionButtons_.reserve(menuOptions.size());
        optionBounds_.resize(menuOptions.size());

        for (const MenuOptionDefinition& option : menuOptions)
        {
            optionButtons_.emplace_back(
                !option.objectTypeId.empty() ? std::string()
                                             : std::string(option.commandLabel)
            );
        }
    }

    void CityHud::layout(int viewportWidth, int viewportHeight) noexcept
    {
        constexpr float backWidth = 140.0F;
        constexpr float backHeight = 44.0F;
        constexpr float informationWidth = 300.0F;
        constexpr float informationTopHeight = 52.0F;
        constexpr float informationBottomHeight = 48.0F;

        const float rowWidth =
            categoryButtonWidth * static_cast<float>(bottomButtons_.size());

        const float rowX =
            (static_cast<float>(viewportWidth) - rowWidth) * 0.5F;

        const float rowY =
            static_cast<float>(viewportHeight) - categoryButtonHeight;

        for (std::size_t index = 0; index < bottomButtons_.size(); ++index)
        {
            bottomButtons_[index].setBounds(
                {rowX + static_cast<float>(index) * categoryButtonWidth,
                 rowY,
                 categoryButtonWidth,
                 categoryButtonHeight}
            );
        }

        toolbarBounds_ = {rowX, rowY, rowWidth, categoryButtonHeight};
        std::array<std::size_t, CategoryCount> stackOffsets{};

        for (std::size_t index = 0; index < menuOptions.size(); ++index)
        {
            const std::size_t category = menuOptions[index].category;
            const std::size_t stackOffset = ++stackOffsets[category];

            optionBounds_[index] = {
                rowX + static_cast<float>(category) * categoryButtonWidth,
                rowY - (categoryButtonHeight + optionButtonGap) *
                           static_cast<float>(stackOffset),
                categoryButtonWidth,
                categoryButtonHeight
            };

            optionButtons_[index].setBounds(optionBounds_[index]);
        }

        backButton_.setBounds(
            {0.0F,
             static_cast<float>(viewportHeight) - backHeight,
             backWidth,
             backHeight}
        );

        cityNamePanel_ = {0.0F, 0.0F, informationWidth, informationTopHeight};

        seasonBounds_ = {
            cityNamePanel_.x + cityNamePanel_.width - 44,
            cityNamePanel_.y + (cityNamePanel_.height - 36) * .5F,
            36,
            36
        };
        dayTimePanel_ = {
            0.0F,
            informationTopHeight,
            informationWidth * 0.5F,
            informationBottomHeight
        };

        const float topButtonWidth = std::clamp(
            (static_cast<float>(viewportWidth) - informationWidth -
             SimulationSpeedControls::RowWidth - 36.0F) /
                static_cast<float>(topButtons_.size()),
            0.0F,
            140.0F
        );
        for (std::size_t index = 0; index < topButtons_.size(); ++index)
        {
            topButtons_[index].setBounds(
                {informationWidth + static_cast<float>(index) * topButtonWidth,
                 0.0F,
                 topButtonWidth,
                 44.0F}
            );
        }

        const float goodsWidth = 144.0F;
        eventsButton_.setBounds(
            {informationWidth + topButtonWidth * topButtons_.size(), 0, 36, 44}
        );
        eventsButton_.setSkinId("events");
        ledgerButton_.setSkinId("ledger");
        const float goodsX = static_cast<float>(viewportWidth) - goodsWidth;
        goodsButton_.setBounds(
            {goodsX, SimulationSpeedControls::ButtonSide, goodsWidth, 32.0F}
        );
        for (std::size_t i = 0; i < goodsCells_.size(); ++i)
        {
            goodsCells_[i] = {
                goodsX + static_cast<float>(i % 2) * 72.0F,
                SimulationSpeedControls::ButtonSide + 32.0F +
                    static_cast<float>(i / 2) * 58.0F,
                72.0F,
                58.0F
            };
        }

        // Reserve the corner without covering the construction toolbar.
        const float minimapSide = std::max(
            0.0F,
            std::min(
                {220.0F,
                 rowX - 8.0F,
                 static_cast<float>(viewportHeight) - 100.0F}
            )
        );
        minimapPanel_ = {
            static_cast<float>(viewportWidth) - minimapSide,
            static_cast<float>(viewportHeight) - minimapSide,
            minimapSide,
            minimapSide
        };

        reservedPanel_ = {
            informationWidth * 0.5F,
            informationTopHeight,
            informationWidth * 0.5F,
            informationBottomHeight
        };
        activeSettlementPanel_ = {
            0,
            informationTopHeight + informationBottomHeight + 36,
            informationWidth,
            36
        };
        // Ledger fits beside the minimap, or above it on narrow windows.
        const float ledgerX = minimapPanel_.x - 44;
        ledgerButton_.setBounds(
            ledgerX >= rowX + rowWidth + 4 || worldMode_
                ? UiRectangle{ledgerX, float(viewportHeight) - 40, 36, 36}
                : UiRectangle{
                      float(viewportWidth) - 40,
                      std::max(104.F, minimapPanel_.y - 40),
                      36,
                      36
                  }
        );
        treasuryPanel_ = {
            0,
            informationTopHeight + informationBottomHeight,
            informationWidth * .5F,
            36
        };
        extensionPanel_ = {
            informationWidth * .5F,
            treasuryPanel_.y,
            informationWidth * .5F,
            36
        };
        populationButton_.setBounds(reservedPanel_);
        populationButton_.setSkinId("population");
        roofsButton_.setBounds(
            {extensionPanel_.x,
             extensionPanel_.y,
             extensionPanel_.width * .5F,
             extensionPanel_.height}
        );
        artButton_.setBounds(
            {extensionPanel_.x + extensionPanel_.width * .5F,
             extensionPanel_.y,
             extensionPanel_.width * .5F,
             extensionPanel_.height}
        );
        artButton_.setSelected(SceneSpriteLibrary::environmentArtEnabled());
    }

    void CityHud::setSettlementStatus(
        bool hasKeep,
        std::size_t population
    ) noexcept
    {
        population_ = population;
        if (hasKeep_ && !hasKeep)
        {
            closeCategoryMenus();
        }
        hasKeep_ = hasKeep;
        for (std::size_t i = 0; i < bottomButtons_.size(); ++i)
        {
            bottomButtons_[i].setEnabled(i == 0 || hasKeep_);
        }
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
        artButton_.pointerMoved(x, y);
        if (!worldMode_)
        {
            roofsButton_.pointerMoved(x, y);
        }
        eventsButton_.pointerMoved(x, y);
        ledgerButton_.pointerMoved(x, y);
        backButton_.pointerMoved(x, y);
        populationButton_.pointerMoved(x, y);
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
        const bool art = artButton_.pointerPressed(x, y);
        const bool roofs = !worldMode_ && roofsButton_.pointerPressed(x, y);
        const bool events = eventsButton_.pointerPressed(x, y);
        const bool ledger = ledgerButton_.pointerPressed(x, y);
        if (events || ledger || roofs || art)
        {
            return true;
        }
        if (treasuryPanel_.contains(x, y) || extensionPanel_.contains(x, y))
        {
            return true;
        }
        if (worldMode_)
        {
            bool captured = populationButton_.pointerPressed(x, y) ||
                            cityNamePanel_.contains(x, y) ||
                            dayTimePanel_.contains(x, y) ||
                            activeSettlementPanel_.contains(x, y);
            for (auto& button : topButtons_)
            {
                captured = button.pointerPressed(x, y) || captured;
            }
            return captured;
        }
        const bool populationCaptured = populationButton_.pointerPressed(x, y);
        const bool goodsCaptured =
            goodsButton_.pointerPressed(x, y) || populationCaptured;
        bool captured =
            toolbarBounds_.contains(x, y) || goodsCaptured ||
            (goodsOpen_ &&
             std::any_of(
                 goodsCells_.begin(),
                 goodsCells_.end(),
                 [=](const auto& cell) { return cell.contains(x, y); }
             )) ||
            backButton_.pointerPressed(x, y) || cityNamePanel_.contains(x, y) ||
            dayTimePanel_.contains(x, y) || reservedPanel_.contains(x, y) ||
            minimapPanel_.contains(x, y);
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
        if (eventsButton_.containsPoint(x, y) ||
            ledgerButton_.containsPoint(x, y))
        {
            return true;
        }
        if (treasuryPanel_.contains(x, y) || extensionPanel_.contains(x, y))
        {
            return true;
        }
        for (const UiButton& button : topButtons_)
        {
            if (button.containsPoint(x, y))
            {
                return true;
            }
        }

        if (worldMode_)
        {
            return cityNamePanel_.contains(x, y) ||
                   dayTimePanel_.contains(x, y) ||
                   reservedPanel_.contains(x, y) ||
                   activeSettlementPanel_.contains(x, y);
        }
        if (toolbarBounds_.contains(x, y) || goodsButton_.containsPoint(x, y) ||
            (goodsOpen_ &&
             std::any_of(
                 goodsCells_.begin(),
                 goodsCells_.end(),
                 [=](const auto& cell) { return cell.contains(x, y); }
             )) ||
            backButton_.containsPoint(x, y) || cityNamePanel_.contains(x, y) ||
            dayTimePanel_.contains(x, y) || reservedPanel_.contains(x, y) ||
            minimapPanel_.contains(x, y))
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
            if (optionIsVisible(index) &&
                optionButtons_[index].containsPoint(x, y))
            {
                return true;
            }
        }

        return false;
    }

    CityHudAction CityHud::pointerReleased(float x, float y) noexcept
    {
        if (artButton_.pointerReleased(x, y))
        {
            return CityHudAction::ToggleEnvironmentArt;
        }
        if (!worldMode_ && roofsButton_.pointerReleased(x, y))
        {
            return CityHudAction::ToggleRoofs;
        }
        const bool events = eventsButton_.pointerReleased(x, y);
        const bool ledger = ledgerButton_.pointerReleased(x, y);
        if (events || ledger)
        {
            closeCategoryMenus();
            return events ? CityHudAction::Events : CityHudAction::Ledger;
        }
        const bool backClicked = backButton_.pointerReleased(x, y);
        if (goodsButton_.pointerReleased(x, y))
        {
            goodsOpen_ = !goodsOpen_;
            goodsButton_.setSelected(goodsOpen_);
        }
        CityHudAction topAction = populationButton_.pointerReleased(x, y)
                                      ? CityHudAction::Population
                                      : CityHudAction::None;
        if (topAction != CityHudAction::None)
        {
            closeCategoryMenus();
        }
        constexpr std::array topActions{
            CityHudAction::Laws,
            CityHudAction::Employment,
            CityHudAction::Technology,
            CityHudAction::Military,
            CityHudAction::Economy
        };
        for (std::size_t i = 0; i < topButtons_.size(); ++i)
        {
            if (topButtons_[i].pointerReleased(x, y))
            {
                closeCategoryMenus();
                topAction = topActions[i];
            }
        }

        if (worldMode_)
        {
            return topAction;
        }
        for (std::size_t index = 0; index < bottomButtons_.size(); ++index)
        {
            if (bottomButtons_[index].pointerReleased(x, y))
            {
                openCategory_ = openCategory_ == index ? CategoryCount : index;

                for (std::size_t buttonIndex = 0;
                     buttonIndex < bottomButtons_.size();
                     ++buttonIndex)
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

                if (clicked && !menuOptions[index].objectTypeId.empty())
                {
                    selectedObjectTypeId_ = menuOptions[index].objectTypeId;
                }
                else if (clicked && !menuOptions[index].commandTypeId.empty())
                {
                    selectedCommandTypeId_ = menuOptions[index].commandTypeId;
                }
            }
            else
            {
                optionButtons_[index].cancelPress();
                optionButtons_[index].pointerMoved(-1.0F, -1.0F);
            }
        }

        if (topAction != CityHudAction::None)
        {
            return topAction;
        }

        if (backClicked)
        {
            closeCategoryMenus();
            return CityHudAction::Back;
        }

        if (!selectedObjectTypeId_.empty())
        {
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
        uiRenderer.drawPanel(renderer, treasuryPanel_);
        uiRenderer.drawPanel(renderer, extensionPanel_);
        // Temporary gold-token artwork; the panel layout does not depend on it.
        renderer.fillRectangle(
            treasuryPanel_.x + 12,
            treasuryPanel_.y + 10,
            15,
            16,
            {124, 85, 19, 255}
        );
        renderer.fillRectangle(
            treasuryPanel_.x + 14,
            treasuryPanel_.y + 9,
            11,
            14,
            {239, 192, 57, 255}
        );
        uiRenderer.drawLabel(
            renderer,
            goldText(treasuryGold_),
            treasuryPanel_.x + 35,
            treasuryPanel_.y + 12,
            1.5F
        );
        populationButton_.render(renderer, uiRenderer);
        const auto populationLabel =
            std::string("Population:") +
            (hasKeep_
                 ? " " + std::to_string(population_) +
                       (worldMode_ ? ""
                                   : "/" + std::to_string(housingCapacity_))
                 : "");
        const float populationScale = std::min(
            1.5F,
            (reservedPanel_.width - 12) /
                (float(populationLabel.size()) * 6 - 1)
        );
        uiRenderer.drawLabel(
            renderer,
            populationLabel,
            centeredLabelX(reservedPanel_, populationLabel, populationScale),
            reservedPanel_.y +
                (reservedPanel_.height - 7 * populationScale) * .5F,
            populationScale
        );

        const std::string visibleCityName =
            cityName_.empty() ? std::string("Unnamed City") : cityName_;

        const UiRectangle nameBounds{
            cityNamePanel_.x,
            cityNamePanel_.y,
            cityNamePanel_.width - 48,
            cityNamePanel_.height
        };
        const float cityNamePixelSize = std::min(
            4.0F,
            (nameBounds.width - 20.0F) /
                std::max(
                    1.0F,
                    static_cast<float>(visibleCityName.size()) * 6.0F - 1.0F
                )
        );

        uiRenderer.drawLabel(
            renderer,
            visibleCityName,
            centeredLabelX(nameBounds, visibleCityName, cityNamePixelSize),
            cityNamePanel_.y +
                (cityNamePanel_.height - 7.0F * cityNamePixelSize) * 0.5F,
            cityNamePixelSize
        );

        renderer.fillRectangle(
            seasonBounds_.x,
            seasonBounds_.y,
            seasonBounds_.width,
            seasonBounds_.height,
            {200, 200, 211, 115}
        );
        renderer.fillRectangle(
            seasonBounds_.x + 2,
            seasonBounds_.y + 2,
            seasonBounds_.width - 4,
            seasonBounds_.height - 4,
            {58, 58, 65, 255}
        );
        const auto season = seasonAtMinute(double(day_ - 1) * 1440);
        constexpr std::array<std::array<std::string_view, 16>, 4> sprites{
            {{{".......yy.......",
               "...y...yy...y...",
               "....y......y....",
               ".....oooooo.....",
               "....oyyyyyyo....",
               "...oyyyyyyyyo...",
               "...oyyyyyyyyo...",
               "yy.oyyyyyyyyo.yy",
               "yy.oyyyyyyyyo.yy",
               "...oyyyyyyyyo...",
               "...oyyyyyyyyo...",
               "....oyyyyyyo....",
               ".....oooooo.....",
               "....y......y....",
               "...y...yy...y...",
               ".......yy......."}},
             {{"................",
               ".....cccc.......",
               "...cccccccc.....",
               "..cccccccccc....",
               "..cccccccccccc..",
               ".cccccccccccccc.",
               ".cccccccccccccc.",
               "..dddddddddddd..",
               "................",
               "....b...b...b...",
               "...b...b...b....",
               "................",
               "..b...b...b.....",
               ".b...b...b......",
               "................",
               "................"}},
             {{"........o.......",
               ".......ooo......",
               "...o..ooooo..o..",
               "...ooooooooooo..",
               "....ooooooooo...",
               "..oooooooooooo..",
               "...ooootooooo...",
               "....oootoooo....",
               "...oooottoooo...",
               "....ooootooo....",
               ".....oootoo.....",
               "......oot.......",
               ".......tt.......",
               ".......t........",
               "......tt........",
               "................"}},
             {{".......b........",
               "....b..b..b.....",
               ".....b.b.b......",
               "...b..bbb..b....",
               "....bbbbbbb.....",
               ".....bbbbb......",
               "..b...bbb...b...",
               ".bbbbbbbbbbbbb..",
               "..b...bbb...b...",
               ".....bbbbb......",
               "....bbbbbbb.....",
               "...b..bbb..b....",
               ".....b.b.b......",
               "....b..b..b.....",
               ".......b........",
               "................"}}}
        };
        const auto& sprite = sprites[std::size_t(season)];
        for (int y = 0; y < 16; ++y)
        {
            for (int x = 0; x < 16; ++x)
            {
                const char pixel = sprite[y][x];
                if (pixel == '.')
                {
                    continue;
                }
                const RenderColor color =
                    pixel == 'y'   ? RenderColor{249, 216, 91, 255}
                    : pixel == 'o' ? RenderColor{219, 131, 56, 255}
                    : pixel == 't' ? RenderColor{112, 73, 45, 255}
                    : pixel == 'c' ? RenderColor{197, 204, 213, 255}
                    : pixel == 'd' ? RenderColor{130, 145, 167, 255}
                                   : RenderColor{126, 198, 240, 255};
                renderer.fillRectangle(
                    seasonBounds_.x + 2 + x * 2,
                    seasonBounds_.y + 2 + y * 2,
                    2,
                    2,
                    color
                );
            }
        }

        const std::string dayAndTime =
            "Day " + std::to_string(day_) + " " + (hour_ < 10 ? "0" : "") +
            std::to_string(hour_) + ":" + (minute_ < 10 ? "0" : "") +
            std::to_string(minute_);

        constexpr float preferredTimePixelSize = 2.0F;
        constexpr float timeHorizontalPadding = 16.0F;

        const float timeWidthAtPreferredSize =
            dayAndTime.empty()
                ? 0.0F
                : (static_cast<float>(dayAndTime.size()) * 6.0F - 1.0F) *
                      preferredTimePixelSize;

        const float availableTimeWidth =
            std::max(1.0F, dayTimePanel_.width - timeHorizontalPadding);

        const float timePixelSize =
            timeWidthAtPreferredSize > availableTimeWidth
                ? preferredTimePixelSize * availableTimeWidth /
                      timeWidthAtPreferredSize
                : preferredTimePixelSize;

        uiRenderer.drawLabel(
            renderer,
            dayAndTime,
            centeredLabelX(dayTimePanel_, dayAndTime, timePixelSize),
            dayTimePanel_.y +
                (dayTimePanel_.height - 7.0F * timePixelSize) * 0.5F,
            timePixelSize
        );

        if (!worldMode_)
        {
            roofsButton_.render(renderer, uiRenderer);
        }
        artButton_.render(renderer, uiRenderer);
        eventsButton_.render(renderer, uiRenderer);
        ledgerButton_.render(renderer, uiRenderer);
        if (worldMode_)
        {
            uiRenderer.drawPanel(renderer, activeSettlementPanel_);
            const std::string label =
                "Active Settlement: " + activeSettlementName_;
            const float scale = std::min(
                1.5F,
                (activeSettlementPanel_.width - 12) /
                    std::max(1.0F, float(label.size() * 6 - 1))
            );
            uiRenderer.drawLabel(
                renderer,
                label,
                6,
                activeSettlementPanel_.y + (36 - 7 * scale) * .5F,
                scale
            );
            for (const auto& button : topButtons_)
            {
                button.render(renderer, uiRenderer);
            }
            return;
        }
        backButton_.render(renderer, uiRenderer);
        for (const UiButton& button : topButtons_)
        {
            button.render(renderer, uiRenderer);
        }
        uiRenderer.drawPanel(renderer, minimapPanel_);
        goodsButton_.render(renderer, uiRenderer);
        if (!goodsIconsLoaded_)
        {
            goodsIconsLoaded_ = true;
            SceneSpriteLibrary icons;
            icons.load(
                renderer,
                std::string(SDL_GetBasePath()) + "assets/sprites"
            );
            const char* names[] = {
                "ui.goods.stone",
                "ui.goods.lumber",
                "ui.goods.fish",
                "ui.goods.meat"
            };
            for (int i = 0; i < 4; ++i)
            {
                if (auto* sprite = icons.find(names[i]))
                {
                    goodsIcons_[i] = sprite->texture;
                }
            }
        }

        if (goodsOpen_)
        {
            for (const auto& cell : goodsCells_)
            {
                uiRenderer.drawPanel(renderer, cell);
            }
            for (std::size_t i = 0; i < 4; ++i)
            {
                const auto& cell = goodsCells_[i];
                const float x = cell.x + cell.width * .5F;
                const float y = cell.y + 9.0F;
                if (const auto& icon = goodsIcons_[i])
                {
                    renderer.drawTexture(
                        *icon,
                        0,
                        0,
                        float(icon->width()),
                        float(icon->height()),
                        x - icon->width() * .5F,
                        y,
                        float(icon->width()),
                        float(icon->height())
                    );
                }
                else
                {
                    renderer.fillRectangle(
                        x - 8,
                        y + 4,
                        16,
                        14,
                        {0xA9, 0x94, 0x78, 255}
                    );
                }
                std::ostringstream amount;
                amount << std::fixed << std::setprecision(0)
                       << std::floor(
                              std::max(
                                  0.0,
                                  i == 0   ? stoneAmount_
                                  : i == 1 ? lumberAmount_
                                  : i == 2 ? fishAmount_
                                           : meatAmount_
                              )
                          );
                const auto label = amount.str();
                const float size = std::min(
                    2.0F,
                    (cell.width - 8) /
                        std::max(1.0F, float(label.size()) * 6 - 1)
                );
                uiRenderer.drawLabel(
                    renderer,
                    label,
                    centeredLabelX(cell, label, size),
                    cell.y + 38,
                    size
                );
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
                SettlementObjectCatalog::definition(definition.objectTypeId);

            if (!objectDefinition)
            {
                continue;
            }

            const UiRectangle& bounds = optionBounds_[index];
            const bool isWorkplaceCategory =
                definition.category == 3 || definition.category == 4;
            const bool isRoad = definition.category == 1;
            const float maximumIconWidth = isWorkplaceCategory
                                               ? bounds.width - 14.0F
                                               : (isRoad ? 18.0F : 36.0F);
            const float maximumIconHeight = isWorkplaceCategory
                                                ? bounds.height - 14.0F
                                                : (isRoad ? 18.0F : 36.0F);
            const float scale = std::min(
                maximumIconWidth / objectDefinition->visual.iconWidth,
                maximumIconHeight / objectDefinition->visual.iconHeight
            );

            const float iconWidth = objectDefinition->visual.iconWidth * scale;
            const float iconHeight =
                objectDefinition->visual.iconHeight * scale;
            const float iconX = bounds.x + (bounds.width - iconWidth) * 0.5F;
            const float iconY = bounds.y + (bounds.height - iconHeight) * 0.5F;

            renderer.fillRectangle(
                iconX,
                iconY,
                iconWidth,
                iconHeight,
                {objectDefinition->visual.frameColor[0],
                 objectDefinition->visual.frameColor[1],
                 objectDefinition->visual.frameColor[2],
                 255}
            );

            constexpr float iconBorder = 2.0F;

            if (iconWidth > iconBorder * 2.0F && iconHeight > iconBorder * 2.0F)
            {
                renderer.fillRectangle(
                    iconX + iconBorder,
                    iconY + iconBorder,
                    iconWidth - iconBorder * 2.0F,
                    iconHeight - iconBorder * 2.0F,
                    {objectDefinition->visual.fillColor[0],
                     objectDefinition->visual.fillColor[1],
                     objectDefinition->visual.fillColor[2],
                     255}
                );
            }

            const bool hasSecondLine = !definition.secondLine.empty();
            const float firstLineY =
                bounds.y + bounds.height * 0.5F - (hasSecondLine ? 8.5F : 3.5F);

            const auto labelPixelSize =
                [&bounds](std::string_view text) noexcept
            {
                constexpr float preferredSize = 1.45F;
                const float preferredWidth =
                    text.empty()
                        ? 0.0F
                        : (static_cast<float>(text.size()) * 6.0F - 1.0F) *
                              preferredSize;

                return preferredWidth > bounds.width - 4.0F
                           ? preferredSize * (bounds.width - 4.0F) /
                                 preferredWidth
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

    bool CityHud::optionIsVisible(std::size_t optionIndex) const noexcept
    {
        return optionIndex < menuOptions.size() &&
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
} // namespace Paladin

namespace Paladin
{
    std::string CityHud::tooltipAt(float x, float y) const
    {
        if (artButton_.containsPoint(x, y))
        {
            return "Environment art: switch sprites on/off (F8). People and "
                   "animals stay unchanged.";
        }
        if (eventsButton_.containsPoint(x, y))
        {
            return worldMode_ ? "World events and realm warnings"
                              : "City events and important warnings";
        }
        if (ledgerButton_.containsPoint(x, y))
        {
            return worldMode_ ? "World ledger - realms and politics"
                              : "City ledger - citizens, production and graphs";
        }
        if (seasonBounds_.contains(x, y))
        {
            return std::string(
                seasonDefinition(seasonAtMinute(double(day_ - 1) * 1440)).name
            );
        }
        if (reservedPanel_.contains(x, y))
        {
            return "Population - history and citizen wellbeing";
        }
        if (goodsButton_.containsPoint(x, y))
        {
            return "Goods stored in containers; excludes construction, "
                   "groundpiles and carried items";
        }
        constexpr std::array titles{
            "Laws - realm and city reforms",
            "Employment - workers and workplaces",
            "Technology",
            "Military",
            "Economy"
        };
        for (std::size_t i = 0; i < topButtons_.size(); ++i)
        {
            if (topButtons_[i].containsPoint(x, y))
            {
                return titles[i];
            }
        }
        if (goodsOpen_)
        {
            constexpr std::array goods{
                "Stone",
                "Lumber",
                "Fish - food",
                "Meat - food",
                "Empty",
                "Empty"
            };
            for (std::size_t i = 0; i < goodsCells_.size(); ++i)
            {
                if (goodsCells_[i].contains(x, y))
                {
                    if (i >= goodsDailyRates_.size()) { return goods[i]; }
                    const auto& rates = goodsDailyRates_[i];
                    const auto number = [](double value)
                    {
                        std::ostringstream text;
                        text << std::fixed << std::setprecision(1) << value;
                        return text.str();
                    };
                    std::string text = std::string(goods[i]) +
                        "\nProduction/day: " + number(rates.production);
                    if (rates.depletion > 1e-7)
                    {
                        text += "\nDepletion/day: " + number(rates.depletion);
                        if (rates.foodEstimate) { text += " (estimated need)"; }
                    }
                    text += "\nProduction: last 24 game hours.";
                    if (rates.foodEstimate && rates.depletion > 1e-7)
                    {
                        text += "\nDaily food need, shared by recent diet."
                                "\nBefore meals are observed: available food mix.";
                    }
                    else if (rates.depletion > 1e-7)
                    {
                        text += "\nDepletion: actual use in the last 24 game hours.";
                    }
                    return text;
                }
            }
        }
        for (std::size_t i = 0; i < optionButtons_.size(); ++i)
        {
            if (!optionIsVisible(i) || !optionButtons_[i].containsPoint(x, y))
            {
                continue;
            }
            if (const auto* d = SettlementObjectCatalog::definition(
                    menuOptions[i].objectTypeId
                ))
            {
                return std::string(d->displayName);
            }
            if (const auto* d = SettlementCommandCatalog::definition(
                    menuOptions[i].commandTypeId
                ))
            {
                return std::string(d->displayName);
            }
        }
        return {};
    }
} // namespace Paladin

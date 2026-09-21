#include "ui/FoundingPanel.h"
#include "world/RealmFlagDesigns.h"

#include "rendering/Renderer.h"
#include "ui/GrayUiRenderer.h"
#include "world/settlements/citizens/SettlementCitizenState.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>

namespace Paladin
{
    namespace
    {
        RenderColor renderColor(MapColor color) noexcept
        {
            return {color.red, color.green, color.blue, 255};
        }
    } // namespace

    FoundingPanel::FoundingPanel()
        : realmNameField_("ENTER REALM NAME", maximumFoundingNameLength),
          cultureNameField_("ENTER CULTURE NAME", maximumFoundingNameLength),
          capitalNameField_("ENTER CITY NAME", maximumFoundingNameLength),
          leftButton_("Cancel"), rightButton_("Continue"),
          pickerDoneButton_("Done")
    {
        refreshButtonState();
    }

    std::uint64_t FoundingPanel::nextRandom() noexcept
    {
        if (randomState_ == 0)
        {
            randomState_ = std::uint64_t(
                std::chrono::steady_clock::now().time_since_epoch().count()
            );
        }
        return randomState_ = GenerationNoise::mix(randomState_);
    }

    void FoundingPanel::suggestNames(bool realm)
    {
        constexpr std::array roots{
            "Aster",   "Valmere", "Dunmar",  "Eldara", "Kestrel", "Ostara",
            "Thalen",  "Nerath",  "Caldrin", "Verden", "Ashen",   "Istria",
            "Merrow",  "Tarsen",  "Orvale",  "Selkar", "Ardent",  "Bracken",
            "Caerwyn", "Darovar", "Estrel",  "Galren", "Harrow",  "Ildren",
            "Junara",  "Keldar",  "Lorien",  "Morven", "Norath",  "Ravelle"
        };
        constexpr std::array endings{"haven", "ford", "wick", "mere", "hold"};
        const std::string root = roots[nextRandom() % roots.size()];
        capitalNameField_.setSuggestedText(
            root + endings[nextRandom() % endings.size()]
        );
        if (realm)
        {
            realmNameField_.setSuggestedText(root);
            cultureNameField_.setSuggestedText(root + " Folk");
        }
    }

    void FoundingPanel::open()
    {
        open_ = true;
        mode_ = FoundingPanelMode::Founding;
        suggestNames(true);
        selectedMapColor_ = randomizedHeraldicColor(nextRandom());
        flag_ = randomizedRealmFlag(nextRandom());
        selectedOriginIndex_ = nextRandom() % originCount;
        colorPickerTarget_ = ColorPickerTarget::None;
        showRealmStep();
    }

    void FoundingPanel::openSettlementChoice()
    {
        openForSettlement();
        step_ = FoundingPanelStep::SettlementType;
        settlementKind_ = SettlementKind::City;
        capitalNameField_.setFocused(false);
        rightButton_.setText("Select region");
        refreshButtonState();
    }

    void FoundingPanel::openForSettlement()
    {
        open_ = true;
        mode_ = FoundingPanelMode::NewSettlement;
        suggestNames(false);
        colorPickerTarget_ = ColorPickerTarget::None;
        showCapitalStep();
    }

    void FoundingPanel::openForCapitalRename(std::string_view currentName)
    {
        open_ = true;
        mode_ = FoundingPanelMode::RenameCapital;
        capitalNameField_.setText(currentName);
        colorPickerTarget_ = ColorPickerTarget::None;
        showCapitalStep();
    }

    void FoundingPanel::openForRealmEdit(const FoundingIdentity& identity)
    {
        open_ = true;
        mode_ = FoundingPanelMode::EditRealm;
        realmNameField_.setText(identity.realmName);
        cultureNameField_.setText(identity.cultureName);
        selectedMapColor_ = identity.mapColor;
        flag_ = identity.flag.isValid() ? identity.flag : RealmFlag{};
        selectedOriginIndex_.reset();

        for (std::size_t index = 0; index < originCount; ++index)
        {
            if (startingRealmOrigins[index].id == identity.realmOriginId)
            {
                selectedOriginIndex_ = index;
                break;
            }
        }

        colorPickerTarget_ = ColorPickerTarget::None;
        showRealmStep();
    }

    void FoundingPanel::close() noexcept
    {
        open_ = false;
        colorPickerTarget_ = ColorPickerTarget::None;
        draggedColorChannel_.reset();
        realmNameField_.setFocused(false);
        cultureNameField_.setFocused(false);
        capitalNameField_.setFocused(false);
        leftButton_.cancelPress();
        rightButton_.cancelPress();
        pickerDoneButton_.cancelPress();
        rulerReloadButton_.cancelPress();
        flagPresetButton_.cancelPress();
        flagStrokeActive_ = false;
    }

    bool FoundingPanel::isOpen() const noexcept
    {
        return open_;
    }
    FoundingPanelStep FoundingPanel::step() const noexcept
    {
        return step_;
    }
    FoundingPanelMode FoundingPanel::mode() const noexcept
    {
        return mode_;
    }

    void FoundingPanel::layout(int viewportWidth, int viewportHeight) noexcept
    {
        const bool realmStep = step_ == FoundingPanelStep::Realm;
        const float panelWidth = std::min(
            realmStep ? 760.0F : 680.0F,
            static_cast<float>(viewportWidth) - 32.0F
        );
        const float panelHeight =
            realmStep
                ? std::min(680.0F, static_cast<float>(viewportHeight) - 24.0F)
                : (step_ == FoundingPanelStep::SettlementType ? 380.0F
                                                              : 300.0F);

        panelBounds_ = {
            (static_cast<float>(viewportWidth) - panelWidth) * 0.5F,
            (static_cast<float>(viewportHeight) - panelHeight) * 0.5F,
            panelWidth,
            panelHeight
        };

        const float fieldX = panelBounds_.x + 250.0F;
        const float fieldWidth = panelBounds_.width - 282.0F;
        realmNameField_.setBounds(
            {fieldX, panelBounds_.y + 76.0F, fieldWidth, 44.0F}
        );
        cultureNameField_.setBounds(
            {fieldX, panelBounds_.y + 132.0F, fieldWidth, 44.0F}
        );
        capitalNameField_.setBounds(
            {fieldX, panelBounds_.y + 112.0F, fieldWidth, 44.0F}
        );
        mapColorBounds_ = {fieldX, panelBounds_.y + 190.0F, 44.0F, 44.0F};

        constexpr float flagCellSize = 20.0F;
        const float flagX = panelBounds_.x + 32.0F;
        const float flagY = panelBounds_.y + 304.0F;
        flagPresetButton_.setBounds({flagX + 68, flagY - 39, 112, 30});

        for (std::size_t y = 0; y < RealmFlag::defaultHeight; ++y)
        {
            for (std::size_t x = 0; x < RealmFlag::defaultWidth; ++x)
            {
                const std::size_t index = y * RealmFlag::defaultWidth + x;
                flagCellBounds_[index] = {
                    flagX + static_cast<float>(x) * flagCellSize,
                    flagY + static_cast<float>(y) * flagCellSize,
                    flagCellSize,
                    flagCellSize
                };
            }
        }

        flagColorBounds_ =
            {flagX + 148.0F, panelBounds_.y + 508.0F, 44.0F, 44.0F};

        const float originX = panelBounds_.x + 224.0F;
        const float originWidth = (panelBounds_.width - 280.0F) * .53F;
        for (std::size_t index = 0; index < originCount; ++index)
        {
            originBounds_[index] = {
                originX,
                panelBounds_.y + 304 + float(index) * 86,
                originWidth,
                72
            };
        }
        rulerBounds_ = {
            originX + originWidth + 22,
            panelBounds_.y + 304,
            panelBounds_.width - 256 - originWidth - 22,
            72
        };
        rulerReloadButton_.setBounds(
            {rulerBounds_.x, rulerBounds_.y + 88, rulerBounds_.width, 44}
        );
        for (std::size_t i = 0; i < 2; ++i)
        {
            settlementKindBounds_[i] = {
                panelBounds_.x + 32 + float(i) * (panelWidth - 48) / 2,
                panelBounds_.y + 100,
                (panelWidth - 80) / 2,
                100
            };
        }

        leftButton_.setBounds(
            {panelBounds_.x + 32.0F,
             panelBounds_.y + panelBounds_.height - 68.0F,
             150.0F,
             44.0F}
        );
        rightButton_.setBounds(
            {panelBounds_.x + panelBounds_.width - 182.0F,
             panelBounds_.y + panelBounds_.height - 68.0F,
             150.0F,
             44.0F}
        );

        pickerBounds_ = {
            (static_cast<float>(viewportWidth) - 520.0F) * 0.5F,
            (static_cast<float>(viewportHeight) - 330.0F) * 0.5F,
            520.0F,
            330.0F
        };

        for (std::size_t channel = 0; channel < 3; ++channel)
        {
            pickerChannelBounds_[channel] = {
                pickerBounds_.x + 82.0F,
                pickerBounds_.y + 88.0F + static_cast<float>(channel) * 58.0F,
                330.0F,
                28.0F
            };
        }

        pickerDoneButton_.setBounds(
            {pickerBounds_.x + pickerBounds_.width - 132.0F,
             pickerBounds_.y + pickerBounds_.height - 60.0F,
             100.0F,
             36.0F}
        );
    }

    void FoundingPanel::pointerMoved(float x, float y) noexcept
    {
        if (colorPickerTarget_ != ColorPickerTarget::None)
        {
            pickerDoneButton_.pointerMoved(x, y);
            if (draggedColorChannel_)
            {
                updateColorChannel(*draggedColorChannel_, x);
            }
            return;
        }

        if (step_ == FoundingPanelStep::Realm)
        {
            flagPresetButton_.pointerMoved(x, y);
        }
        leftButton_.pointerMoved(x, y);
        rightButton_.pointerMoved(x, y);
        if (mode_ == FoundingPanelMode::Founding &&
            step_ == FoundingPanelStep::Realm)
        {
            rulerReloadButton_.pointerMoved(x, y);
        }
        hoveredOriginIndex_.reset();
        hoveredFlagCell_.reset();
        mapColorHovered_ = mapColorBounds_.contains(x, y);
        flagColorHovered_ = flagColorBounds_.contains(x, y);

        if (step_ != FoundingPanelStep::Realm)
        {
            return;
        }

        for (std::size_t index = 0; index < originCount; ++index)
        {
            if (originBounds_[index].contains(x, y))
            {
                hoveredOriginIndex_ = index;
                break;
            }
        }

        for (std::size_t index = 0; index < flagCellCount; ++index)
        {
            if (flagCellBounds_[index].contains(x, y))
            {
                hoveredFlagCell_ = index;
                if (flagStrokeActive_)
                {
                    applyFlagStroke(index);
                }
                break;
            }
        }
    }

    void FoundingPanel::pointerPressed(float x, float y) noexcept
    {
        if (colorPickerTarget_ != ColorPickerTarget::None)
        {
            draggedColorChannel_.reset();
            for (std::size_t channel = 0; channel < 3; ++channel)
            {
                if (pickerChannelBounds_[channel].contains(x, y))
                {
                    draggedColorChannel_ = channel;
                    updateColorChannel(channel, x);
                    break;
                }
            }
            static_cast<void>(pickerDoneButton_.pointerPressed(x, y));
            return;
        }

        if (step_ == FoundingPanelStep::Realm)
        {
            static_cast<void>(flagPresetButton_.pointerPressed(x, y));
            realmNameField_.setFocused(realmNameField_.contains(x, y));
            cultureNameField_.setFocused(cultureNameField_.contains(x, y));
            capitalNameField_.setFocused(false);

            if (mapColorBounds_.contains(x, y))
            {
                openColorPicker(ColorPickerTarget::Map);
                return;
            }
            if (flagColorBounds_.contains(x, y))
            {
                openColorPicker(ColorPickerTarget::FlagPrimary);
                return;
            }

            for (std::size_t index = 0; index < flagCellCount; ++index)
            {
                if (flagCellBounds_[index].contains(x, y))
                {
                    const FlagCell& cell = flag_.cells[index];
                    flagStrokePaints_ =
                        !cell.painted || cell.color != flag_.primaryColor;
                    flagStrokeActive_ = true;
                    applyFlagStroke(index);
                    break;
                }
            }

            pressedOriginIndex_.reset();
            for (std::size_t index = 0; index < originCount; ++index)
            {
                if (originBounds_[index].contains(x, y))
                {
                    selectedOriginIndex_ = index;
                    pressedOriginIndex_ = index;
                    break;
                }
            }
        }
        else
        {
            capitalNameField_.setFocused(capitalNameField_.contains(x, y));
            realmNameField_.setFocused(false);
            cultureNameField_.setFocused(false);
        }

        if (mode_ == FoundingPanelMode::Founding &&
            step_ == FoundingPanelStep::Realm)
        {
            static_cast<void>(rulerReloadButton_.pointerPressed(x, y));
        }
        if (step_ == FoundingPanelStep::SettlementType)
        {
            for (std::size_t i = 0; i < 2; ++i)
            {
                if (settlementKindBounds_[i].contains(x, y))
                {
                    settlementKind_ = i == 0 ? SettlementKind::City
                                             : SettlementKind::Fortress;
                }
            }
        }
        static_cast<void>(leftButton_.pointerPressed(x, y));
        static_cast<void>(rightButton_.pointerPressed(x, y));
        refreshButtonState();
    }

    FoundingPanelAction FoundingPanel::pointerReleased(float x, float y)
    {
        flagStrokeActive_ = false;

        if (colorPickerTarget_ != ColorPickerTarget::None)
        {
            draggedColorChannel_.reset();
            if (pickerDoneButton_.pointerReleased(x, y))
            {
                colorPickerTarget_ = ColorPickerTarget::None;
            }
            return FoundingPanelAction::None;
        }

        if (mode_ == FoundingPanelMode::Founding &&
            step_ == FoundingPanelStep::Realm &&
            rulerReloadButton_.pointerReleased(x, y))
        {
            rulerNameIndex_ = (rulerNameIndex_ + 1) % 100;
        }
        if (step_ == FoundingPanelStep::Realm &&
            flagPresetButton_.pointerReleased(x, y))
        {
            const auto previous = flag_;
            do
            {
                flag_ = randomizedRealmFlag(nextRandom());
            } while (flag_ == previous);
        }
        pressedOriginIndex_.reset();
        const bool leftClicked = leftButton_.pointerReleased(x, y);
        const bool rightClicked = rightButton_.pointerReleased(x, y);

        if (leftClicked)
        {
            if (mode_ == FoundingPanelMode::Founding &&
                step_ == FoundingPanelStep::Capital)
            {
                showRealmStep();
                return FoundingPanelAction::None;
            }
            return FoundingPanelAction::Cancel;
        }

        return rightClicked ? submit() : FoundingPanelAction::None;
    }

    FoundingPanelAction FoundingPanel::submit()
    {
        if (colorPickerTarget_ != ColorPickerTarget::None)
        {
            colorPickerTarget_ = ColorPickerTarget::None;
            return FoundingPanelAction::None;
        }

        if (mode_ == FoundingPanelMode::Founding &&
            step_ == FoundingPanelStep::Realm)
        {
            if (canContinueFromRealm())
            {
                showCapitalStep();
            }
            return FoundingPanelAction::None;
        }

        if (step_ == FoundingPanelStep::SettlementType)
        {
            return FoundingPanelAction::SelectRegion;
        }
        return canConfirm() ? FoundingPanelAction::Confirm
                            : FoundingPanelAction::None;
    }

    bool FoundingPanel::closeTopLayer() noexcept
    {
        if (colorPickerTarget_ == ColorPickerTarget::None)
        {
            return false;
        }
        colorPickerTarget_ = ColorPickerTarget::None;
        draggedColorChannel_.reset();
        pickerDoneButton_.cancelPress();
        rulerReloadButton_.cancelPress();
        flagPresetButton_.cancelPress();
        return true;
    }

    void FoundingPanel::appendText(std::string_view text)
    {
        if (realmNameField_.focused())
        {
            realmNameField_.appendText(text);
        }
        else if (cultureNameField_.focused())
        {
            cultureNameField_.appendText(text);
        }
        else if (capitalNameField_.focused())
        {
            capitalNameField_.appendText(text);
        }
        refreshButtonState();
    }

    void FoundingPanel::backspace() noexcept
    {
        if (realmNameField_.focused())
        {
            realmNameField_.backspace();
        }
        else if (cultureNameField_.focused())
        {
            cultureNameField_.backspace();
        }
        else if (capitalNameField_.focused())
        {
            capitalNameField_.backspace();
        }
        refreshButtonState();
    }

    void FoundingPanel::focusNextField() noexcept
    {
        if (step_ == FoundingPanelStep::Capital)
        {
            capitalNameField_.setFocused(true);
            return;
        }

        const bool realmWasFocused = realmNameField_.focused();
        realmNameField_.setFocused(!realmWasFocused);
        cultureNameField_.setFocused(realmWasFocused);
    }

    bool FoundingPanel::canConfirm() const noexcept
    {
        if (mode_ == FoundingPanelMode::RenameCapital ||
            mode_ == FoundingPanelMode::NewSettlement)
        {
            return isValidFoundingName(capitalNameField_.text());
        }
        if (mode_ == FoundingPanelMode::EditRealm)
        {
            return canContinueFromRealm();
        }
        return step_ == FoundingPanelStep::Capital && canContinueFromRealm() &&
               isValidFoundingName(capitalNameField_.text());
    }

    FoundingIdentity FoundingPanel::identity() const
    {
        const std::string_view originId =
            selectedOriginIndex_
                ? startingRealmOrigins[*selectedOriginIndex_].id
                : std::string_view{};

        return {
            trimFoundingName(realmNameField_.text()),
            trimFoundingName(cultureNameField_.text()),
            trimFoundingName(capitalNameField_.text()),
            selectedMapColor_,
            std::string(originId),
            flag_,
            std::string(SettlementCitizenState::maleName(rulerNameIndex_))
        };
    }

    MapColor FoundingPanel::selectedColor() const noexcept
    {
        return selectedMapColor_;
    }

    void FoundingPanel::render(
        Renderer& renderer,
        const GrayUiRenderer& uiRenderer
    )
    {
        if (!open_)
        {
            return;
        }

        refreshButtonState();
        uiRenderer.drawModalBackdrop(renderer);
        uiRenderer.drawPanel(renderer, panelBounds_);
        const auto heading = [&](std::string_view text,
                                 const UiRectangle& box,
                                 float preferredSize)
        {
            const BitmapFontRenderer font;
            const float size = std::min(
                preferredSize,
                (box.width - 64) / font.measureWidth(text, 1)
            );
            uiRenderer.drawLabel(
                renderer,
                text,
                box.x + (box.width - font.measureWidth(text, size)) * .5F,
                box.y + 24,
                size
            );
        };

        if (step_ == FoundingPanelStep::Realm)
        {
            heading(
                mode_ == FoundingPanelMode::EditRealm ? "EDIT YOUR REALM"
                                                      : "FOUND YOUR REALM",
                panelBounds_,
                4.4F
            );
            uiRenderer.drawLabel(
                renderer,
                "REALM NAME",
                panelBounds_.x + 32.0F,
                panelBounds_.y + 87.0F
            );
            uiRenderer.drawLabel(
                renderer,
                "CULTURE NAME",
                panelBounds_.x + 32.0F,
                panelBounds_.y + 143.0F
            );
            uiRenderer.drawLabel(
                renderer,
                "MAP COLOR",
                panelBounds_.x + 32.0F,
                panelBounds_.y + 201.0F
            );
            realmNameField_.render(renderer, uiRenderer);
            cultureNameField_.render(renderer, uiRenderer);
            uiRenderer.drawColorSwatch(
                renderer,
                mapColorBounds_,
                renderColor(selectedMapColor_),
                mapColorHovered_,
                true
            );

            uiRenderer.drawLabel(
                renderer,
                "FLAG",
                panelBounds_.x + 32.0F,
                panelBounds_.y + 272.0F,
                2.5F
            );
            flagPresetButton_.render(renderer, uiRenderer);
            for (std::size_t index = 0; index < flagCellCount; ++index)
            {
                const FlagCell& cell = flag_.cells[index];
                const RenderColor color = cell.painted
                                              ? renderColor(cell.color)
                                              : RenderColor{40, 40, 44, 255};
                const UiRectangle& bounds = flagCellBounds_[index];

                renderer.fillRectangle(
                    bounds.x,
                    bounds.y,
                    bounds.width - 1.0F,
                    bounds.height - 1.0F,
                    hoveredFlagCell_ == index ? RenderColor{218, 218, 224, 255}
                                              : color
                );

                if (hoveredFlagCell_ == index)
                {
                    renderer.fillRectangle(
                        bounds.x + 1.0F,
                        bounds.y + 1.0F,
                        bounds.width - 3.0F,
                        bounds.height - 3.0F,
                        color
                    );
                }
            }
            uiRenderer.drawLabel(
                renderer,
                "FLAG COLOR",
                panelBounds_.x + 32.0F,
                panelBounds_.y + 519.0F,
                2.5F
            );
            uiRenderer.drawColorSwatch(
                renderer,
                flagColorBounds_,
                renderColor(flag_.primaryColor),
                flagColorHovered_,
                true
            );

            uiRenderer.drawLabel(
                renderer,
                "GOVERNMENT TYPE",
                panelBounds_.x + 224.0F,
                panelBounds_.y + 272.0F,
                2.5F,
                {216, 216, 220, 255}
            );
            for (std::size_t index = 0; index < originCount; ++index)
            {
                uiRenderer.drawChoiceCard(
                    renderer,
                    originBounds_[index],
                    startingRealmOrigins[index].displayName,
                    hoveredOriginIndex_ == index,
                    pressedOriginIndex_ == index,
                    selectedOriginIndex_ == index
                );
            }

            if (mode_ == FoundingPanelMode::Founding)
            {
                uiRenderer.drawLabel(
                    renderer,
                    "RULER",
                    rulerBounds_.x,
                    panelBounds_.y + 272,
                    2.5F
                );
                uiRenderer.drawChoiceCard(
                    renderer,
                    rulerBounds_,
                    SettlementCitizenState::maleName(rulerNameIndex_),
                    false,
                    false,
                    true
                );
                rulerReloadButton_.render(renderer, uiRenderer);
            }

            if (!canContinueFromRealm())
            {
                uiRenderer.drawLabel(
                    renderer,
                    "ENTER BOTH NAMES AND CHOOSE A GOVERNMENT",
                    panelBounds_.x + 224.0F,
                    panelBounds_.y + 565.0F,
                    2.0F,
                    {190, 190, 196, 255}
                );
            }
        }
        else if (step_ == FoundingPanelStep::SettlementType)
        {
            heading("FOUND A NEW SETTLEMENT", panelBounds_, 3.5F);
            for (std::size_t i = 0; i < 2; ++i)
            {
                uiRenderer.drawChoiceCard(
                    renderer,
                    settlementKindBounds_[i],
                    i == 0 ? "City" : "Fortress",
                    false,
                    false,
                    settlementKind_ == (i == 0 ? SettlementKind::City
                                               : SettlementKind::Fortress)
                );
            }
            uiRenderer.drawLabel(
                renderer,
                settlementKind_ == SettlementKind::Fortress
                    ? "COMPACT MAP, STRONGER TERRITORIAL CONTROL"
                    : "FULL CITY MAP AND CONSTRUCTION OPTIONS",
                panelBounds_.x + 32,
                panelBounds_.y + 220,
                2.0F
            );
            if (settlementKind_ == SettlementKind::Fortress)
            {
                uiRenderer.drawLabel(
                    renderer,
                    "FINITE STARTING SUPPLIES",
                    panelBounds_.x + 32,
                    panelBounds_.y + 248,
                    2.0F
                );
            }
        }
        else
        {
            heading(
                mode_ == FoundingPanelMode::NewSettlement
                    ? "FOUND NEW SETTLEMENT"
                : mode_ == FoundingPanelMode::RenameCapital
                    ? "RENAME CAPITAL"
                    : "FOUND YOUR CAPITAL",
                panelBounds_,
                4.4F
            );
            uiRenderer.drawLabel(
                renderer,
                mode_ == FoundingPanelMode::NewSettlement
                    ? "SETTLEMENT OF YOUR REALM"
                    : "CAPITAL OF YOUR REALM",
                panelBounds_.x + 32.0F,
                panelBounds_.y + 73.0F,
                2.0F,
                {194, 194, 200, 255}
            );
            uiRenderer.drawLabel(
                renderer,
                mode_ == FoundingPanelMode::NewSettlement &&
                        settlementKind_ == SettlementKind::Fortress
                    ? "FORTRESS NAME"
                    : "CITY NAME",
                panelBounds_.x + 32.0F,
                panelBounds_.y + 123.0F
            );
            capitalNameField_.render(renderer, uiRenderer);
        }

        leftButton_.render(renderer, uiRenderer);
        rightButton_.render(renderer, uiRenderer);

        if (colorPickerTarget_ != ColorPickerTarget::None)
        {
            uiRenderer.drawModalBackdrop(renderer);
            uiRenderer.drawPanel(renderer, pickerBounds_);
            heading("RGB COLOR", pickerBounds_, 3.8F);

            const MapColor color = pickerColor();
            const std::array<std::uint8_t, 3> values{
                color.red,
                color.green,
                color.blue
            };
            const std::array<std::string_view, 3> labels{"R", "G", "B"};
            const std::array<RenderColor, 3> fills{
                RenderColor{220, 64, 64, 255},
                RenderColor{64, 200, 92, 255},
                RenderColor{64, 112, 224, 255}
            };

            for (std::size_t channel = 0; channel < 3; ++channel)
            {
                const UiRectangle& bounds = pickerChannelBounds_[channel];
                uiRenderer.drawLabel(
                    renderer,
                    labels[channel],
                    pickerBounds_.x + 36.0F,
                    bounds.y + 4.0F,
                    2.5F
                );
                renderer.fillRectangle(
                    bounds.x,
                    bounds.y,
                    bounds.width,
                    bounds.height,
                    {110, 110, 116, 255}
                );
                renderer.fillRectangle(
                    bounds.x + 2.0F,
                    bounds.y + 2.0F,
                    bounds.width - 4.0F,
                    bounds.height - 4.0F,
                    {38, 38, 42, 255}
                );
                renderer.fillRectangle(
                    bounds.x + 2.0F,
                    bounds.y + 2.0F,
                    (bounds.width - 4.0F) *
                        static_cast<float>(values[channel]) / 255.0F,
                    bounds.height - 4.0F,
                    fills[channel]
                );
                uiRenderer.drawLabel(
                    renderer,
                    std::to_string(values[channel]),
                    bounds.x + bounds.width + 14.0F,
                    bounds.y + 4.0F,
                    2.5F
                );
            }

            uiRenderer.drawColorSwatch(
                renderer,
                {pickerBounds_.x + 28.0F,
                 pickerBounds_.y + 264.0F,
                 72.0F,
                 42.0F},
                renderColor(color),
                false,
                true
            );
            pickerDoneButton_.render(renderer, uiRenderer);
        }
    }

    bool FoundingPanel::canContinueFromRealm() const noexcept
    {
        return isValidFoundingName(realmNameField_.text()) &&
               isValidFoundingName(cultureNameField_.text()) &&
               selectedOriginIndex_.has_value() && flag_.isValid();
    }

    void FoundingPanel::showRealmStep()
    {
        step_ = FoundingPanelStep::Realm;
        realmNameField_.setFocused(true);
        cultureNameField_.setFocused(false);
        capitalNameField_.setFocused(false);
        leftButton_.setText("Cancel");
        rightButton_.setText(
            mode_ == FoundingPanelMode::Founding ? "Continue" : "Confirm"
        );
        refreshButtonState();
    }

    void FoundingPanel::showCapitalStep()
    {
        step_ = FoundingPanelStep::Capital;
        realmNameField_.setFocused(false);
        cultureNameField_.setFocused(false);
        capitalNameField_.setFocused(true);
        leftButton_.setText(
            mode_ == FoundingPanelMode::Founding ? "Back" : "Cancel"
        );
        rightButton_.setText("Confirm");
        refreshButtonState();
    }

    void FoundingPanel::refreshButtonState()
    {
        rightButton_.setEnabled(
            step_ == FoundingPanelStep::SettlementType ? true
            : mode_ == FoundingPanelMode::Founding &&
                    step_ == FoundingPanelStep::Realm
                ? canContinueFromRealm()
                : canConfirm()
        );
    }

    void FoundingPanel::openColorPicker(ColorPickerTarget target) noexcept
    {
        colorPickerTarget_ = target;
        draggedColorChannel_.reset();
        realmNameField_.setFocused(false);
        cultureNameField_.setFocused(false);
    }

    void FoundingPanel::updateColorChannel(
        std::size_t channel,
        float pointerX
    ) noexcept
    {
        const UiRectangle& bounds = pickerChannelBounds_[channel];
        const float ratio =
            std::clamp((pointerX - bounds.x) / bounds.width, 0.0F, 1.0F);
        const std::uint8_t value =
            static_cast<std::uint8_t>(std::lround(ratio * 255.0F));
        MapColor& color = pickerColor();
        if (channel == 0)
        {
            color.red = value;
        }
        else if (channel == 1)
        {
            color.green = value;
        }
        else
        {
            color.blue = value;
        }
    }

    void FoundingPanel::applyFlagStroke(std::size_t cellIndex) noexcept
    {
        if (cellIndex >= flag_.cells.size())
        {
            return;
        }

        FlagCell& cell = flag_.cells[cellIndex];
        cell.painted = flagStrokePaints_;

        if (flagStrokePaints_)
        {
            cell.color = flag_.primaryColor;
        }
    }

    MapColor& FoundingPanel::pickerColor() noexcept
    {
        return colorPickerTarget_ == ColorPickerTarget::FlagPrimary
                   ? flag_.primaryColor
                   : selectedMapColor_;
    }

    const MapColor& FoundingPanel::pickerColor() const noexcept
    {
        return colorPickerTarget_ == ColorPickerTarget::FlagPrimary
                   ? flag_.primaryColor
                   : selectedMapColor_;
    }
} // namespace Paladin

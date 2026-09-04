#include "ui/FoundingPanel.h"

#include "rendering/Renderer.h"
#include "ui/GrayUiRenderer.h"

#include <algorithm>
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
    }

    FoundingPanel::FoundingPanel()
        : polityNameField_("ENTER POLITY NAME", maximumFoundingNameLength),
          cultureNameField_("ENTER CULTURE NAME", maximumFoundingNameLength),
          capitalNameField_("ENTER CITY NAME", maximumFoundingNameLength),
          leftButton_("Cancel"),
          rightButton_("Continue"),
          pickerDoneButton_("Done")
    {
        refreshButtonState();
    }

    void FoundingPanel::open()
    {
        open_ = true;
        mode_ = FoundingPanelMode::Founding;
        polityNameField_.clear();
        cultureNameField_.clear();
        capitalNameField_.clear();
        selectedMapColor_ = {210, 54, 54};
        flag_ = {};
        selectedOriginIndex_.reset();
        colorPickerTarget_ = ColorPickerTarget::None;
        showPolityStep();
    }

    void FoundingPanel::openForCapitalRename(std::string_view currentName)
    {
        open_ = true;
        mode_ = FoundingPanelMode::RenameCapital;
        capitalNameField_.setText(currentName);
        colorPickerTarget_ = ColorPickerTarget::None;
        showCapitalStep();
    }

    void FoundingPanel::openForPolityEdit(const FoundingIdentity& identity)
    {
        open_ = true;
        mode_ = FoundingPanelMode::EditPolity;
        polityNameField_.setText(identity.polityName);
        cultureNameField_.setText(identity.cultureName);
        selectedMapColor_ = identity.mapColor;
        flag_ = identity.flag.isValid() ? identity.flag : PolityFlag{};
        selectedOriginIndex_.reset();

        for (std::size_t index = 0; index < originCount; ++index)
        {
            if (startingPolityOrigins[index].id == identity.polityOriginId)
            {
                selectedOriginIndex_ = index;
                break;
            }
        }

        colorPickerTarget_ = ColorPickerTarget::None;
        showPolityStep();
    }

    void FoundingPanel::close() noexcept
    {
        open_ = false;
        colorPickerTarget_ = ColorPickerTarget::None;
        draggedColorChannel_.reset();
        polityNameField_.setFocused(false);
        cultureNameField_.setFocused(false);
        capitalNameField_.setFocused(false);
        leftButton_.cancelPress();
        rightButton_.cancelPress();
        pickerDoneButton_.cancelPress();
        flagStrokeActive_ = false;
    }

    bool FoundingPanel::isOpen() const noexcept { return open_; }
    FoundingPanelStep FoundingPanel::step() const noexcept { return step_; }
    FoundingPanelMode FoundingPanel::mode() const noexcept { return mode_; }

    void FoundingPanel::layout(int viewportWidth, int viewportHeight) noexcept
    {
        const bool polityStep = step_ == FoundingPanelStep::Polity;
        const float panelWidth = std::min(
            polityStep ? 760.0F : 680.0F,
            static_cast<float>(viewportWidth) - 32.0F
        );
        const float panelHeight = polityStep
            ? std::min(680.0F, static_cast<float>(viewportHeight) - 24.0F)
            : 300.0F;

        panelBounds_ = {
            (static_cast<float>(viewportWidth) - panelWidth) * 0.5F,
            (static_cast<float>(viewportHeight) - panelHeight) * 0.5F,
            panelWidth,
            panelHeight
        };

        const float fieldX = panelBounds_.x + 250.0F;
        const float fieldWidth = panelBounds_.width - 282.0F;
        polityNameField_.setBounds({fieldX, panelBounds_.y + 76.0F, fieldWidth, 44.0F});
        cultureNameField_.setBounds({fieldX, panelBounds_.y + 132.0F, fieldWidth, 44.0F});
        capitalNameField_.setBounds({fieldX, panelBounds_.y + 112.0F, fieldWidth, 44.0F});
        mapColorBounds_ = {fieldX, panelBounds_.y + 190.0F, 44.0F, 44.0F};

        constexpr float flagCellSize = 20.0F;
        const float flagX = panelBounds_.x + 32.0F;
        const float flagY = panelBounds_.y + 304.0F;

        for (std::size_t y = 0; y < PolityFlag::defaultHeight; ++y)
        {
            for (std::size_t x = 0; x < PolityFlag::defaultWidth; ++x)
            {
                const std::size_t index = y * PolityFlag::defaultWidth + x;
                flagCellBounds_[index] = {
                    flagX + static_cast<float>(x) * flagCellSize,
                    flagY + static_cast<float>(y) * flagCellSize,
                    flagCellSize,
                    flagCellSize
                };
            }
        }

        flagColorBounds_ = {flagX + 148.0F, panelBounds_.y + 508.0F, 44.0F, 44.0F};

        constexpr float originGap = 18.0F;
        const float originX = panelBounds_.x + 224.0F;
        const float originAreaWidth = panelBounds_.width - 256.0F;
        const float originWidth =
            (originAreaWidth - originGap * static_cast<float>(originCount - 1))
            / static_cast<float>(originCount);

        for (std::size_t index = 0; index < originCount; ++index)
        {
            originBounds_[index] = {
                originX + static_cast<float>(index) * (originWidth + originGap),
                panelBounds_.y + 304.0F,
                originWidth,
                190.0F
            };
        }

        leftButton_.setBounds({panelBounds_.x + 32.0F, panelBounds_.y + panelBounds_.height - 68.0F, 150.0F, 44.0F});
        rightButton_.setBounds({panelBounds_.x + panelBounds_.width - 182.0F, panelBounds_.y + panelBounds_.height - 68.0F, 150.0F, 44.0F});

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

        pickerDoneButton_.setBounds({pickerBounds_.x + pickerBounds_.width - 132.0F, pickerBounds_.y + pickerBounds_.height - 60.0F, 100.0F, 36.0F});
    }

    void FoundingPanel::pointerMoved(float x, float y) noexcept
    {
        if (colorPickerTarget_ != ColorPickerTarget::None)
        {
            pickerDoneButton_.pointerMoved(x, y);
            if (draggedColorChannel_) updateColorChannel(*draggedColorChannel_, x);
            return;
        }

        leftButton_.pointerMoved(x, y);
        rightButton_.pointerMoved(x, y);
        hoveredOriginIndex_.reset();
        hoveredFlagCell_.reset();
        mapColorHovered_ = mapColorBounds_.contains(x, y);
        flagColorHovered_ = flagColorBounds_.contains(x, y);

        if (step_ != FoundingPanelStep::Polity) return;

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

        if (step_ == FoundingPanelStep::Polity)
        {
            polityNameField_.setFocused(polityNameField_.contains(x, y));
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
                        !cell.painted ||
                        cell.color != flag_.primaryColor;
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
            polityNameField_.setFocused(false);
            cultureNameField_.setFocused(false);
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

        pressedOriginIndex_.reset();
        const bool leftClicked = leftButton_.pointerReleased(x, y);
        const bool rightClicked = rightButton_.pointerReleased(x, y);

        if (leftClicked)
        {
            if (mode_ == FoundingPanelMode::Founding && step_ == FoundingPanelStep::Capital)
            {
                showPolityStep();
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

        if (mode_ == FoundingPanelMode::Founding && step_ == FoundingPanelStep::Polity)
        {
            if (canContinueFromPolity()) showCapitalStep();
            return FoundingPanelAction::None;
        }

        return canConfirm() ? FoundingPanelAction::Confirm : FoundingPanelAction::None;
    }

    bool FoundingPanel::closeTopLayer() noexcept
    {
        if (colorPickerTarget_ == ColorPickerTarget::None) return false;
        colorPickerTarget_ = ColorPickerTarget::None;
        draggedColorChannel_.reset();
        pickerDoneButton_.cancelPress();
        return true;
    }

    void FoundingPanel::appendText(std::string_view text)
    {
        if (polityNameField_.focused()) polityNameField_.appendText(text);
        else if (cultureNameField_.focused()) cultureNameField_.appendText(text);
        else if (capitalNameField_.focused()) capitalNameField_.appendText(text);
        refreshButtonState();
    }

    void FoundingPanel::backspace() noexcept
    {
        if (polityNameField_.focused()) polityNameField_.backspace();
        else if (cultureNameField_.focused()) cultureNameField_.backspace();
        else if (capitalNameField_.focused()) capitalNameField_.backspace();
        refreshButtonState();
    }

    void FoundingPanel::focusNextField() noexcept
    {
        if (step_ == FoundingPanelStep::Capital)
        {
            capitalNameField_.setFocused(true);
            return;
        }

        const bool polityWasFocused = polityNameField_.focused();
        polityNameField_.setFocused(!polityWasFocused);
        cultureNameField_.setFocused(polityWasFocused);
    }

    bool FoundingPanel::canConfirm() const noexcept
    {
        if (mode_ == FoundingPanelMode::RenameCapital)
        {
            return isValidFoundingName(capitalNameField_.text());
        }
        if (mode_ == FoundingPanelMode::EditPolity)
        {
            return canContinueFromPolity();
        }
        return step_ == FoundingPanelStep::Capital &&
            canContinueFromPolity() &&
            isValidFoundingName(capitalNameField_.text());
    }

    FoundingIdentity FoundingPanel::identity() const
    {
        const std::string_view originId = selectedOriginIndex_
            ? startingPolityOrigins[*selectedOriginIndex_].id
            : std::string_view{};

        return {
            trimFoundingName(polityNameField_.text()),
            trimFoundingName(cultureNameField_.text()),
            trimFoundingName(capitalNameField_.text()),
            selectedMapColor_,
            std::string(originId),
            flag_
        };
    }

    MapColor FoundingPanel::selectedColor() const noexcept
    {
        return selectedMapColor_;
    }

    void FoundingPanel::render(Renderer& renderer, const GrayUiRenderer& uiRenderer)
    {
        if (!open_) return;

        refreshButtonState();
        uiRenderer.drawModalBackdrop(renderer);
        uiRenderer.drawPanel(renderer, panelBounds_);

        if (step_ == FoundingPanelStep::Polity)
        {
            uiRenderer.drawLabel(
                renderer,
                mode_ == FoundingPanelMode::EditPolity
                    ? "EDIT YOUR POLITY"
                    : "FOUND YOUR POLITY",
                panelBounds_.x + 32.0F,
                panelBounds_.y + 24.0F,
                4.0F
            );
            uiRenderer.drawLabel(renderer, "POLITY NAME", panelBounds_.x + 32.0F, panelBounds_.y + 87.0F);
            uiRenderer.drawLabel(renderer, "CULTURE NAME", panelBounds_.x + 32.0F, panelBounds_.y + 143.0F);
            uiRenderer.drawLabel(renderer, "MAP COLOR", panelBounds_.x + 32.0F, panelBounds_.y + 201.0F);
            polityNameField_.render(renderer, uiRenderer);
            cultureNameField_.render(renderer, uiRenderer);
            uiRenderer.drawColorSwatch(renderer, mapColorBounds_, renderColor(selectedMapColor_), mapColorHovered_, true);

            uiRenderer.drawLabel(renderer, "FLAG", panelBounds_.x + 32.0F, panelBounds_.y + 272.0F, 2.5F);
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
                    hoveredFlagCell_ == index
                        ? RenderColor{218, 218, 224, 255}
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
            uiRenderer.drawLabel(renderer, "FLAG COLOR", panelBounds_.x + 32.0F, panelBounds_.y + 519.0F, 2.5F);
            uiRenderer.drawColorSwatch(renderer, flagColorBounds_, renderColor(flag_.primaryColor), flagColorHovered_, true);

            uiRenderer.drawLabel(renderer, "CHOOSE A STARTING FORM", panelBounds_.x + 224.0F, panelBounds_.y + 272.0F, 2.5F, {216, 216, 220, 255});
            for (std::size_t index = 0; index < originCount; ++index)
            {
                uiRenderer.drawChoiceCard(
                    renderer,
                    originBounds_[index],
                    startingPolityOrigins[index].displayName,
                    hoveredOriginIndex_ == index,
                    pressedOriginIndex_ == index,
                    selectedOriginIndex_ == index
                );
            }

            if (!canContinueFromPolity())
            {
                uiRenderer.drawLabel(renderer, "ENTER BOTH NAMES AND CHOOSE A FORM", panelBounds_.x + 224.0F, panelBounds_.y + 565.0F, 2.0F, {190, 190, 196, 255});
            }
        }
        else
        {
            uiRenderer.drawLabel(
                renderer,
                mode_ == FoundingPanelMode::RenameCapital
                    ? "RENAME CAPITAL"
                    : "FOUND YOUR CAPITAL",
                panelBounds_.x + 32.0F,
                panelBounds_.y + 26.0F,
                4.0F
            );
            uiRenderer.drawLabel(renderer, "CAPITAL OF YOUR POLITY", panelBounds_.x + 32.0F, panelBounds_.y + 73.0F, 2.0F, {194, 194, 200, 255});
            uiRenderer.drawLabel(renderer, "CITY NAME", panelBounds_.x + 32.0F, panelBounds_.y + 123.0F);
            capitalNameField_.render(renderer, uiRenderer);
        }

        leftButton_.render(renderer, uiRenderer);
        rightButton_.render(renderer, uiRenderer);

        if (colorPickerTarget_ != ColorPickerTarget::None)
        {
            uiRenderer.drawModalBackdrop(renderer);
            uiRenderer.drawPanel(renderer, pickerBounds_);
            uiRenderer.drawLabel(renderer, "RGB COLOR", pickerBounds_.x + 28.0F, pickerBounds_.y + 24.0F, 3.5F);

            const MapColor color = pickerColor();
            const std::array<std::uint8_t, 3> values{color.red, color.green, color.blue};
            const std::array<std::string_view, 3> labels{"R", "G", "B"};
            const std::array<RenderColor, 3> fills{
                RenderColor{220, 64, 64, 255},
                RenderColor{64, 200, 92, 255},
                RenderColor{64, 112, 224, 255}
            };

            for (std::size_t channel = 0; channel < 3; ++channel)
            {
                const UiRectangle& bounds = pickerChannelBounds_[channel];
                uiRenderer.drawLabel(renderer, labels[channel], pickerBounds_.x + 36.0F, bounds.y + 4.0F, 2.5F);
                renderer.fillRectangle(bounds.x, bounds.y, bounds.width, bounds.height, {110, 110, 116, 255});
                renderer.fillRectangle(bounds.x + 2.0F, bounds.y + 2.0F, bounds.width - 4.0F, bounds.height - 4.0F, {38, 38, 42, 255});
                renderer.fillRectangle(
                    bounds.x + 2.0F,
                    bounds.y + 2.0F,
                    (bounds.width - 4.0F) * static_cast<float>(values[channel]) / 255.0F,
                    bounds.height - 4.0F,
                    fills[channel]
                );
                uiRenderer.drawLabel(renderer, std::to_string(values[channel]), bounds.x + bounds.width + 14.0F, bounds.y + 4.0F, 2.5F);
            }

            uiRenderer.drawColorSwatch(renderer, {pickerBounds_.x + 28.0F, pickerBounds_.y + 264.0F, 72.0F, 42.0F}, renderColor(color), false, true);
            pickerDoneButton_.render(renderer, uiRenderer);
        }
    }

    bool FoundingPanel::canContinueFromPolity() const noexcept
    {
        return isValidFoundingName(polityNameField_.text()) &&
            isValidFoundingName(cultureNameField_.text()) &&
            selectedOriginIndex_.has_value() &&
            flag_.isValid();
    }

    void FoundingPanel::showPolityStep()
    {
        step_ = FoundingPanelStep::Polity;
        polityNameField_.setFocused(true);
        cultureNameField_.setFocused(false);
        capitalNameField_.setFocused(false);
        leftButton_.setText("Cancel");
        rightButton_.setText(mode_ == FoundingPanelMode::Founding ? "Continue" : "Confirm");
        refreshButtonState();
    }

    void FoundingPanel::showCapitalStep()
    {
        step_ = FoundingPanelStep::Capital;
        polityNameField_.setFocused(false);
        cultureNameField_.setFocused(false);
        capitalNameField_.setFocused(true);
        leftButton_.setText(mode_ == FoundingPanelMode::Founding ? "Back" : "Cancel");
        rightButton_.setText("Confirm");
        refreshButtonState();
    }

    void FoundingPanel::refreshButtonState()
    {
        rightButton_.setEnabled(
            mode_ == FoundingPanelMode::Founding && step_ == FoundingPanelStep::Polity
                ? canContinueFromPolity()
                : canConfirm()
        );
    }

    void FoundingPanel::openColorPicker(ColorPickerTarget target) noexcept
    {
        colorPickerTarget_ = target;
        draggedColorChannel_.reset();
        polityNameField_.setFocused(false);
        cultureNameField_.setFocused(false);
    }

    void FoundingPanel::updateColorChannel(std::size_t channel, float pointerX) noexcept
    {
        const UiRectangle& bounds = pickerChannelBounds_[channel];
        const float ratio = std::clamp((pointerX - bounds.x) / bounds.width, 0.0F, 1.0F);
        const std::uint8_t value = static_cast<std::uint8_t>(std::lround(ratio * 255.0F));
        MapColor& color = pickerColor();
        if (channel == 0) color.red = value;
        else if (channel == 1) color.green = value;
        else color.blue = value;
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
}

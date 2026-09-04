#include "ui/FoundingPanel.h"

#include "rendering/Renderer.h"
#include "ui/GrayUiRenderer.h"

#include <algorithm>

namespace Paladin
{
    namespace
    {
        constexpr std::array<MapColor, 8> foundingColors{{
            {210, 54, 54},
            {220, 126, 42},
            {220, 192, 52},
            {54, 166, 84},
            {50, 146, 192},
            {68, 88, 210},
            {142, 72, 190},
            {190, 72, 142}
        }};

        RenderColor renderColor(MapColor color) noexcept
        {
            return {
                color.red,
                color.green,
                color.blue,
                255
            };
        }
    }

    FoundingPanel::FoundingPanel()
        : polityNameField_(
              "ENTER POLITY NAME",
              maximumFoundingNameLength
          ),
          cultureNameField_(
              "ENTER CULTURE NAME",
              maximumFoundingNameLength
          ),
          capitalNameField_(
              "ENTER CITY NAME",
              maximumFoundingNameLength
          ),
          leftButton_("Cancel"),
          rightButton_("Continue")
    {
        refreshButtonState();
    }

    void FoundingPanel::open()
    {
        open_ = true;
        polityNameField_.clear();
        cultureNameField_.clear();
        capitalNameField_.clear();
        selectedColorIndex_ = 0;
        selectedOriginIndex_.reset();
        hoveredColorIndex_.reset();
        hoveredOriginIndex_.reset();
        pressedOriginIndex_.reset();
        showPolityStep();
    }

    void FoundingPanel::close() noexcept
    {
        open_ = false;
        polityNameField_.setFocused(false);
        cultureNameField_.setFocused(false);
        capitalNameField_.setFocused(false);
        leftButton_.cancelPress();
        rightButton_.cancelPress();
        hoveredColorIndex_.reset();
        hoveredOriginIndex_.reset();
        pressedOriginIndex_.reset();
    }

    bool FoundingPanel::isOpen() const noexcept
    {
        return open_;
    }

    FoundingPanelStep FoundingPanel::step() const noexcept
    {
        return step_;
    }

    void FoundingPanel::layout(
        int viewportWidth,
        int viewportHeight
    ) noexcept
    {
        const float panelWidth =
            std::min(
                680.0F,
                static_cast<float>(viewportWidth) - 32.0F
            );

        const float panelHeight =
            step_ == FoundingPanelStep::Polity
                ? 620.0F
                : 300.0F;

        panelBounds_ = {
            (static_cast<float>(viewportWidth) - panelWidth) * 0.5F,
            (static_cast<float>(viewportHeight) - panelHeight) * 0.5F,
            panelWidth,
            panelHeight
        };

        const float fieldX = panelBounds_.x + 285.0F;
        const float fieldWidth = panelBounds_.width - 317.0F;

        polityNameField_.setBounds({
            fieldX,
            panelBounds_.y + 82.0F,
            fieldWidth,
            44.0F
        });

        cultureNameField_.setBounds({
            fieldX,
            panelBounds_.y + 142.0F,
            fieldWidth,
            44.0F
        });

        capitalNameField_.setBounds({
            fieldX,
            panelBounds_.y + 112.0F,
            fieldWidth,
            44.0F
        });

        constexpr float swatchSize = 38.0F;
        constexpr float swatchGap = 7.0F;

        for (std::size_t index = 0; index < colorCount; ++index)
        {
            colorBounds_[index] = {
                fieldX
                    + static_cast<float>(index)
                        * (swatchSize + swatchGap),
                panelBounds_.y + 210.0F,
                swatchSize,
                swatchSize
            };
        }

        constexpr float originGap = 28.0F;
        const float originAreaWidth = panelBounds_.width - 64.0F;
        const float originWidth =
            (
                originAreaWidth
                - originGap
                    * static_cast<float>(originCount - 1)
            )
            / static_cast<float>(originCount);

        for (std::size_t index = 0; index < originCount; ++index)
        {
            originBounds_[index] = {
                panelBounds_.x + 32.0F
                    + static_cast<float>(index)
                        * (originWidth + originGap),
                panelBounds_.y + 310.0F,
                originWidth,
                180.0F
            };
        }

        leftButton_.setBounds({
            panelBounds_.x + 32.0F,
            panelBounds_.y + panelBounds_.height - 68.0F,
            150.0F,
            44.0F
        });

        rightButton_.setBounds({
            panelBounds_.x + panelBounds_.width - 182.0F,
            panelBounds_.y + panelBounds_.height - 68.0F,
            150.0F,
            44.0F
        });
    }

    void FoundingPanel::pointerMoved(float x, float y) noexcept
    {
        leftButton_.pointerMoved(x, y);
        rightButton_.pointerMoved(x, y);
        hoveredColorIndex_.reset();
        hoveredOriginIndex_.reset();

        if (step_ != FoundingPanelStep::Polity)
        {
            return;
        }

        for (std::size_t index = 0; index < colorCount; ++index)
        {
            if (colorBounds_[index].contains(x, y))
            {
                hoveredColorIndex_ = index;
                break;
            }
        }

        for (std::size_t index = 0; index < originCount; ++index)
        {
            if (originBounds_[index].contains(x, y))
            {
                hoveredOriginIndex_ = index;
                break;
            }
        }
    }

    void FoundingPanel::pointerPressed(float x, float y) noexcept
    {
        if (step_ == FoundingPanelStep::Polity)
        {
            const bool polityFieldClicked =
                polityNameField_.contains(x, y);

            const bool cultureFieldClicked =
                cultureNameField_.contains(x, y);

            polityNameField_.setFocused(polityFieldClicked);
            cultureNameField_.setFocused(cultureFieldClicked);
            capitalNameField_.setFocused(false);

            for (std::size_t index = 0; index < colorCount; ++index)
            {
                if (colorBounds_[index].contains(x, y))
                {
                    selectedColorIndex_ = index;
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
            const bool capitalFieldClicked =
                capitalNameField_.contains(x, y);

            polityNameField_.setFocused(false);
            cultureNameField_.setFocused(false);
            capitalNameField_.setFocused(capitalFieldClicked);
        }

        static_cast<void>(leftButton_.pointerPressed(x, y));
        static_cast<void>(rightButton_.pointerPressed(x, y));
        refreshButtonState();
    }

    FoundingPanelAction FoundingPanel::pointerReleased(
        float x,
        float y
    )
    {
        pressedOriginIndex_.reset();

        const bool leftClicked =
            leftButton_.pointerReleased(x, y);

        const bool rightClicked =
            rightButton_.pointerReleased(x, y);

        if (leftClicked)
        {
            if (step_ == FoundingPanelStep::Polity)
            {
                return FoundingPanelAction::Cancel;
            }

            showPolityStep();
            return FoundingPanelAction::None;
        }

        if (rightClicked)
        {
            return submit();
        }

        return FoundingPanelAction::None;
    }

    FoundingPanelAction FoundingPanel::submit()
    {
        if (step_ == FoundingPanelStep::Polity)
        {
            if (canContinueFromPolity())
            {
                showCapitalStep();
            }

            return FoundingPanelAction::None;
        }

        return canConfirm()
            ? FoundingPanelAction::Confirm
            : FoundingPanelAction::None;
    }

    void FoundingPanel::appendText(std::string_view text)
    {
        if (polityNameField_.focused())
        {
            polityNameField_.appendText(text);
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
        if (polityNameField_.focused())
        {
            polityNameField_.backspace();
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

        const bool polityWasFocused = polityNameField_.focused();
        polityNameField_.setFocused(!polityWasFocused);
        cultureNameField_.setFocused(polityWasFocused);
    }

    bool FoundingPanel::canConfirm() const noexcept
    {
        return
            step_ == FoundingPanelStep::Capital &&
            canContinueFromPolity() &&
            isValidFoundingName(capitalNameField_.text());
    }

    FoundingIdentity FoundingPanel::identity() const
    {
        const std::string_view originId =
            selectedOriginIndex_
                ? startingPolityOrigins[*selectedOriginIndex_].id
                : std::string_view{};

        return {
            trimFoundingName(polityNameField_.text()),
            trimFoundingName(cultureNameField_.text()),
            trimFoundingName(capitalNameField_.text()),
            selectedColor(),
            std::string(originId)
        };
    }

    MapColor FoundingPanel::selectedColor() const noexcept
    {
        return foundingColors[selectedColorIndex_];
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

        if (step_ == FoundingPanelStep::Polity)
        {
            uiRenderer.drawLabel(
                renderer,
                "FOUND YOUR POLITY",
                panelBounds_.x + 32.0F,
                panelBounds_.y + 26.0F,
                4.0F
            );

            uiRenderer.drawLabel(
                renderer,
                "POLITY NAME",
                panelBounds_.x + 32.0F,
                panelBounds_.y + 93.0F
            );

            uiRenderer.drawLabel(
                renderer,
                "CULTURE NAME",
                panelBounds_.x + 32.0F,
                panelBounds_.y + 153.0F
            );

            uiRenderer.drawLabel(
                renderer,
                "MAP COLOR",
                panelBounds_.x + 32.0F,
                panelBounds_.y + 219.0F
            );

            polityNameField_.render(renderer, uiRenderer);
            cultureNameField_.render(renderer, uiRenderer);

            for (std::size_t index = 0; index < colorCount; ++index)
            {
                uiRenderer.drawColorSwatch(
                    renderer,
                    colorBounds_[index],
                    renderColor(foundingColors[index]),
                    hoveredColorIndex_ == index,
                    selectedColorIndex_ == index
                );
            }

            uiRenderer.drawLabel(
                renderer,
                "CHOOSE A STARTING FORM",
                panelBounds_.x + 32.0F,
                panelBounds_.y + 273.0F,
                2.5F,
                {216, 216, 220, 255}
            );

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
                uiRenderer.drawLabel(
                    renderer,
                    "ENTER BOTH NAMES AND CHOOSE A FORM",
                    panelBounds_.x + 32.0F,
                    panelBounds_.y + 510.0F,
                    2.0F,
                    {190, 190, 196, 255}
                );
            }
        }
        else
        {
            uiRenderer.drawLabel(
                renderer,
                "FOUND YOUR CAPITAL",
                panelBounds_.x + 32.0F,
                panelBounds_.y + 26.0F,
                4.0F
            );

            uiRenderer.drawLabel(
                renderer,
                "CAPITAL OF YOUR POLITY",
                panelBounds_.x + 32.0F,
                panelBounds_.y + 73.0F,
                2.0F,
                {194, 194, 200, 255}
            );

            uiRenderer.drawLabel(
                renderer,
                "CITY NAME",
                panelBounds_.x + 32.0F,
                panelBounds_.y + 123.0F
            );

            capitalNameField_.render(renderer, uiRenderer);

            if (!canConfirm())
            {
                uiRenderer.drawLabel(
                    renderer,
                    "ENTER A CITY NAME TO CONFIRM",
                    panelBounds_.x + 32.0F,
                    panelBounds_.y + 176.0F,
                    2.0F,
                    {190, 190, 196, 255}
                );
            }
        }

        leftButton_.render(renderer, uiRenderer);
        rightButton_.render(renderer, uiRenderer);
    }

    bool FoundingPanel::canContinueFromPolity() const noexcept
    {
        return
            isValidFoundingName(polityNameField_.text()) &&
            isValidFoundingName(cultureNameField_.text()) &&
            selectedOriginIndex_.has_value();
    }

    void FoundingPanel::showPolityStep()
    {
        step_ = FoundingPanelStep::Polity;
        polityNameField_.setFocused(true);
        cultureNameField_.setFocused(false);
        capitalNameField_.setFocused(false);
        leftButton_.setText("Cancel");
        rightButton_.setText("Continue");
        refreshButtonState();
    }

    void FoundingPanel::showCapitalStep()
    {
        step_ = FoundingPanelStep::Capital;
        polityNameField_.setFocused(false);
        cultureNameField_.setFocused(false);
        capitalNameField_.setFocused(true);
        leftButton_.setText("Back");
        rightButton_.setText("Confirm");
        hoveredColorIndex_.reset();
        hoveredOriginIndex_.reset();
        pressedOriginIndex_.reset();
        refreshButtonState();
    }

    void FoundingPanel::refreshButtonState()
    {
        rightButton_.setEnabled(
            step_ == FoundingPanelStep::Polity
                ? canContinueFromPolity()
                : canConfirm()
        );
    }
}

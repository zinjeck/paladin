#pragma once

#include "ui/UiButton.h"
#include "ui/UiTextField.h"
#include "world/FoundingIdentity.h"
#include "world/PolityOrigin.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

namespace Paladin
{
    class GrayUiRenderer;
    class Renderer;

    enum class FoundingPanelAction
    {
        None,
        Cancel,
        Confirm
    };

    enum class FoundingPanelStep
    {
        Polity,
        Capital
    };

    class FoundingPanel
    {
    public:
        FoundingPanel();

        void open();
        void close() noexcept;

        [[nodiscard]]
        bool isOpen() const noexcept;

        [[nodiscard]]
        FoundingPanelStep step() const noexcept;

        void layout(
            int viewportWidth,
            int viewportHeight
        ) noexcept;

        void pointerMoved(float x, float y) noexcept;
        void pointerPressed(float x, float y) noexcept;

        [[nodiscard]]
        FoundingPanelAction pointerReleased(
            float x,
            float y
        );

        [[nodiscard]]
        FoundingPanelAction submit();

        void appendText(std::string_view text);
        void backspace() noexcept;
        void focusNextField() noexcept;

        [[nodiscard]]
        bool canConfirm() const noexcept;

        [[nodiscard]]
        FoundingIdentity identity() const;

        [[nodiscard]]
        MapColor selectedColor() const noexcept;

        void render(
            Renderer& renderer,
            const GrayUiRenderer& uiRenderer
        );

    private:
        [[nodiscard]]
        bool canContinueFromPolity() const noexcept;

        void showPolityStep();
        void showCapitalStep();
        void refreshButtonState();

        static constexpr std::size_t colorCount = 8;
        static constexpr std::size_t originCount =
            startingPolityOrigins.size();

        UiTextField polityNameField_;
        UiTextField cultureNameField_;
        UiTextField capitalNameField_;
        UiButton leftButton_;
        UiButton rightButton_;

        UiRectangle panelBounds_;
        std::array<UiRectangle, colorCount> colorBounds_{};
        std::array<UiRectangle, originCount> originBounds_{};

        std::optional<std::size_t> hoveredColorIndex_;
        std::optional<std::size_t> hoveredOriginIndex_;
        std::optional<std::size_t> pressedOriginIndex_;
        std::optional<std::size_t> selectedOriginIndex_;
        std::size_t selectedColorIndex_ = 0;

        FoundingPanelStep step_ = FoundingPanelStep::Polity;
        bool open_ = false;
    };
}

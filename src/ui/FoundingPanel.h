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

    enum class FoundingPanelAction { None, Cancel, Confirm };
    enum class FoundingPanelStep { Polity, Capital };
    enum class FoundingPanelMode { Founding, RenameCapital, EditPolity };

    class FoundingPanel
    {
    public:
        FoundingPanel();

        void open();
        void openForCapitalRename(std::string_view currentName);
        void openForPolityEdit(const FoundingIdentity& identity);
        void close() noexcept;

        [[nodiscard]] bool isOpen() const noexcept;
        [[nodiscard]] FoundingPanelStep step() const noexcept;
        [[nodiscard]] FoundingPanelMode mode() const noexcept;

        void layout(int viewportWidth, int viewportHeight) noexcept;
        void pointerMoved(float x, float y) noexcept;
        void pointerPressed(float x, float y) noexcept;
        [[nodiscard]] FoundingPanelAction pointerReleased(float x, float y);
        [[nodiscard]] FoundingPanelAction submit();
        [[nodiscard]] bool closeTopLayer() noexcept;

        void appendText(std::string_view text);
        void backspace() noexcept;
        void focusNextField() noexcept;

        [[nodiscard]] bool canConfirm() const noexcept;
        [[nodiscard]] FoundingIdentity identity() const;
        [[nodiscard]] MapColor selectedColor() const noexcept;

        void render(Renderer& renderer, const GrayUiRenderer& uiRenderer);

    private:
        enum class ColorPickerTarget { None, Map, FlagPrimary };

        [[nodiscard]] bool canContinueFromPolity() const noexcept;
        void showPolityStep();
        void showCapitalStep();
        void refreshButtonState();
        void openColorPicker(ColorPickerTarget target) noexcept;
        void updateColorChannel(std::size_t channel, float pointerX) noexcept;
        void applyFlagStroke(std::size_t cellIndex) noexcept;
        [[nodiscard]] MapColor& pickerColor() noexcept;
        [[nodiscard]] const MapColor& pickerColor() const noexcept;

        static constexpr std::size_t originCount = startingPolityOrigins.size();
        static constexpr std::size_t flagCellCount =
            PolityFlag::defaultWidth * PolityFlag::defaultHeight;

        UiTextField polityNameField_;
        UiTextField cultureNameField_;
        UiTextField capitalNameField_;
        UiButton leftButton_;
        UiButton rightButton_;
        UiButton pickerDoneButton_;

        UiRectangle panelBounds_;
        UiRectangle mapColorBounds_;
        UiRectangle flagColorBounds_;
        std::array<UiRectangle, flagCellCount> flagCellBounds_{};
        std::array<UiRectangle, originCount> originBounds_{};
        UiRectangle pickerBounds_;
        std::array<UiRectangle, 3> pickerChannelBounds_{};

        std::optional<std::size_t> hoveredOriginIndex_;
        std::optional<std::size_t> pressedOriginIndex_;
        std::optional<std::size_t> selectedOriginIndex_;
        std::optional<std::size_t> draggedColorChannel_;
        std::optional<std::size_t> hoveredFlagCell_;

        MapColor selectedMapColor_{210, 54, 54};
        PolityFlag flag_;
        ColorPickerTarget colorPickerTarget_ = ColorPickerTarget::None;
        FoundingPanelStep step_ = FoundingPanelStep::Polity;
        FoundingPanelMode mode_ = FoundingPanelMode::Founding;
        bool mapColorHovered_ = false;
        bool flagColorHovered_ = false;
        bool flagStrokeActive_ = false;
        bool flagStrokePaints_ = true;
        bool open_ = false;
    };
}

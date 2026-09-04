#pragma once

#include "ui/UiButton.h"

#include <array>
#include <cstddef>

namespace Paladin
{
    class GrayUiRenderer;
    class Renderer;

    enum class SimulationSpeedControlAction
    {
        None,
        Pause,
        Normal,
        Double,
        Fast
    };

    class SimulationSpeedControls
    {
    public:
        static constexpr float ButtonSide = 48.0F;
        static constexpr float RowWidth = ButtonSide * 4;
        SimulationSpeedControls();

        void layout(int viewportWidth) noexcept;

        void setPlaybackState(
            bool paused,
            double speedMultiplier
        );

        void pointerMoved(float x, float y) noexcept;

        [[nodiscard]]
        bool pointerPressed(float x, float y) noexcept;

        [[nodiscard]]
        bool containsInteractivePoint(float x, float y) const noexcept;

        [[nodiscard]]
        SimulationSpeedControlAction pointerReleased(
            float x,
            float y
        ) noexcept;

        void render(
            Renderer& renderer,
            const GrayUiRenderer& uiRenderer
        ) const;

    private:
        static constexpr std::size_t ButtonCount = 4;

        std::array<UiButton, ButtonCount> buttons_;
        std::array<UiRectangle, ButtonCount> bounds_{};
    };
}

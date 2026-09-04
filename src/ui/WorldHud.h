#pragma once

#include "ui/UiButton.h"

namespace Paladin
{
    class GrayUiRenderer;
    class Renderer;

    enum class WorldHudAction
    {
        None,
        SelectRegion,
        Back
    };

    class WorldHud
    {
    public:
        WorldHud();

        void layout(
            int viewportWidth,
            int viewportHeight
        ) noexcept;

        void setRegionSelectionAvailable(
            bool available
        ) noexcept;
        void pointerMoved(float x, float y) noexcept;

        [[nodiscard]]
        bool pointerPressed(float x, float y) noexcept;

        [[nodiscard]]
        WorldHudAction pointerReleased(float x, float y) noexcept;

        void render(
            Renderer& renderer,
            const GrayUiRenderer& uiRenderer,
            bool regionSelectionActive
        );

    private:
        UiButton selectRegionButton_;
        UiButton backButton_;
        bool regionSelectionAvailable_ = true;
    };
}

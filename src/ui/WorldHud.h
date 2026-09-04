#pragma once

#include "ui/UiButton.h"

namespace Paladin
{
    class GrayUiRenderer;
    class Renderer;

    class WorldHud
    {
    public:
        WorldHud();

        void layout(int viewportWidth) noexcept;
        void pointerMoved(float x, float y) noexcept;

        [[nodiscard]]
        bool pointerPressed(float x, float y) noexcept;

        [[nodiscard]]
        bool pointerReleased(float x, float y) noexcept;

        void render(
            Renderer& renderer,
            const GrayUiRenderer& uiRenderer,
            bool regionSelectionActive
        );

    private:
        UiButton selectRegionButton_;
    };
}

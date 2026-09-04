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
        MoveCapital,
        RenameCapital,
        EditPolity,
        Play,
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

        void setCapitalEstablished(bool established) noexcept;
        void pointerMoved(float x, float y) noexcept;

        [[nodiscard]]
        bool pointerPressed(float x, float y) noexcept;

        [[nodiscard]]
        bool containsInteractivePoint(
            float x,
            float y
        ) const noexcept;

        [[nodiscard]]
        WorldHudAction pointerReleased(float x, float y) noexcept;

        void render(
            Renderer& renderer,
            const GrayUiRenderer& uiRenderer,
            bool regionSelectionActive
        );

    private:
        UiButton selectRegionButton_;
        UiButton moveCapitalButton_;
        UiButton renameCapitalButton_;
        UiButton editPolityButton_;
        UiButton playButton_;
        UiButton backButton_;
        bool capitalEstablished_ = false;
    };
}

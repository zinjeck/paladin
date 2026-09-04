#include "ui/WorldHud.h"

#include "ui/GrayUiRenderer.h"

namespace Paladin
{
    WorldHud::WorldHud()
        : selectRegionButton_("Select Region")
    {
    }

    void WorldHud::layout(int viewportWidth) noexcept
    {
        constexpr float width = 220.0F;
        constexpr float height = 44.0F;

        selectRegionButton_.setBounds({
            (
                static_cast<float>(viewportWidth)
                - width
            ) * 0.5F,
            16.0F,
            width,
            height
        });
    }

    void WorldHud::pointerMoved(float x, float y) noexcept
    {
        selectRegionButton_.pointerMoved(x, y);
    }

    bool WorldHud::pointerPressed(float x, float y) noexcept
    {
        return selectRegionButton_.pointerPressed(x, y);
    }

    bool WorldHud::pointerReleased(float x, float y) noexcept
    {
        return selectRegionButton_.pointerReleased(x, y);
    }

    void WorldHud::render(
        Renderer& renderer,
        const GrayUiRenderer& uiRenderer,
        bool regionSelectionActive
    )
    {
        selectRegionButton_.setSelected(regionSelectionActive);
        selectRegionButton_.render(renderer, uiRenderer);
    }
}

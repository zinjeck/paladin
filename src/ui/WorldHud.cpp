#include "ui/WorldHud.h"

#include "ui/GrayUiRenderer.h"

namespace Paladin
{
    WorldHud::WorldHud()
        : selectRegionButton_("Select Region"),
          backButton_("Back")
    {
    }

    void WorldHud::layout(
        int viewportWidth,
        int viewportHeight
    ) noexcept
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

        backButton_.setBounds({
            16.0F,
            static_cast<float>(viewportHeight) - height - 16.0F,
            140.0F,
            height
        });
    }

    void WorldHud::pointerMoved(float x, float y) noexcept
    {
        selectRegionButton_.pointerMoved(x, y);
        backButton_.pointerMoved(x, y);
    }

    void WorldHud::setRegionSelectionAvailable(
        bool available
    ) noexcept
    {
        regionSelectionAvailable_ = available;
        selectRegionButton_.setEnabled(available);
    }

    bool WorldHud::pointerPressed(float x, float y) noexcept
    {
        const bool selectRegionCaptured =
            selectRegionButton_.pointerPressed(x, y);

        const bool backCaptured =
            backButton_.pointerPressed(x, y);

        return selectRegionCaptured || backCaptured;
    }

    WorldHudAction WorldHud::pointerReleased(
        float x,
        float y
    ) noexcept
    {
        const bool selectRegionClicked =
            selectRegionButton_.pointerReleased(x, y);

        const bool backClicked =
            backButton_.pointerReleased(x, y);

        if (regionSelectionAvailable_ && selectRegionClicked)
        {
            return WorldHudAction::SelectRegion;
        }

        if (backClicked)
        {
            return WorldHudAction::Back;
        }

        return WorldHudAction::None;
    }

    void WorldHud::render(
        Renderer& renderer,
        const GrayUiRenderer& uiRenderer,
        bool regionSelectionActive
    )
    {
        if (regionSelectionAvailable_)
        {
            selectRegionButton_.setSelected(regionSelectionActive);
            selectRegionButton_.render(renderer, uiRenderer);
        }

        backButton_.render(renderer, uiRenderer);
    }
}

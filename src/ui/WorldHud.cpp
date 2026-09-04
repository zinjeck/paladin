#include "ui/WorldHud.h"

#include "ui/GrayUiRenderer.h"

namespace Paladin
{
    WorldHud::WorldHud()
        : selectRegionButton_("Select Region"),
          moveCapitalButton_("Move Capital"),
          renameCapitalButton_("Rename Capital"),
          editPolityButton_("Edit Polity"),
          playButton_("Play"),
          backButton_("Back")
    {
        playButton_.setEnabled(false);
    }

    void WorldHud::layout(
        int viewportWidth,
        int viewportHeight
    ) noexcept
    {
        constexpr float width = 190.0F;
        constexpr float height = 44.0F;
        constexpr float gap = 10.0F;

        selectRegionButton_.setBounds({
            (
                static_cast<float>(viewportWidth)
                - width
            ) * 0.5F,
            16.0F,
            width,
            height
        });

        const float actionRowWidth = width * 3.0F + gap * 2.0F;
        const float actionRowX =
            (static_cast<float>(viewportWidth) - actionRowWidth) * 0.5F;

        moveCapitalButton_.setBounds({actionRowX, 16.0F, width, height});
        renameCapitalButton_.setBounds({
            actionRowX + width + gap,
            16.0F,
            width,
            height
        });
        editPolityButton_.setBounds({
            actionRowX + (width + gap) * 2.0F,
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

        playButton_.setBounds({
            static_cast<float>(viewportWidth) - 140.0F - 16.0F,
            static_cast<float>(viewportHeight) - height - 16.0F,
            140.0F,
            height
        });
    }

    void WorldHud::pointerMoved(float x, float y) noexcept
    {
        selectRegionButton_.pointerMoved(x, y);
        moveCapitalButton_.pointerMoved(x, y);
        renameCapitalButton_.pointerMoved(x, y);
        editPolityButton_.pointerMoved(x, y);
        playButton_.pointerMoved(x, y);
        backButton_.pointerMoved(x, y);
    }

    void WorldHud::setCapitalEstablished(bool established) noexcept
    {
        capitalEstablished_ = established;
        playButton_.setEnabled(established);
    }

    bool WorldHud::pointerPressed(float x, float y) noexcept
    {
        bool actionCaptured = false;

        if (capitalEstablished_)
        {
            actionCaptured =
                moveCapitalButton_.pointerPressed(x, y) ||
                renameCapitalButton_.pointerPressed(x, y) ||
                editPolityButton_.pointerPressed(x, y);
        }
        else
        {
            actionCaptured = selectRegionButton_.pointerPressed(x, y);
        }

        const bool backCaptured =
            backButton_.pointerPressed(x, y);

        const bool playCaptured =
            playButton_.pointerPressed(x, y);

        return
            actionCaptured || playCaptured || backCaptured;
    }

    bool WorldHud::containsInteractivePoint(
        float x,
        float y
    ) const noexcept
    {
        return
            backButton_.containsPoint(x, y) ||
            playButton_.containsPoint(x, y) ||
            (
                !capitalEstablished_ &&
                selectRegionButton_.containsPoint(x, y)
            ) ||
            (
                capitalEstablished_ &&
                (
                    moveCapitalButton_.containsPoint(x, y) ||
                    renameCapitalButton_.containsPoint(x, y) ||
                    editPolityButton_.containsPoint(x, y)
                )
            );
    }

    WorldHudAction WorldHud::pointerReleased(
        float x,
        float y
    ) noexcept
    {
        const bool selectRegionClicked =
            selectRegionButton_.pointerReleased(x, y);

        const bool moveClicked =
            moveCapitalButton_.pointerReleased(x, y);

        const bool renameClicked =
            renameCapitalButton_.pointerReleased(x, y);

        const bool editClicked =
            editPolityButton_.pointerReleased(x, y);

        const bool backClicked =
            backButton_.pointerReleased(x, y);

        const bool playClicked =
            playButton_.pointerReleased(x, y);

        if (!capitalEstablished_ && selectRegionClicked)
        {
            return WorldHudAction::SelectRegion;
        }


        if (capitalEstablished_ && moveClicked)
        {
            return WorldHudAction::MoveCapital;
        }

        if (capitalEstablished_ && renameClicked)
        {
            return WorldHudAction::RenameCapital;
        }

        if (capitalEstablished_ && editClicked)
        {
            return WorldHudAction::EditPolity;
        }

        if (capitalEstablished_ && playClicked)
        {
            return WorldHudAction::Play;
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
        if (!capitalEstablished_)
        {
            selectRegionButton_.setSelected(regionSelectionActive);
            selectRegionButton_.render(renderer, uiRenderer);
        }
        else
        {
            moveCapitalButton_.setSelected(regionSelectionActive);
            moveCapitalButton_.render(renderer, uiRenderer);
            renameCapitalButton_.render(renderer, uiRenderer);
            editPolityButton_.render(renderer, uiRenderer);
        }

        // Before founding, Play remains fully transparent as well as disabled.
        if (capitalEstablished_)
        {
            playButton_.render(renderer, uiRenderer);
        }

        backButton_.render(renderer, uiRenderer);
    }
}

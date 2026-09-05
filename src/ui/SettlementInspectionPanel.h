#pragma once

#include "core/StrongId.h"
#include "ui/BitmapFontRenderer.h"
#include "ui/NormalFontRenderer.h"
#include "ui/UiButton.h"
#include "ui/UiTextField.h"
#include "ui/UiTypes.h"

namespace Paladin
{
class Camera2D;
class GrayUiRenderer;
class Renderer;
class SettlementInspectionController;
class SettlementCitizenState;
class SettlementMap;
struct SettlementObjectFootprint;
struct TileRenderMetrics;

class SettlementInspectionPanel
{
  public:
    void render(
        Renderer& renderer,
        GrayUiRenderer& grayUiRenderer,
        const SettlementInspectionController& controller,
        const SettlementMap& settlementMap,
        const SettlementCitizenState& citizenState,
        const Camera2D& camera,
        const TileRenderMetrics& metrics
    );

    [[nodiscard]]
    bool containsPoint(float x, float y) const noexcept;

    void clearLayout() noexcept;
    bool pointerPressed(float x, float y);
    void pointerReleased(
        float x,
        float y,
        SettlementMap&,
        SettlementCitizenState&,
        double minute
    );
    bool editingName() const noexcept
    {
        return nameField_.focused();
    }
    void appendText(std::string_view text)
    {
        nameField_.appendText(text);
    }
    void backspace() noexcept
    {
        nameField_.backspace();
    }
    void finishRename(SettlementMap&, bool commit);
    void pointerMoved(float x, float y);

  private:
    [[nodiscard]]
    UiRectangle anchoredBounds(
        const SettlementObjectFootprint& footprint,
        bool placeOnRight,
        float panelWidth,
        float panelHeight,
        const Renderer& renderer,
        const Camera2D& camera,
        const TileRenderMetrics& metrics
    ) const noexcept;

    bool dragCandidate_ = false, dragging_ = false, detached_ = false;
    float pressX_ = 0, pressY_ = 0, offsetX_ = 0, offsetY_ = 0;
    float positionX_ = 0, positionY_ = 0;
    float viewportWidth_ = 0, viewportHeight_ = 0;
    BitmapFontRenderer retroFontRenderer_;
    NormalFontRenderer normalFontRenderer_;
    UiRectangle renderedBounds_;
    bool hasRenderedBounds_ = false;
    WorkplaceId workplaceId_;
    UiButton nameButton_{""};
    UiButton decreaseButton_{"<"};
    UiButton increaseButton_{">"};
    UiTextField nameField_{"Workplace name", 40};
};
} // namespace Paladin

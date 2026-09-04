#pragma once

#include "ui/BitmapFontRenderer.h"
#include "ui/NormalFontRenderer.h"
#include "ui/UiTypes.h"

namespace Paladin
{
    class Camera2D;
    class GrayUiRenderer;
    class Renderer;
    class SettlementInspectionController;
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
            const Camera2D& camera,
            const TileRenderMetrics& metrics
        );

        [[nodiscard]]
        bool containsPoint(float x, float y) const noexcept;

        void clearLayout() noexcept;

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

        BitmapFontRenderer retroFontRenderer_;
        NormalFontRenderer normalFontRenderer_;
        UiRectangle renderedBounds_;
        bool hasRenderedBounds_ = false;
    };
}

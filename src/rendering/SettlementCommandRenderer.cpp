#include "rendering/SettlementCommandRenderer.h"

#include "interaction/SettlementCommandController.h"
#include "rendering/Camera2D.h"
#include "rendering/Renderer.h"
#include "rendering/TileRenderMetrics.h"
#include "world/settlements/commands/SettlementCommandState.h"

#include <array>
#include <optional>
#include <vector>

namespace Paladin
{
    void SettlementCommandRenderer::render(
        Renderer& renderer,
        const SettlementCommandState& state,
        const SettlementCommandController& controller,
        const Camera2D& camera,
        const TileRenderMetrics& metrics
    ) const
    {
        std::vector<TileOverlayRenderItem> overlays;
        std::vector<TileOutlineRenderItem> outlines;
        overlays.reserve(state.commands().size());
        outlines.reserve(state.commands().size());

        for (const SettlementCommand& command : state.commands())
        {
            for (const auto& target : command.targets)
            {
                if (!target.objectId && !target.constructionId) continue;
                const auto& footprint = target.footprint;
                overlays.push_back({
                    double(footprint.topLeft.x), double(footprint.topLeft.y),
                    double(footprint.width), double(footprint.height),
                    {255, 199, 31, 65}
                });
            }
        }

        overlayRenderer_.render(renderer, overlays, camera, metrics);
        overlayRenderer_.renderOutlines(renderer, outlines, camera, metrics);

        const std::optional<SettlementObjectFootprint> preview =
            controller.visibleFootprint();
        if (!preview)
        {
            return;
        }

        const RenderColor fill = controller.isCancelMode()
            ? RenderColor{255, 41, 41, 62}
            : RenderColor{0, 217, 255, 62};
        const RenderColor border = controller.isCancelMode()
            ? RenderColor{255, 77, 77, 242}
            : RenderColor{51, 242, 255, 242};
        const std::array<TileOverlayRenderItem, 1> previewOverlay{{{
            static_cast<double>(preview->topLeft.x),
            static_cast<double>(preview->topLeft.y),
            static_cast<double>(preview->width),
            static_cast<double>(preview->height),
            fill
        }}};
        const std::array<TileOutlineRenderItem, 1> previewOutline{{{
            static_cast<double>(preview->topLeft.x),
            static_cast<double>(preview->topLeft.y),
            static_cast<double>(preview->width),
            static_cast<double>(preview->height),
            1.5F,
            border
        }}};

        overlayRenderer_.render(renderer, previewOverlay, camera, metrics);
        overlayRenderer_.renderOutlines(
            renderer,
            previewOutline,
            camera,
            metrics
        );
    }
}

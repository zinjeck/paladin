#include "rendering/SettlementLogisticsRenderer.h"
#include "interaction/SettlementInspectionController.h"
#include "interaction/SettlementObjectPlacementController.h"
#include "rendering/Camera2D.h"
#include "rendering/Renderer.h"
#include "rendering/TileRenderMetrics.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include <algorithm>
#include <unordered_set>

namespace Paladin
{
void SettlementLogisticsRenderer::render(
    Renderer& renderer,
    const SettlementMap& map,
    const Camera2D& camera,
    const TileRenderMetrics& metrics,
    const SettlementObjectPlacementController& placement,
    const SettlementInspectionController& inspection
) const
{
    const float size = float(metrics.scaledTilePixels(camera.zoom()));
    const float ox =
        renderer.outputWidth() * .5F - float(camera.tileX()) * size;
    const float oy =
        renderer.outputHeight() * .5F - float(camera.tileY()) * size;
    const auto visible = [&](SettlementTilePosition p)
    {
        return ox + (p.x + 1) * size >= 0 && oy + (p.y + 1) * size >= 0 &&
               ox + p.x * size <= renderer.outputWidth() &&
               oy + p.y * size <= renderer.outputHeight();
    };
    for (const auto& inventory : map.logistics.inventories())
    {
        if (inventory.kind != InventoryKind::Groundpile ||
            inventory.used() <= 0 || !visible(inventory.footprint.topLeft))
        {
            continue;
        }
        const auto p = inventory.footprint.topLeft;
        const float x = ox + p.x * size, y = oy + p.y * size;
        int stack = 0;
        for (const auto& goods : inventory.goods)
        {
            if (goods.amount <= 0)
            {
                continue;
            }
            const RenderColor fill =
                goods.resource == "lumber" ? RenderColor{164, 111, 57, 255}
                : goods.resource == "fish" ? RenderColor{85, 172, 215, 255}
                                           : RenderColor{165, 169, 178, 255};
            const float inset = .15F + .08F * stack++;
            renderer.fillRectangle(
                x + size * inset,
                y + size * inset,
                size * .65F,
                size * .55F,
                {45, 40, 36, 255}
            );
            renderer.fillRectangle(
                x + size * (inset + .06F),
                y + size * (inset + .06F),
                size * .53F,
                size * .43F,
                fill
            );
            renderer.fillRectangle(
                x + size * (inset + .06F),
                y + size * (inset + .25F),
                size * .53F,
                std::max(1.0F, size * .05F),
                {65, 55, 44, 255}
            );
        }
    }
    const auto* definition = placement.activeDefinition();
    const auto footprint = placement.visibleFootprint();
    if (definition && definition->id == SettlementObjectTypes::FishingGrounds &&
        footprint)
    {
        if (mapId_ != map.instanceId() ||
            version_ != map.objectState().presentationVersion() ||
            footprint_ != footprint)
        {
            preview_ =
                fisheryZonePreview(map.grid(), map.objectState(), *footprint);
            mapId_ = map.instanceId();
            version_ = map.objectState().presentationVersion();
            footprint_ = footprint;
        }
        const auto& f = preview_.bounds;
        const float x = ox + f.topLeft.x * size, y = oy + f.topLeft.y * size;
        renderer.fillRectangle(
            x,
            y,
            f.width * size,
            f.height * size,
            {80, 190, 205, 24}
        );
        const auto water = [&](const auto& tiles, RenderColor color)
        {
            std::vector<RenderRectangle> rectangles;
            for (auto p : tiles)
            {
                if (visible(p))
                {
                    rectangles.push_back(
                        {ox + p.x * size, oy + p.y * size, size, size}
                    );
                }
            }
            renderer.fillRectangles(rectangles, color);
        };
        water(preview_.availableWater, {68, 218, 180, 115});
        water(preview_.excludedWater, {242, 65, 65, 175});
        renderer.drawLine(x, y, x + f.width * size, y, {110, 232, 219, 255});
        renderer.drawLine(x, y, x, y + f.height * size, {110, 232, 219, 255});
        renderer.drawLine(
            x + f.width * size,
            y,
            x + f.width * size,
            y + f.height * size,
            {110, 232, 219, 255}
        );
        renderer.drawLine(
            x,
            y + f.height * size,
            x + f.width * size,
            y + f.height * size,
            {110, 232, 219, 255}
        );
    }
    const auto* object = inspection.selectedObject(map.objectState());
    if (!object ||
        object->objectTypeId != SettlementObjectTypes::FishingGrounds)
    {
        return;
    }
    const auto key = [&](SettlementTilePosition p)
    { return (std::uint64_t(std::uint32_t(p.x)) << 32) | std::uint32_t(p.y); };
    std::unordered_set<std::uint64_t> water;
    for (auto p : object->productionWater)
    {
        water.insert(key(p));
    }
    for (auto p : object->productionWater)
    {
        if (!visible(p))
        {
            continue;
        }
        const float x = ox + p.x * size, y = oy + p.y * size;
        const RenderColor border{102, 235, 212, 255};
        if (!water.contains(key({p.x, p.y - 1})))
        {
            renderer.fillRectangle(x, y, size, 2, border);
        }
        if (!water.contains(key({p.x, p.y + 1})))
        {
            renderer.fillRectangle(x, y + size - 2, size, 2, border);
        }
        if (!water.contains(key({p.x - 1, p.y})))
        {
            renderer.fillRectangle(x, y, 2, size, border);
        }
        if (!water.contains(key({p.x + 1, p.y})))
        {
            renderer.fillRectangle(x + size - 2, y, 2, size, border);
        }
    }
}
} // namespace Paladin

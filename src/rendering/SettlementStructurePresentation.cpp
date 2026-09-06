#include "rendering/SettlementStructurePresentation.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include <cmath>

namespace Paladin
{
    void SettlementStructurePresentation::submit(
        SceneDrawQueue& queue,
        const SceneProjection& projection,
        const SettlementMap& map,
        const CityPresentation& policy,
        const SceneSpriteLibrary& sprites
    ) const
    {
        const auto& state = map.objectState();
        if (instance_ != map.instanceId() ||
            version_ != state.presentationVersion())
        {
            instance_ = map.instanceId();
            version_ = state.presentationVersion();
            buildings_.clear();
            for (const auto& object : state.completedObjects())
            {
                if (CityPresentation::enclosed(object.objectTypeId))
                {
                    buildings_.push_back(object);
                }
            }
        }
        for (const auto& object : buildings_)
        {
            const auto& f = object.footprint;
            const double x = f.topLeft.x, y = f.topLeft.y, w = f.width,
                         h = f.height;
            const double height = policy.wallHeight,
                         thickness = policy.wallThickness;
            const auto visible = projection.bounds(
                {x, y - height, 0, w + .4, h + height + .4, 0, 0}
            );
            if (!projection.visible(visible))
            {
                continue;
            }
            const auto id = (object.id.value() << 3) | 2;
            const auto& visual =
                SettlementObjectCatalog::definition(object.objectTypeId)
                    ->visual;
            const RenderColor roof{
                visual.fillColor[0],
                visual.fillColor[1],
                visual.fillColor[2],
                255
            };
            const RenderColor wall{
                visual.frameColor[0],
                visual.frameColor[1],
                visual.frameColor[2],
                255
            };
            if (policy.shadowsVisible)
            {
                // Cast beyond the footprint, not across the entire interior.
                queue.submit(
                    {projection.bounds({x + w, y + .2, 0, .2, h, 0, 0}),
                     policy.shadowColor,
                     y,
                     id,
                     -1}
                );
                queue.submit(
                    {projection.bounds({x + .2, y + h, 0, w - .2, .2, 0, 0}),
                     policy.shadowColor,
                     y,
                     id,
                     -1}
                );
            }
            // Floor is a ground surface, visible through the roof toggle. It
            // retains the existing footprint palette until the artist replaces
            // it.
            const auto floorStart = queue.size();
            sprites.surface(
                queue,
                projection,
                object.objectTypeId + ".floor",
                {x, y, 0, w, h, 0, 0},
                roof,
                y,
                id,
                0
            );
            queue.setLayerFrom(floorStart, -2);

            const auto strip = [&](double left,
                                   double top,
                                   double width,
                                   double depthSize,
                                   double depth,
                                   int part)
            {
                sprites.surface(
                    queue,
                    projection,
                    object.objectTypeId + ".wall",
                    {left, top, 0, width, depthSize, 0, 0},
                    wall,
                    depth,
                    id,
                    part
                );
            };
            // North and south walls have real door gaps. Side walls are split
            // into visible row segments so people can pass either side
            // correctly.
            for (int edge = 0; edge < 2; ++edge)
            {
                const double row = edge ? y + h - 1 : y;
                const double base = edge ? y + h : y + thickness;
                const double depth = edge ? y + h : y;
                const bool door = object.door && object.door->y == row;
                const double cut = door ? object.door->x - x : w;
                if (cut > 0)
                {
                    strip(x, base - height, cut, height, depth, edge ? 12 : 1);
                }
                if (door && cut + 1 < w)
                {
                    strip(
                        x + cut + 1,
                        base - height,
                        w - cut - 1,
                        height,
                        depth,
                        edge ? 12 : 1
                    );
                }
            }
            if (projection.tilePixels >= policy.detailTilePixels)
            {
                const int first = std::max(
                    0,
                    int(std::floor(
                        projection.cameraY -
                        projection.screenHeight * .5 / projection.tilePixels - y
                    )) - 1
                );
                const int last = std::min(
                    f.height,
                    int(std::ceil(
                        projection.cameraY +
                        projection.screenHeight * .5 / projection.tilePixels -
                        y + height
                    )) + 1
                );
                for (int row = first; row < last; ++row)
                {
                    for (int edge = 0; edge < 2; ++edge)
                    {
                        const double col = edge ? x + w - 1 : x;
                        if (object.door && object.door->x == col &&
                            object.door->y == y + row)
                        {
                            continue;
                        }
                        strip(
                            edge ? x + w - thickness : x,
                            y + row - height,
                            thickness,
                            1,
                            y + row + 1,
                            2
                        );
                    }
                }
            }
            if (policy.roofsVisible)
            {
                sprites.surface(
                    queue,
                    projection,
                    object.objectTypeId + ".roof",
                    {x, y, height, w, h, 0, 0},
                    roof,
                    y + h - .001,
                    id,
                    10
                );
            }
        }
    }
} // namespace Paladin

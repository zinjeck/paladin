#include "rendering/SettlementStructurePresentation.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/objects/SettlementObjectDefinition.h"
#include <cmath>
#include <unordered_set>

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
        constexpr int chunkSide = 32;
        const auto key = [](int x, int y)
        { return (std::uint64_t(std::uint32_t(x)) << 32) | std::uint32_t(y); };
        if (instance_ != map.instanceId() ||
            version_ != state.presentationVersion())
        {
            instance_ = map.instanceId();
            version_ = state.presentationVersion();
            chunks_.clear();
            large_.clear();
            for (const auto& object : state.completedObjects())
            {
                const auto& f = object.footprint;
                const int x0 = f.topLeft.x / chunkSide,
                          y0 = f.topLeft.y / chunkSide;
                const int x1 = (f.topLeft.x + f.width - 1) / chunkSide;
                const int y1 = (f.topLeft.y + f.height - 1) / chunkSide;
                if (std::int64_t(x1 - x0 + 1) * (y1 - y0 + 1) > 64)
                {
                    large_.push_back(object.id);
                    continue;
                }
                for (int y = y0; y <= y1; ++y)
                {
                    for (int x = x0; x <= x1; ++x)
                    {
                        chunks_[key(x, y)].push_back(object.id);
                    }
                }
            }
        }
        // Artist exports may overhang 64 tiles. Query chunks, not every
        // object/tile.
        const double halfW =
            projection.screenWidth * .5 / projection.tilePixels;
        const double halfH =
            projection.screenHeight * .5 / projection.tilePixels;
        const int x0 = std::max(
            0,
            int(std::floor((projection.cameraX - halfW - 80) / chunkSide))
        );
        const int y0 = std::max(
            0,
            int(std::floor((projection.cameraY - halfH - 80) / chunkSide))
        );
        const int x1 = std::min(
            map.grid().width() / chunkSide,
            int(std::floor((projection.cameraX + halfW + 80) / chunkSide))
        );
        const int y1 = std::min(
            map.grid().height() / chunkSide,
            int(std::floor((projection.cameraY + halfH + 80) / chunkSide))
        );
        std::unordered_set<SettlementObjectId, StrongIdHash> seen;
        std::vector<SettlementObjectId> candidates = large_;
        for (int y = y0; y <= y1; ++y)
        {
            for (int x = x0; x <= x1; ++x)
            {
                if (const auto it = chunks_.find(key(x, y));
                    it != chunks_.end())
                {
                    for (const auto id : it->second)
                    {
                        if (seen.insert(id).second)
                        {
                            candidates.push_back(id);
                        }
                    }
                }
            }
        }
        for (const auto objectId : candidates)
        {
            const auto* pointer = state.completedObject(objectId);
            if (!pointer)
            {
                continue;
            }
            const auto& object = *pointer;
            const auto& style = sprites.objectStyle(object.objectTypeId);
            const bool placeholder = !sprites.objectHasArt(object.objectTypeId);
            const auto& f = object.footprint;
            const double x = f.topLeft.x, y = f.topLeft.y, w = f.width,
                         h = f.height;
            const double height = style.height, thickness = style.thickness;
            const auto visible = projection.bounds(
                {x - 64, y - height - 64, 0, w + 128, h + height + 128, 0, 0}
            );
            if (!projection.visible(visible))
            {
                continue;
            }
            const auto id = (object.id.value() << 3) | 2;
            const auto rgb = [](std::uint32_t color)
            {
                return RenderColor{
                    std::uint8_t(color >> 16),
                    std::uint8_t(color >> 8),
                    std::uint8_t(color),
                    255
                };
            };
            const auto roof = rgb(style.fillRgb), wall = rgb(style.frameRgb);
            if (policy.shadowsVisible && style.mode == "enclosed" &&
                placeholder)
            {
                const double cast = height * .45;
                // Cast beyond the footprint, not across the entire interior.
                queue.submit(
                    {projection.bounds({x + w, y + cast, 0, cast, h, 0, 0}),
                     {12, 16, 24, std::uint8_t(style.shadowAlpha)},
                     y,
                     id,
                     -1}
                );
                queue.submit(
                    {projection.bounds(
                         {x + cast, y + h, 0, w - cast, cast, 0, 0}
                     ),
                     {12, 16, 24, std::uint8_t(style.shadowAlpha)},
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
                style.floor,
                {x, y, 0, w, h, 0, 0},
                roof,
                y,
                id,
                0,
                placeholder
            );
            queue.setLayerFrom(floorStart, -2);
            if (style.mode != "enclosed")
            {
                if (style.mode == "ground")
                {
                    continue;
                }
                const auto overview = [&]()
                {
                    if (!placeholder)
                    {
                        sprites.submit(
                            queue,
                            projection,
                            style.sprite,
                            x + w * .5,
                            y + h * .5,
                            id
                        );
                    }
                };
                if (projection.tilePixels < policy.detailTilePixels)
                {
                    overview();
                    continue;
                }
                const bool single = style.mode == "single";
                const double stepX = single ? w : style.moduleWidth;
                const double stepY = single ? h : style.moduleDepth;
                const int columns = std::max(1, int(std::floor(w / stepX)));
                const int rows = std::max(1, int(std::floor(h / stepY)));
                const int firstX = std::clamp(
                    int(std::floor(
                        (projection.cameraX - halfW - 64 - x) / stepX
                    )),
                    0,
                    columns
                );
                const int lastX = std::clamp(
                    int(
                        std::ceil((projection.cameraX + halfW + 64 - x) / stepX)
                    ),
                    0,
                    columns
                );
                const int firstY = std::clamp(
                    int(std::floor(
                        (projection.cameraY - halfH - 64 - y) / stepY
                    )),
                    0,
                    rows
                );
                const int lastY = std::clamp(
                    int(
                        std::ceil((projection.cameraY + halfH + 64 - y) / stepY)
                    ),
                    0,
                    rows
                );
                if (std::int64_t(lastX - firstX) * (lastY - firstY) > 2048)
                {
                    overview();
                    continue; // Coarse ground surface instead of unbounded
                              // props.
                }
                std::size_t submitted = 0;
                for (int row = firstY;
                     row < lastY && submitted < 2048 && queue.size() < 32768;
                     ++row)
                {
                    for (int col = firstX; col < lastX && submitted < 2048 &&
                                           queue.size() < 32768;
                         ++col)
                    {
                        const double cellW = std::min(stepX, w),
                                     cellH = std::min(stepY, h);
                        const double cx = x + col * stepX + cellW * .5;
                        const double cy = y + row * stepY + cellH * .75;
                        if (sprites.submit(
                                queue,
                                projection,
                                style.sprite,
                                cx,
                                cy,
                                id
                            ))
                        {
                            ++submitted;
                            continue;
                        }
                        if (!placeholder)
                        {
                            continue;
                        }
                        // Generic volume proxy, not object-specific artwork.
                        const double bw = cellW * style.bodyWidth,
                                     bd = cellH * style.bodyDepth;
                        const auto top =
                            projection.bounds({cx, cy, height, bw, bd, .5, 1});
                        const auto face =
                            projection.bounds({cx, cy, 0, bw, height, .5, 1});
                        if (!projection.visible(top) &&
                            !projection.visible(face))
                        {
                            continue;
                        }
                        ++submitted;
                        if (policy.shadowsVisible)
                        {
                            queue.submit(
                                {projection.bounds(
                                     {cx + height * .35,
                                      cy + height * .35,
                                      0,
                                      bw,
                                      bd,
                                      .5,
                                      1}
                                 ),
                                 {12, 16, 24, std::uint8_t(style.shadowAlpha)},
                                 cy,
                                 id,
                                 -1}
                            );
                        }
                        queue.submit({face, wall, cy, id, 0, 0});
                        queue.submit(
                            {face,
                             {0, 0, 0, std::uint8_t(style.frontShade)},
                             cy,
                             id,
                             0,
                             1}
                        );
                        queue.submit({top, roof, cy, id, 0, 2});
                        queue.submit(
                            {projection.bounds(
                                 {cx + bw * .5 - thickness,
                                  cy,
                                  height,
                                  thickness,
                                  bd,
                                  0,
                                  1}
                             ),
                             {0, 0, 0, std::uint8_t(style.sideShade)},
                             cy,
                             id,
                             0,
                             3}
                        );
                    }
                }
                continue;
            }

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
                    style.wall,
                    {left, top, 0, width, depthSize, 0, 0},
                    wall,
                    depth,
                    id,
                    part,
                    placeholder
                );
                // Light the existing surface, including supplied textures.
                // One bounded overlay per visible wall segment, no tile scan.
                const auto bounds =
                    projection.bounds({left, top, 0, width, depthSize, 0, 0});
                if (projection.visible(bounds) && placeholder)
                {
                    queue.submit(
                        {bounds,
                         {0,
                          0,
                          0,
                          std::uint8_t(
                              part == 2 ? style.sideShade : style.frontShade
                          )},
                         depth,
                         id,
                         0,
                         part}
                    );
                    queue.submit(
                        {projection.bounds(
                             {left, top, 0, width, thickness * .35, 0, 0}
                         ),
                         {255, 255, 255, std::uint8_t(style.edgeLight)},
                         depth,
                         id,
                         0,
                         part}
                    );
                }
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
            else
            {
                // Preserve the silhouette at low zoom without row subdivision.
                strip(x, y - height, thickness, h, y + h, 2);
                strip(x + w - thickness, y - height, thickness, h, y + h, 2);
            }
            if (policy.roofsVisible)
            {
                sprites.surface(
                    queue,
                    projection,
                    style.roof,
                    {x, y, height, w, h, 0, 0},
                    roof,
                    y + h - .001,
                    id,
                    10,
                    placeholder
                );
                if (!placeholder)
                {
                    continue;
                }
                const double rim = thickness * .5;
                queue.submit(
                    {projection.bounds({x, y, height, w, rim, 0, 0}),
                     {255, 255, 255, std::uint8_t(style.edgeLight)},
                     y + h - .001,
                     id,
                     0,
                     11}
                );
                queue.submit(
                    {projection.bounds({x + w - rim, y, height, rim, h, 0, 0}),
                     {0, 0, 0, std::uint8_t(style.sideShade)},
                     y + h - .001,
                     id,
                     0,
                     11}
                );
                queue.submit(
                    {projection.bounds({x, y + h - rim, height, w, rim, 0, 0}),
                     {0, 0, 0, std::uint8_t(style.frontShade)},
                     y + h - .001,
                     id,
                     0,
                     11}
                );
            }
        }
    }
} // namespace Paladin

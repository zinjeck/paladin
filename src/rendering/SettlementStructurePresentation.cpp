#include "rendering/SettlementStructurePresentation.h"
#include "rendering/BuildingView.h"
#include "rendering/HomePresentation.h"
#include "rendering/PasturePresentation.h"
#include "rendering/SettlementEnvironmentDetails.h"
#include "rendering/StockpilePresentation.h"
#include "world/settlements/SettlementMap.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
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
        const SceneSpriteLibrary& sprites,
        const SettlementCitizenState* citizens,
        Renderer* renderer
    ) const
    {
        const auto& state = map.objectState();
        ++commandFrame_;
        std::unordered_map<SettlementObjectId, unsigned, StrongIdHash>
            doubleRows;
        if (citizens && !policy.roofsVisible)
        {
            for (const auto& c : citizens->citizens())
            {
                if (c.health > 0 && c.doubleBed && c.bedSlot >= 0 &&
                    c.bedHomeId)
                {
                    doubleRows[c.bedHomeId] |= 1u << (c.bedSlot / 2);
                }
            }
        }
        const bool animate = projection.tilePixels >= AnimationDetailPixels;
        const bool detailed = projection.tilePixels >= StaticDetailPixels;
        if (renderer)
        {
            groundCache_.begin(map, sprites);
        }
        constexpr int chunkSide = 32;
        const auto key = [](int x, int y)
        { return (std::uint64_t(std::uint32_t(x)) << 32) | std::uint32_t(y); };
        const double now = sprites.time();
        const double dt =
            lastDoorTime_ < 0 ? 0 : std::clamp(now - lastDoorTime_, 0., .25);
        lastDoorTime_ = now;
        if (instance_ != map.instanceId())
        {
            doors_.clear();
            buildingCommands_.clear();
        }
        std::unordered_set<std::uint64_t> doorTraffic;
        if (citizens && animate)
        {
            for (const auto& c : citizens->citizens())
            {
                if (c.health <= 0 || (c.insideHome && c.path.empty()))
                {
                    continue;
                }
                doorTraffic.insert(key(c.tilePosition.x, c.tilePosition.y));
                doorTraffic.insert(
                    key(int(std::floor(c.visualX())),
                        int(std::floor(c.visualY())))
                );
                if (c.pathIndex < c.path.size())
                {
                    doorTraffic.insert(
                        key(c.path[c.pathIndex].x, c.path[c.pathIndex].y)
                    );
                }
            }
        }
        if (instance_ != map.instanceId() ||
            version_ != state.navigationVersion())
        {
            instance_ = map.instanceId();
            version_ = state.navigationVersion();
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
            const auto& footprint = object.footprint;
            const auto& recipe = sprites.objectStyle(object.objectTypeId);
            bool animatedArt = false;
            double marginX = 1, marginY = std::max(1., recipe.height);
            for (const auto& name :
                 {object.objectTypeId + ".roof.full",
                  recipe.roof + ".full",
                  recipe.roof + ".full.side",
                  recipe.sprite,
                  recipe.wall})
            {
                if (const auto* s = sprites.find(name))
                {
                    animatedArt |= s->frames > 1;
                    marginX = std::max(marginX, s->width);
                    marginY = std::max(marginY, s->height + s->elevation);
                }
            }
            for (const auto& piece : sprites.pieces())
            {
                if (piece.object == object.objectTypeId ||
                    piece.object == recipe.decor)
                {
                    if (const auto* s = sprites.find(piece.sprite))
                    {
                        animatedArt |= s->frames > 1;
                        marginX =
                            std::max(marginX, std::abs(piece.x) + s->width);
                        marginY = std::max(
                            marginY,
                            std::abs(piece.y) + s->height + s->elevation
                        );
                    }
                }
            }
            // Real artwork extents, not the former 64-tile padding per object.
            if (!projection.visible(projection.bounds(
                    {footprint.topLeft.x - marginX,
                     footprint.topLeft.y - marginY,
                     0,
                     footprint.width + 2 * marginX,
                     footprint.height + 2 * marginY,
                     0,
                     0}
                )))
            {
                continue;
            }
            double doorOpen = 0;
            if (object.door && citizens && animate)
            {
                auto& door = doors_[object.id.value()];
                if (doorTraffic.contains(key(object.door->x, object.door->y)))
                {
                    door.until = now + .7;
                }
                const double target = now < door.until ? 1 : 0;
                if (target > door.openness)
                {
                    door.openness = std::min(target, door.openness + 4.5 * dt);
                }
                else if (target < door.openness)
                {
                    door.openness = std::max(target, door.openness - 2.5 * dt);
                }
                doorOpen = door.openness;
            }
            const auto& style = sprites.objectStyle(object.objectTypeId);
            const bool placeholder = !sprites.objectHasArt(object.objectTypeId);
            const auto& f = object.footprint;
            const double x = f.topLeft.x, y = f.topLeft.y, w = f.width,
                         h = f.height;
            const double thickness = style.thickness;
            // Roof-off is a cutaway, not just removal of the roof plane.
            // Retain a low perimeter without hiding occupants on the front row.
            const double height =
                style.mode == "enclosed" && !policy.roofsVisible
                    ? std::min(style.height, thickness)
                    : style.height;
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
            if (object.objectTypeId == SettlementObjectTypes::Road &&
                sprites.find("road.floor"))
            {
                if (!groundCache_.submit(
                        renderer,
                        queue,
                        projection,
                        map,
                        sprites,
                        object,
                        id
                    ))
                {
                    if (detailed)
                    {
                        naturalRoad(
                            queue,
                            projection,
                            map,
                            sprites,
                            object,
                            id
                        );
                    }
                    else
                    {
                        const auto* art = sprites.find("road.floor");
                        queue.submit(
                            {projection.bounds({x, y, 0, w, h, 0, 0}),
                             {},
                             y,
                             id,
                             -2,
                             0,
                             art->texture.get(),
                             sprites.frame(*art, false)}
                        );
                    }
                }
                continue;
            }
            if (object.objectTypeId == SettlementObjectTypes::Stockpile &&
                stockpilePresentation(
                    queue,
                    projection,
                    sprites,
                    map,
                    object,
                    id
                ))
            {
                continue;
            }
            if (object.objectTypeId == SettlementObjectTypes::LoggingGrounds &&
                !placeholder)
            {
                loggingDetails(
                    queue,
                    projection,
                    sprites,
                    object,
                    citizens,
                    id,
                    policy.shadowsVisible
                );
                continue;
            }
            if (style.mode == "enclosed" && detailed)
            {
                if (!groundCache_.submit(
                        renderer,
                        queue,
                        projection,
                        map,
                        sprites,
                        object,
                        id
                    ))
                {
                    buildingGround(queue, projection, sprites, map, f, id);
                }
            }
            if (object.objectTypeId == SettlementObjectTypes::House &&
                policy.roofsVisible && animate)
            {
                homeChimney(queue, projection, sprites, map, object, id);
            }
            if (const auto* soil = sprites.find("road.floor");
                detailed && soil && style.mode != "enclosed" &&
                (!sprites.find(style.floor) || !sprites.find("terrain.plain") ||
                 sprites.find(style.floor)->texture !=
                     sprites.find("terrain.plain")->texture))
            {
                // A shallow, irregular apron ties placed surfaces into their
                // surroundings. It lies under floors and ground shadows.
                const auto frame = sprites.frame(*soil, false);
                const double apron = style.mode == "ground" ? .20 : .38;
                for (int edge = 0; edge < 4; ++edge)
                {
                    const double length = edge % 2 ? h : w;
                    for (double along = 0; along < length; along += .25)
                    {
                        const std::uint64_t grain =
                            (id * 73856093ULL) ^
                            std::uint64_t(along * 4 + edge * 193);
                        const double spread = apron * (.45 + .18 * (grain % 4));
                        const double px = edge == 1   ? x + w
                                          : edge == 3 ? x - spread
                                                      : x + along;
                        const double py = edge == 2   ? y + h
                                          : edge == 0 ? y - spread
                                                      : y + along;
                        const double pw =
                            edge % 2 ? spread : std::min(.25, length - along);
                        const double ph =
                            edge % 2 ? std::min(.25, length - along) : spread;
                        const auto* ground = map.grid().tile(
                            {int(std::floor(px + pw * .5)),
                             int(std::floor(py + ph * .5))}
                        );
                        if (!ground || ground->terrain != TerrainType::Land)
                        {
                            continue;
                        }
                        const auto b =
                            projection.bounds({px, py, 0, pw, ph, 0, 0});
                        if (projection.visible(b))
                        {
                            queue.submit(
                                {b,
                                 {},
                                 y,
                                 id,
                                 -3,
                                 0,
                                 soil->texture.get(),
                                 {frame.x +
                                      float((grain % 4) * frame.width / 4),
                                  0,
                                  frame.width / 4,
                                  frame.height / 4},
                                 std::uint8_t(55 + grain % 40)}
                            );
                        }
                    }
                }
            }
            if (policy.shadowsVisible && style.mode == "enclosed" &&
                style.shadowAlpha > 0)
            {
                const bool modular = sprites.find(style.wall + ".front");
                const int facing =
                    buildingView(f, object.door, policy.viewAzimuthDegrees);
                const bool side =
                    h > w * 1.4 || (std::abs(w - h) < .5 && facing % 2);
                const auto roofName =
                    modular ? style.roof + (side ? ".full.side" : ".full")
                            : object.objectTypeId + ".roof.full";
                const auto* roofArt = sprites.find(roofName);
                if (const auto* variant = sprites.find(
                        roofName + "." +
                        std::to_string(1 + ((id ^ (id >> 3) ^ (id >> 17)) % 4))
                    ))
                {
                    roofArt = variant;
                }
                if (roofArt && roofArt->shadow && policy.roofsVisible)
                {
                    const double rw =
                        modular ? w + .44
                                : roofArt->width + w - style.moduleWidth;
                    const double rh =
                        modular ? h + .08
                                : roofArt->height + h - style.moduleDepth;
                    // Short sun cast from the roof's real silhouette. Ground
                    // floors cover its interior, leaving only the cast edge.
                    const auto b = projection.bounds(
                        {modular ? x + .06
                                 : x - roofArt->width * roofArt->pivotX + .28,
                         y + .22,
                         0,
                         rw,
                         rh,
                         0,
                         0}
                    );
                    queue.submit(
                        {b,
                         {},
                         y,
                         id,
                         -1,
                         0,
                         roofArt->shadow.get(),
                         {0,
                          0,
                          float(roofArt->shadow->width()),
                          float(roofArt->shadow->height())}}
                    );
                }
                else
                {
                    const double cast = policy.roofsVisible ? .24 : .08;
                    for (int band = 0; band < 3; ++band)
                    {
                        const double inset = band * cast / 3;
                        const auto shade = RenderColor{
                            32,
                            44,
                            67,
                            std::uint8_t(48 - band * 12)
                        };
                        queue.submit(
                            {projection.bounds(
                                 {x + w + inset, y + .12, 0, cast / 3, h, 0, 0}
                             ),
                             shade,
                             y,
                             id,
                             -1}
                        );
                        queue.submit(
                            {projection.bounds(
                                 {x + .12,
                                  y + h + inset,
                                  0,
                                  w - .12,
                                  cast / 3,
                                  0,
                                  0}
                             ),
                             shade,
                             y,
                             id,
                             -1}
                        );
                    }
                }
            } // Floor is a ground surface, visible through the roof toggle. It
            // retains the existing footprint palette until the artist replaces
            // it.
            homeDetails(
                queue,
                projection,
                sprites,
                policy,
                object,
                id,
                doubleRows[object.id]
            );
            if (style.mode == "enclosed")
            {
                bool handled = false;
                const auto* wallArt = sprites.find(style.wall + ".front");
                if (renderer && wallArt && !animatedArt && doorOpen == 0 &&
                    w + 2 * marginX + 4 <= 128 && h + 2 * marginY + 4 <= 128)
                {
                    const std::array<double, 9> cacheKey{
                        double(policy.roofsVisible),
                        double(policy.shadowsVisible),
                        policy.viewAzimuthDegrees,
                        x,
                        y,
                        w,
                        h,
                        object.door ? double(object.door->x) : -1,
                        object.door ? double(object.door->y) : -1
                    };
                    auto& cached = buildingCommands_[id];
                    if (cached.key != cacheKey ||
                        cached.art != wallArt->texture || !cached.valid)
                    {
                        cached.key = cacheKey;
                        cached.art = wallArt->texture;
                        cached.projection = {
                            x + w * .5,
                            y + h * .5,
                            16,
                            int(std::ceil((w + 2 * marginX + 4) * 16)),
                            int(std::ceil((h + 2 * marginY + 4) * 16))
                        };
                        SceneDrawQueue local;
                        cached.valid = tribalBuilding(
                            local,
                            cached.projection,
                            sprites,
                            policy,
                            object.objectTypeId,
                            f,
                            object.door,
                            id,
                            0
                        );
                        cached.items = local.items();
                    }
                    cached.used = commandFrame_;
                    handled = cached.valid;
                    const float scale = float(projection.tilePixels / 16);
                    const float dx = float(
                        projection.screenWidth * .5 +
                        (cached.projection.cameraX - projection.cameraX) *
                            projection.tilePixels -
                        cached.projection.screenWidth * .5 * scale
                    );
                    const float dy = float(
                        projection.screenHeight * .5 +
                        (cached.projection.cameraY - projection.cameraY) *
                            projection.tilePixels -
                        cached.projection.screenHeight * .5 * scale
                    );
                    for (auto item : cached.items)
                    {
                        item.bounds = {
                            dx + item.bounds.x * scale,
                            dy + item.bounds.y * scale,
                            item.bounds.width * scale,
                            item.bounds.height * scale
                        };
                        item.thatchPixelPitch *= scale;
                        item.windSeconds = animate ? sprites.time() : 0;
                        if (projection.visible(item.bounds))
                        {
                            queue.submit(item);
                        }
                    }
                    if (buildingCommands_.size() > 128)
                    {
                        auto oldest = std::min_element(
                            buildingCommands_.begin(),
                            buildingCommands_.end(),
                            [](const auto& a, const auto& b)
                            { return a.second.used < b.second.used; }
                        );
                        if (oldest->first != id)
                        {
                            buildingCommands_.erase(oldest);
                        }
                    }
                }
                else
                {
                    handled = tribalBuilding(
                        queue,
                        projection,
                        sprites,
                        policy,
                        object.objectTypeId,
                        f,
                        object.door,
                        id,
                        doorOpen
                    );
                }
                if (handled)
                {
                    continue;
                }
            }
            const auto floorStart = queue.size();
            if (!policy.roofsVisible ||
                !sprites.find(object.objectTypeId + ".roof.full"))
            {
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
            }
            if (placeholder && object.door)
            {
                auto threshold = wall;
                threshold.alpha = std::uint8_t(style.sideShade);
                queue.submit(
                    {projection.bounds(
                         {double(object.door->x),
                          double(object.door->y),
                          0,
                          1,
                          1,
                          0,
                          0}
                     ),
                     threshold,
                     y,
                     id,
                     -2}
                );
            }
            queue.setLayerFrom(floorStart, -2);
            if (style.mode != "enclosed")
            {
                if (style.mode == "ground")
                {
                    if (object.objectTypeId ==
                        SettlementObjectTypes::Pastureland)
                    {
                        pastureFence(queue, projection, sprites, f, id);
                    }
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
                if (projection.tilePixels <
                    std::max(StaticDetailPixels, policy.detailTilePixels))
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
                            if (policy.shadowsVisible && style.shadowAlpha > 0)
                            {
                                const double breadth = std::min(cellW * .8, 2.);
                                for (int band = 0; band < 3; band++)
                                {
                                    const double fraction =
                                        band == 1 ? 1. : .65;
                                    queue.submit(
                                        {projection.bounds(
                                             {cx + .15,
                                              cy + .06 + band * .08,
                                              0,
                                              breadth * fraction,
                                              .08,
                                              .5,
                                              .5}
                                         ),
                                         {12, 16, 24, 85},
                                         cy,
                                         id,
                                         -1}
                                    );
                                }
                            }
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
                // Placeholder-only shading; authored pixels are never painted
                // over with proxy geometry. Scene lighting is a separate pass.
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
            // Directional artwork is a complete wall. Cutaway selection is
            // whole-wall, never individual holes. The rear wall cannot occlude
            // the interior. North and south walls have real door gaps. Side
            // walls are split into visible row segments so people can pass
            // either side correctly.
            for (int edge = 0; edge < 2; ++edge)
            {
                if (!edge && policy.roofsVisible &&
                    sprites.find(object.objectTypeId + ".roof.full"))
                {
                    continue;
                }
                const double row = edge ? y + h - 1 : y;
                const double wallHeight =
                    (!policy.roofsVisible && !edge) ? style.height : height;
                const double base = edge ? y + h : y + thickness;
                const std::string direction = edge ? "south" : "north";
                std::string piece =
                    object.objectTypeId + "." +
                    ((!policy.roofsVisible && edge) ? "cap." : "wall.") +
                    direction;
                if (!policy.roofsVisible && !edge &&
                    sprites.find(object.objectTypeId + ".wall.front"))
                {
                    piece = object.objectTypeId + (buildingView(
                                                       object.footprint,
                                                       object.door,
                                                       policy.viewAzimuthDegrees
                                                   ) == 0
                                                       ? ".wall.front"
                                                       : ".wall.back");
                }
                if (sprites.placed(
                        queue,
                        projection,
                        piece,
                        x,
                        base,
                        edge ? y + h : y,
                        id,
                        edge ? 12 : 1,
                        w,
                        0,
                        edge && policy.roofsVisible ? doorOpen : 0
                    ))
                {
                    continue;
                }
                const double depth = edge ? y + h : y;
                const bool door = object.door && object.door->y == row;
                const double cut = door ? object.door->x - x : w;
                if (cut > 0)
                {
                    strip(
                        x,
                        base - wallHeight,
                        cut,
                        wallHeight,
                        depth,
                        edge ? 12 : 1
                    );
                }
                if (door && cut + 1 < w)
                {
                    strip(
                        x + cut + 1,
                        base - wallHeight,
                        w - cut - 1,
                        wallHeight,
                        depth,
                        edge ? 12 : 1
                    );
                }
            }
            const bool coveredSides =
                policy.roofsVisible &&
                sprites.find(object.objectTypeId + ".roof.full");
            const bool west =
                coveredSides ||
                sprites.placed(
                    queue,
                    projection,
                    object.objectTypeId +
                        (policy.roofsVisible ? ".wall.west" : ".cap.west"),
                    x,
                    y + h,
                    y + h,
                    id,
                    2,
                    0,
                    policy.roofsVisible ? 0 : h
                );
            const bool east =
                coveredSides ||
                sprites.placed(
                    queue,
                    projection,
                    object.objectTypeId +
                        (policy.roofsVisible ? ".wall.east" : ".cap.east"),
                    x + w,
                    y + h,
                    y + h,
                    id,
                    2,
                    0,
                    policy.roofsVisible ? 0 : h
                );
            if (projection.tilePixels >=
                std::max(StaticDetailPixels, policy.detailTilePixels))
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
                        if ((edge && east) || (!edge && west))
                        {
                            continue;
                        }
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
                if (!west)
                {
                    strip(x, y - height, thickness, h, y + h, 2);
                }
                if (!east)
                {
                    strip(
                        x + w - thickness,
                        y - height,
                        thickness,
                        h,
                        y + h,
                        2
                    );
                }
            }
            for (const auto& p : sprites.pieces())
            {
                if (p.object != object.objectTypeId ||
                    (p.state == "roofed" && !policy.roofsVisible) ||
                    (p.state == "cutaway" && policy.roofsVisible))
                {
                    continue;
                }
                sprites.placed(
                    queue,
                    projection,
                    p.sprite,
                    x + p.x,
                    y + p.y,
                    y + p.depth,
                    id,
                    20
                );
            }
            if (policy.roofsVisible)
            {
                const auto* fullRoof =
                    sprites.find(object.objectTypeId + ".roof.full");
                const double extraWidth = w - style.moduleWidth;
                const double extraDepth = h - style.moduleDepth;
                if (sprites.placed(
                        queue,
                        projection,
                        object.objectTypeId + ".roof.full",
                        x + (fullRoof ? extraWidth * fullRoof->pivotX : 0),
                        y + (fullRoof ? extraDepth * fullRoof->pivotY : 0),
                        y + h,
                        id,
                        10,
                        fullRoof ? std::max(.125, fullRoof->width + extraWidth)
                                 : 0,
                        fullRoof ? std::max(.125, fullRoof->height + extraDepth)
                                 : 0
                    ))
                {
                    continue;
                }
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

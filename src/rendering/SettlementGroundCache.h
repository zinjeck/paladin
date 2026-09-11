#pragma once
#include "rendering/SceneDetail.h"
#include "rendering/SettlementEnvironmentDetails.h"
#include "rendering/WorldPixelGrid.h"
#include <SDL3/SDL.h>

namespace Paladin
{
    class SettlementGroundCache
    {
        struct Entry
        {
            std::shared_ptr<Texture> texture;
            std::uint64_t used = 0, version = 0, signature = 0;
        };
        std::unordered_map<SettlementObjectId, Entry, StrongIdHash> entries_;
        std::unordered_map<
            SettlementObjectId,
            std::vector<SceneDrawItem>,
            StrongIdHash>
            pendingCommands_;
        std::shared_ptr<Texture> source_, atlas_;
        int atlasX_ = 0, atlasY_ = 0, atlasRow_ = 0;
        std::uint64_t instance_ = 0, version_ = 0, frame_ = 0;
        std::size_t bytes_ = 0;
        std::size_t builds_ = 0;
        int remaining_ = 0;

        static std::uint64_t signature(
            const SettlementMap& map,
            const CompletedSettlementObject& object
        )
        {
            const auto& f = object.footprint;
            std::uint64_t hash = 1469598103934665603ull;
            const auto add = [&](std::uint64_t value)
            { hash = (hash ^ value) * 1099511628211ull; };
            for (const int value :
                 {f.topLeft.x, f.topLeft.y, f.width, f.height})
            {
                add(std::uint32_t(value));
            }
            if (object.objectTypeId == SettlementObjectTypes::Road)
            {
                for (int y = f.topLeft.y - 2; y < f.topLeft.y + f.height + 2;
                     ++y)
                {
                    for (int x = f.topLeft.x - 2; x < f.topLeft.x + f.width + 2;
                         ++x)
                    {
                        const auto* neighbor =
                            map.objectState().completedObjectAt({x, y});
                        add(neighbor ? neighbor->id.value() : 0);
                    }
                }
            }
            if (object.objectTypeId != SettlementObjectTypes::Road)
            {
                add(object.door ? std::uint32_t(object.door->x) : ~0u);
                add(object.door ? std::uint32_t(object.door->y) : ~0u);
                // Building ground now depends on touching roads. Keep this a
                // local perimeter dependency, not a global cache invalidation.
                const auto neighbor = [&](int x, int y)
                {
                    const auto* at = map.objectState().completedObjectAt({x, y});
                    add(at ? at->id.value() : 0);
                };
                for (int x = f.topLeft.x - 1; x <= f.topLeft.x + f.width; ++x)
                {
                    neighbor(x, f.topLeft.y - 1);
                    neighbor(x, f.topLeft.y + f.height);
                }
                for (int y = f.topLeft.y; y < f.topLeft.y + f.height; ++y)
                {
                    neighbor(f.topLeft.x - 1, y);
                    neighbor(f.topLeft.x + f.width, y);
                }
            }
            return hash;
        }

        static std::uint64_t detailHash(std::uint64_t value)
        {
            value ^= value >> 30;
            value *= 0xbf58476d1ce4e5b9ULL;
            value ^= value >> 27;
            value *= 0x94d049bb133111ebULL;
            return value ^ (value >> 31);
        }

        static void buildingEdgeDetails(
            SceneDrawQueue& queue,
            const SceneProjection& p,
            const SettlementMap& map,
            const CompletedSettlementObject& object,
            std::uint64_t id
        )
        {
            if (p.tilePixels < AnimationDetailPixels)
            {
                return;
            }
            const auto& f = object.footprint;
            constexpr double pixel = 1. / WorldPixelsPerTile;
            const double x = f.topLeft.x, y = f.topLeft.y;
            const double w = f.width, h = f.height;
            int part = 70;
            const auto draw = [&](double px,
                                  double py,
                                  double pw,
                                  double ph,
                                  RenderColor color,
                                  double depth)
            {
                const auto bounds = p.bounds({px, py, 0, pw, ph, 0, 0});
                if (p.visible(bounds))
                {
                    queue.submit({bounds, color, depth, id, -1, part++});
                }
            };
            const auto outsideObject = [&](double px, double py)
            {
                return map.objectState().completedObjectAt(
                    {int(std::floor(px)), int(std::floor(py))}
                );
            };
            const auto validGround = [&](double px, double py)
            {
                const auto* tile = map.grid().tile(
                    {int(std::floor(px)), int(std::floor(py))}
                );
                return tile && tile->terrain == TerrainType::Land;
            };
            const auto doorGap = [&](int edge, double along)
            {
                if (!object.door)
                {
                    return false;
                }
                const auto& door = *object.door;
                if (edge == 0 && door.y == f.topLeft.y)
                {
                    return std::abs(x + along - (door.x + .5)) < .68;
                }
                if (edge == 2 && door.y == f.topLeft.y + f.height - 1)
                {
                    return std::abs(x + along - (door.x + .5)) < .68;
                }
                if (edge == 1 && door.x == f.topLeft.x + f.width - 1)
                {
                    return std::abs(y + along - (door.y + .5)) < .68;
                }
                if (edge == 3 && door.x == f.topLeft.x)
                {
                    return std::abs(y + along - (door.y + .5)) < .68;
                }
                return false;
            };

            // A broken one-pixel contact band keeps the front wall grounded.
            // It is allowed to sit on a road directly outside the door wall;
            // the doorway itself remains clear.
            for (int i = 0; i < f.width; ++i)
            {
                const auto seed = detailHash(
                    object.id.value() ^
                    (std::uint64_t(std::uint32_t(f.topLeft.x + i)) << 32) ^
                    std::uint32_t(f.topLeft.y + f.height) ^
                    0x4f1bbcdcULL
                );
                if (seed % 5 == 0)
                {
                    continue;
                }
                const double along = std::clamp(
                    i + .5 + (int((seed >> 9) % 5) - 2) * pixel,
                    .25,
                    w - .25
                );
                if (doorGap(2, along))
                {
                    continue;
                }
                const double ox = x + along;
                const double oy = y + h + pixel * .5;
                if (!validGround(ox, oy))
                {
                    continue;
                }
                const auto* occupant = outsideObject(ox, oy);
                const bool road = occupant &&
                                  occupant->objectTypeId ==
                                      SettlementObjectTypes::Road;
                if (occupant && !road)
                {
                    continue;
                }
                const double length = (4 + int((seed >> 15) % 4)) * pixel;
                draw(
                    ox - length * .5,
                    y + h,
                    length,
                    pixel,
                    {57, 43, 60, std::uint8_t(road ? 92 : 72)},
                    y + h + .001
                );
            }

            // Sparse, stable accents break the ruler-straight foundation edge.
            // They are presentation only. A few pixels may overlap an adjacent
            // road, but no road tile, collision or navigation state is changed.
            for (int edge = 0; edge < 4; ++edge)
            {
                const int samples = edge % 2 ? f.height : f.width;
                const double length = edge % 2 ? h : w;
                for (int i = 0; i < samples; ++i)
                {
                    const auto seed = detailHash(
                        object.id.value() * 0x9e3779b97f4a7c15ULL ^
                        std::uint64_t(edge * 131 + i * 977) ^
                        (std::uint64_t(std::uint32_t(f.topLeft.x)) << 32) ^
                        std::uint32_t(f.topLeft.y)
                    );
                    const int chance = edge == 2 ? 68 : 42;
                    if (int(seed % 100) >= chance)
                    {
                        continue;
                    }
                    const double along = std::clamp(
                        i + .5 + (int((seed >> 8) % 5) - 2) * pixel,
                        .22,
                        length - .22
                    );
                    if (doorGap(edge, along))
                    {
                        continue;
                    }
                    double ox = 0, oy = 0;
                    if (edge == 0)
                    {
                        ox = x + along;
                        oy = y - pixel;
                    }
                    else if (edge == 1)
                    {
                        ox = x + w + pixel;
                        oy = y + along;
                    }
                    else if (edge == 2)
                    {
                        ox = x + along;
                        oy = y + h + pixel;
                    }
                    else
                    {
                        ox = x - pixel;
                        oy = y + along;
                    }
                    if (!validGround(ox, oy))
                    {
                        continue;
                    }
                    const auto* occupant = outsideObject(ox, oy);
                    const bool road = occupant &&
                                      occupant->objectTypeId ==
                                          SettlementObjectTypes::Road;
                    if (occupant && !road)
                    {
                        continue;
                    }
                    int kind = int((seed >> 17) % 4);
                    if (road && kind == 2 && (seed >> 23) % 13 != 0)
                    {
                        kind = 0;
                    }
                    const double depth = edge == 0 ? y : edge == 2 ? y + h
                                                    : y + along;
                    if (kind == 0)
                    {
                        const double sw = (2 + int((seed >> 25) % 2)) * pixel;
                        const double sh = (1 + int((seed >> 27) % 2)) * pixel;
                        const double sx = edge == 1   ? x + w
                                          : edge == 3 ? x - sw
                                                      : ox - sw * .5;
                        const double sy = edge == 0   ? y - sh
                                          : edge == 2 ? y + h
                                                      : oy - sh * .5;
                        draw(sx, sy, sw, sh, {89, 102, 121, 255}, depth);
                        draw(
                            sx,
                            sy,
                            pixel,
                            pixel,
                            {154, 167, 175, 255},
                            depth + .001
                        );
                    }
                    else if (kind == 1)
                    {
                        const double dw = (2 + int((seed >> 25) % 2)) * pixel;
                        const double dh = pixel;
                        const double dx = edge == 1   ? x + w
                                          : edge == 3 ? x - dw
                                                      : ox - dw * .5;
                        const double dy = edge == 0   ? y - dh
                                          : edge == 2 ? y + h
                                                      : oy - dh * .5;
                        draw(dx, dy, dw, dh, {122, 80, 56, 255}, depth);
                        if ((seed >> 29) % 2)
                        {
                            draw(
                                dx + pixel,
                                dy,
                                pixel,
                                pixel,
                                {183, 131, 80, 255},
                                depth + .001
                            );
                        }
                    }
                    else if (kind == 2)
                    {
                        const double wx = edge == 1   ? x + w
                                          : edge == 3 ? x - pixel
                                                      : ox;
                        const double wy = edge == 0   ? y - 2 * pixel
                                          : edge == 2 ? y + h - pixel
                                                      : oy - pixel;
                        draw(
                            wx,
                            wy,
                            pixel,
                            2 * pixel,
                            {51, 122, 88, 255},
                            depth
                        );
                        draw(
                            wx + pixel,
                            wy + pixel,
                            pixel,
                            pixel,
                            {121, 181, 109, 255},
                            depth + .001
                        );
                    }
                    else
                    {
                        const bool horizontal = edge == 0 || edge == 2;
                        const double tw = horizontal ? 2 * pixel : pixel;
                        const double th = horizontal ? pixel : 2 * pixel;
                        const double tx = edge == 1   ? x + w
                                          : edge == 3 ? x - tw
                                                      : ox - tw * .5;
                        const double ty = edge == 0   ? y - th
                                          : edge == 2 ? y + h
                                                      : oy - th * .5;
                        draw(tx, ty, tw, th, {73, 53, 47, 255}, depth);
                        draw(
                            tx,
                            ty,
                            horizontal ? tw : pixel,
                            pixel,
                            {136, 96, 68, 255},
                            depth + .001
                        );
                    }
                }
            }
        }

    public:
        std::size_t buildCount() const
        {
            return builds_;
        }
        void begin(const SettlementMap& map, const SceneSpriteLibrary& sprites)
        {
            const auto* road = sprites.find("road.floor");
            const auto source = road ? road->texture : nullptr;
            if (instance_ != map.instanceId() || source_ != source)
            {
                entries_.clear();
                atlas_.reset();
                atlasX_ = atlasY_ = atlasRow_ = 0;
                pendingCommands_.clear();
                bytes_ = 0;
                instance_ = map.instanceId();
                source_ = source;
            }
            if (version_ != map.objectState().navigationVersion())
            {
                pendingCommands_.clear();
            }
            version_ = map.objectState().navigationVersion();
            ++frame_;
            remaining_ = 12;
            // Evict between frames only: the draw queue holds raw texture
            // pointers.
            if (bytes_ > 32 * 1024 * 1024)
            {
                std::erase_if(
                    entries_,
                    [&](const auto& pair)
                    {
                        if (pair.second.used + 2 >= frame_)
                        {
                            return false;
                        }
                        bytes_ -= std::size_t(pair.second.texture->width()) *
                                  pair.second.texture->height() * 4;
                        return true;
                    }
                );
            }
        }
        void prewarm(
            Renderer& renderer,
            const SceneProjection& p,
            const SettlementMap& map,
            const SceneSpriteLibrary& sprites
        )
        {
            begin(map, sprites);
            const auto deadline = SDL_GetTicksNS() + 2000000;
            SceneDrawQueue scratch;
            for (const auto& object : map.objectState().completedObjects())
            {
                if (remaining_ <= 0 || SDL_GetTicksNS() >= deadline)
                {
                    break;
                }
                if (entries_.contains(object.id))
                {
                    continue;
                }
                const auto& f = object.footprint;
                if (!p.visible(p.bounds(
                        {double(f.topLeft.x),
                         double(f.topLeft.y),
                         0,
                         double(f.width),
                         double(f.height),
                         0,
                         0}
                    )))
                {
                    continue;
                }
                if (object.objectTypeId != SettlementObjectTypes::Road &&
                    sprites.objectStyle(object.objectTypeId).mode != "enclosed")
                {
                    continue;
                }
                scratch.clear();
                submit(
                    &renderer,
                    scratch,
                    p,
                    map,
                    sprites,
                    object,
                    (object.id.value() << 3) | 2
                );
            }
        }
        bool submit(
            Renderer* renderer,
            SceneDrawQueue& queue,
            const SceneProjection& p,
            const SettlementMap& map,
            const SceneSpriteLibrary& sprites,
            const CompletedSettlementObject& object,
            std::uint64_t id
        )
        {
            if (!renderer || !source_)
            {
                return false;
            }
            const bool road =
                object.objectTypeId == SettlementObjectTypes::Road;
            const auto& f = object.footprint;
            const double pad = road ? 1 : .5, width = f.width + 2 * pad,
                         height = f.height + 2 * pad;
            const int pixels = WorldPixelsPerTile, tw = int(width * pixels),
                      th = int(height * pixels);
            if (tw > 512 || th > 512)
            {
                return false;
            }
            auto it = entries_.find(object.id);
            if (it != entries_.end() && it->second.version != version_)
            {
                // Construction elsewhere must not repeatedly discard a city's
                // finished road edges. Validate only the local dependency ring.
                if (it->second.signature == signature(map, object))
                {
                    it->second.version = version_;
                }
                else
                {
                    bytes_ -= std::size_t(it->second.texture->width()) *
                              it->second.texture->height() * 4;
                    entries_.erase(it);
                    it = entries_.end();
                }
            }
            if (it == entries_.end())
            {
                const auto size = std::size_t(tw) * th * 4;
                auto pending = pendingCommands_.find(object.id);
                if (pending == pendingCommands_.end())
                {
                    SceneDrawQueue patches;
                    SceneProjection local{
                        f.topLeft.x + f.width * .5,
                        f.topLeft.y + f.height * .5,
                        double(pixels),
                        tw,
                        th
                    };
                    if (road)
                    {
                        naturalRoad(patches, local, map, sprites, object, id);
                    }
                    else
                    {
                        buildingGround(patches, local, sprites, map, f, id);
                    }
                    pending =
                        pendingCommands_.emplace(object.id, patches.items())
                            .first;
                }
                if (remaining_ <= 0 || bytes_ + size > 33 * 1024 * 1024)
                {
                    const float scale = float(p.tilePixels / pixels);
                    const auto bounds = p.bounds(
                        {f.topLeft.x - pad,
                         f.topLeft.y - pad,
                         0,
                         width,
                         height,
                         0,
                         0}
                    );
                    for (auto item : pending->second)
                    {
                        item.bounds = {
                            bounds.x + item.bounds.x * scale,
                            bounds.y + item.bounds.y * scale,
                            item.bounds.width * scale,
                            item.bounds.height * scale
                        };
                        if (p.visible(item.bounds))
                        {
                            queue.submit(item);
                        }
                    }
                    if (!road)
                    {
                        buildingEdgeDetails(queue, p, map, object, id);
                    }
                    return true;
                }
                --remaining_;
                std::vector<TextureDrawItem> draws;
                for (const auto& item : pending->second)
                {
                    draws.push_back(
                        {item.texture,
                         item.atlasFrame,
                         item.bounds,
                         item.color,
                         item.opacity}
                    );
                }
                auto texture =
                    renderer->createTextureFromDrawItems(tw, th, draws, true);
                if (!texture)
                {
                    return false;
                }
                pendingCommands_.erase(pending);
                bytes_ += size;
                ++builds_;
                it = entries_
                         .emplace(
                             object.id,
                             Entry{
                                 renderer->cacheTextureInAtlas(
                                     atlas_,
                                     atlasX_,
                                     atlasY_,
                                     atlasRow_,
                                     std::move(texture)
                                 ),
                                 frame_,
                                 version_,
                                 signature(map, object)
                             }
                         )
                         .first;
            }
            it->second.used = frame_;
            // Cache residency is never an animation. Adjacent construction
            // changes the road contour; present the rebuilt contour
            // immediately.
            queue.submit(
                {p.bounds(
                     {f.topLeft.x - pad,
                      f.topLeft.y - pad,
                      0,
                      width,
                      height,
                      0,
                      0}
                 ),
                 {},
                 double(f.topLeft.y),
                 id,
                 road ? -2 : -3,
                 0,
                 it->second.texture.get(),
                 {0, 0, float(tw), float(th)},
                 255}
            );
            if (!road)
            {
                buildingEdgeDetails(queue, p, map, object, id);
            }
            return true;
        }
    };
} // namespace Paladin

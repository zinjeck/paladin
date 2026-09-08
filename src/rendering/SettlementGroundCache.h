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
            std::unique_ptr<Texture> texture;
            std::uint64_t used = 0, version = 0, signature = 0;
        };
        std::unordered_map<SettlementObjectId, Entry, StrongIdHash> entries_;
        std::shared_ptr<Texture> source_;
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
            return hash;
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
                bytes_ = 0;
                instance_ = map.instanceId();
                source_ = source;
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
            if (!renderer || !source_ || p.tilePixels < StaticDetailPixels)
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
                if (remaining_ <= 0 || bytes_ + size > 33 * 1024 * 1024)
                {
                    return false;
                }
                --remaining_;
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
                std::vector<TextureDrawItem> draws;
                for (const auto& item : patches.items())
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
                bytes_ += size;
                ++builds_;
                it = entries_
                         .emplace(
                             object.id,
                             Entry{
                                 std::move(texture),
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
            return true;
        }
    };
} // namespace Paladin

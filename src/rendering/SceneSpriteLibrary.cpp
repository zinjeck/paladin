#include "rendering/SceneSpriteLibrary.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace Paladin
{
    void SceneSpriteLibrary::load(Renderer& renderer, const std::string& root)
    {
        if (loaded_)
        {
            return;
        }
        loaded_ = true;
        const auto base = std::filesystem::path(root);
        std::ifstream input(base / "sprites.catalog");
        std::unordered_map<std::string, std::shared_ptr<Texture>> textures;
        std::string line;
        int number = 0;
        while (std::getline(input, line))
        {
            ++number;
            if (line.empty() || line[0] == '#')
            {
                continue;
            }
            std::istringstream row(line);
            std::string id, file;
            SceneSprite sprite;
            int smooth = 0;
            if (!(row >> id >> file >> sprite.width >> sprite.height >>
                  sprite.pivotX >> sprite.pivotY >> sprite.elevation >>
                  smooth) ||
                !std::isfinite(sprite.width) || !std::isfinite(sprite.height) ||
                !std::isfinite(sprite.elevation) ||
                !std::isfinite(sprite.pivotX) ||
                !std::isfinite(sprite.pivotY) || sprite.width < .125 ||
                sprite.height < .125 || sprite.width > 64 ||
                sprite.height > 64 || sprite.elevation < 0 ||
                sprite.elevation > 64 || sprite.pivotX < 0 ||
                sprite.pivotX > 1 || sprite.pivotY < 0 || sprite.pivotY > 1 ||
                (smooth != 0 && smooth != 1))
            {
                SDL_Log(
                    "Invalid sprite catalog row %d; retaining placeholder",
                    number
                );
                continue;
            }
            const auto relative = std::filesystem::path(file);
            if (relative.is_absolute() || relative.has_root_name() ||
                std::find(relative.begin(), relative.end(), "..") !=
                    relative.end())
            {
                SDL_Log(
                    "Sprite row %d must use a path inside assets/sprites",
                    number
                );
                continue;
            }
            const auto key = file + (smooth ? ":linear" : ":nearest");
            auto& texture = textures[key];
            if (!texture)
            {
                texture = renderer.loadImageTexture(
                    (base / relative).string().c_str(),
                    smooth != 0
                );
            }
            if (!texture)
            {
                continue;
            }
            sprite.texture = texture;
            sprites_[id] = std::move(sprite);
        }
    }
    const SceneSprite* SceneSpriteLibrary::find(const std::string& id) const
    {
        const auto it = sprites_.find(id);
        return it == sprites_.end() ? nullptr : &it->second;
    }
    bool SceneSpriteLibrary::submit(
        SceneDrawQueue& queue,
        const SceneProjection& projection,
        const std::string& id,
        double x,
        double y,
        std::uint64_t stableId,
        double scale
    ) const
    {
        const auto* s = find(id);
        if (!s)
        {
            return false;
        }
        const auto bounds = projection.bounds(
            {x,
             y,
             s->elevation * scale,
             s->width * scale,
             s->height * scale,
             s->pivotX,
             s->pivotY}
        );
        if (projection.visible(bounds))
        {
            queue.submit(
                {bounds,
                 {},
                 y,
                 stableId,
                 0,
                 0,
                 s->texture.get(),
                 {0,
                  0,
                  float(s->texture->width()),
                  float(s->texture->height())}}
            );
        }
        return true;
    }
    void SceneSpriteLibrary::surface(
        SceneDrawQueue& queue,
        const SceneProjection& projection,
        const std::string& id,
        const SceneVisual& visual,
        RenderColor fallback,
        double depth,
        std::uint64_t stableId,
        int part
    ) const
    {
        const auto bounds = projection.bounds(visual);
        if (!projection.visible(bounds))
        {
            return;
        }
        const auto* s = find(id);
        if (!s || projection.tilePixels < 8)
        {
            queue.submit({bounds, fallback, depth, stableId, 0, part});
            return;
        }
        const double w = s->width * projection.tilePixels,
                     h = s->height * projection.tilePixels;
        const int x0 = int(std::floor(std::max(0.0, -double(bounds.x)) / w));
        const int y0 = int(std::floor(std::max(0.0, -double(bounds.y)) / h));
        const int x1 = int(std::ceil(
            std::min(
                double(bounds.width),
                projection.screenWidth - double(bounds.x)
            ) /
            w
        ));
        const int y1 = int(std::ceil(
            std::min(
                double(bounds.height),
                projection.screenHeight - double(bounds.y)
            ) /
            h
        ));
        if (std::int64_t(x1 - x0) * (y1 - y0) > 8192 || queue.size() > 32768)
        {
            queue.submit({bounds, fallback, depth, stableId, 0, part});
            return;
        }
        for (int y = y0; y < y1; ++y)
        {
            for (int x = x0; x < x1; ++x)
            {
                const float width = float(std::min(w, bounds.width - x * w));
                const float height = float(std::min(h, bounds.height - y * h));
                queue.submit(
                    {{float(bounds.x + x * w),
                      float(bounds.y + y * h),
                      width,
                      height},
                     {},
                     depth,
                     stableId,
                     0,
                     part,
                     s->texture.get(),
                     {0,
                      0,
                      float(s->texture->width() * width / w),
                      float(s->texture->height() * height / h)}}
                );
            }
        }
    }
} // namespace Paladin

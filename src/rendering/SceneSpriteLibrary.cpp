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
        std::ifstream styles(base / "objects.catalog");
        std::string styleLine;
        while (std::getline(styles, styleLine))
        {
            if (styleLine.empty() || styleLine[0] == '#')
            {
                continue;
            }
            std::istringstream row(styleLine);
            std::string id;
            ObjectPresentation style;
            int outline = 1;
            if (!(row >> id >> style.mode >> style.floor >> style.wall >>
                  style.roof >> style.sprite >> style.moduleWidth >>
                  style.moduleDepth >> style.height >> style.thickness >>
                  style.frontShade >> style.sideShade >> style.edgeLight >>
                  style.shadowAlpha >> outline >> std::hex >> style.fillRgb >>
                  style.frameRgb >> std::dec >> style.bodyWidth >>
                  style.bodyDepth) ||
                (style.mode != "ground" && style.mode != "enclosed" &&
                 style.mode != "modules" && style.mode != "single") ||
                !std::isfinite(style.moduleWidth) ||
                !std::isfinite(style.moduleDepth) ||
                !std::isfinite(style.height) ||
                !std::isfinite(style.thickness) || style.moduleWidth < .25 ||
                style.moduleDepth < .25 || style.moduleWidth > 64 ||
                style.moduleDepth > 64 || style.height < 0 ||
                style.height > 16 || style.thickness < .01 ||
                style.thickness > 1 || style.frontShade < 0 ||
                style.frontShade > 255 || style.sideShade < 0 ||
                style.sideShade > 255 || style.edgeLight < 0 ||
                style.edgeLight > 255 || style.shadowAlpha < 0 ||
                style.shadowAlpha > 255 || (outline != 0 && outline != 1) ||
                style.fillRgb > 0xffffff || style.frameRgb > 0xffffff ||
                !std::isfinite(style.bodyWidth) ||
                !std::isfinite(style.bodyDepth) || style.bodyWidth <= 0 ||
                style.bodyWidth > 1 || style.bodyDepth <= 0 ||
                style.bodyDepth > 1)
            {
                SDL_Log(
                    "Invalid object presentation recipe; using flat fallback"
                );
                continue;
            }
            style.outline = outline != 0;
            objects_[id] = std::move(style);
        }
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
    const ObjectPresentation& SceneSpriteLibrary::objectStyle(
        const std::string& id
    ) const
    {
        static const ObjectPresentation fallback;
        const auto it = objects_.find(id);
        return it == objects_.end() ? fallback : it->second;
    }
    bool SceneSpriteLibrary::objectHasArt(const std::string& id) const
    {
        const auto& style = objectStyle(id);
        return find(style.floor) || find(style.wall) || find(style.roof) ||
               find(style.sprite);
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
        int part,
        bool placeholder
    ) const
    {
        const auto bounds = projection.bounds(visual);
        if (!projection.visible(bounds))
        {
            return;
        }
        const auto* s = find(id);
        if (!s && !placeholder)
        {
            return;
        }
        if (!s)
        {
            queue.submit({bounds, fallback, depth, stableId, 0, part});
            return;
        }
        const auto overview = [&]()
        {
            // Art-only coarse LOD: never resurrect the placeholder under art.
            queue.submit(
                {bounds,
                 {},
                 depth,
                 stableId,
                 0,
                 part,
                 s->texture.get(),
                 {0,
                  0,
                  float(s->texture->width()),
                  float(s->texture->height())}}
            );
        };
        if (projection.tilePixels < 8)
        {
            overview();
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
            overview();
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

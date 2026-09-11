#include "rendering/SceneSpriteLibrary.h"
#include "assets/PresentationCodec.h"
#include "rendering/AssetUpload.h"
#include "rendering/SceneDetail.h"
#ifdef PALADIN_SOURCE_ART
#include "tools/asset_compiler/AssetCompiler.h"
#include "tools/asset_compiler/SourceImporter.h"
#endif
#include <SDL3/SDL.h>
#include <mutex>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace Paladin
{
    void SceneSpriteLibrary::load(Renderer& renderer, const std::string& root)
    {
        if (loaded_)
        {
            return;
        }
        try
        {
#ifdef PALADIN_SOURCE_ART
            // Tiny authoring fixtures / development overrides use the same
            // importer.
            if (
                std::filesystem::exists(std::filesystem::path(root)) &&
                std::filesystem::weakly_canonical(root) !=
                    std::filesystem::weakly_canonical(
                        std::filesystem::path(SDL_GetBasePath()) /
                        "assets/sprites"
                    )
#ifdef PALADIN_ART_ROOT
                && std::filesystem::weakly_canonical(root) !=
                       std::filesystem::weakly_canonical(PALADIN_ART_ROOT)
#endif
#ifdef PALADIN_TEST_SOURCE_ROOT
                && std::filesystem::weakly_canonical(root) !=
                       std::filesystem::weakly_canonical(
                           std::filesystem::path(PALADIN_TEST_SOURCE_ROOT) /
                           "assets/sprites"
                       )
#endif
            )
            {
                SourceImporter source(false);
                ImportImages cpu;
                source.load(cpu, root);
                sprites_.clear();
                for (auto& [id, in] : source.sprites_)
                {
                    SceneSprite out;
                    static_cast<SpriteAsset&>(out) = in;
                    out.texture = renderer.createTextureFromPixels(
                        in.texture->width(),
                        in.texture->height(),
                        in.texture->atlas.pixels
                    );
                    renderer.setTextureFiltering(
                        *out.texture,
                        in.texture->atlas.linear
                    );
                    if (in.shadow)
                    {
                        out.shadow = renderer.createTextureFromPixels(
                            in.shadow->width(),
                            in.shadow->height(),
                            in.shadow->atlas.pixels
                        );
                    }
                    sprites_[id] = std::move(out);
                }
                objects_ = std::move(source.objects_);
                pieces_ = std::move(source.pieces_);
                lights_ = std::move(source.lights_);
                loaded_ = true;
                return;
            }
#ifdef PALADIN_ART_ROOT
            // F6 resets this facade; incremental import is content-keyed.
            static std::mutex compileMutex;
            std::lock_guard lock(compileMutex);
            ensureDevelopmentAssets(
                PALADIN_ART_ROOT,
                std::filesystem::path(SDL_GetBasePath()) / "assets/packages",
                std::filesystem::path(PALADIN_ART_ROOT) / "../../.cache/assets"
            );
#endif
#endif
            auto manager = renderer.compiledAssets();
            std::unordered_map<std::string, SceneSprite> sprites;
            std::unordered_map<std::string, ObjectPresentation> objects;
            std::vector<BuildingPiece> pieces;
            std::vector<BlueprintLight> lights;
            for (auto& record : manager->records())
            {
                auto name = record.name.starts_with("paladin:")
                                ? record.name.substr(8)
                                : record.name;
                if (record.type == AssetType::Sprite ||
                    record.type == AssetType::UiAsset)
                {
                    SceneSprite sprite;
                    static_cast<SpriteAsset&>(sprite) =
                        decodeSprite(manager->data(record.id));
                    sprite.texture = assetTextureView(
                        renderer,
                        *manager,
                        sprite.atlas,
                        sprite.x,
                        sprite.y,
                        sprite.pixelWidth,
                        sprite.pixelHeight
                    );
                    if (sprite.shadowAtlas)
                    {
                        sprite.shadow = assetTextureView(
                            renderer,
                            *manager,
                            sprite.shadowAtlas,
                            sprite.shadowX,
                            sprite.shadowY,
                            sprite.shadowWidth,
                            sprite.shadowHeight
                        );
                    }
                    AssetHandle h{
                        record.id,
                        manager->residency(record.id)->generation,
                        record.type
                    };
                    manager->completeUpload(h, {}, 0);
                    sprites[name] = std::move(sprite);
                }
                else if (record.type == AssetType::ObjectPresentation)
                {
                    auto key = name;
                    auto start = key.find(':');
                    start = start == std::string::npos ? 0 : start + 1;
                    if (key.compare(start, 13, "presentation.") != 0)
                    {
                        throw std::runtime_error(
                            "Invalid presentation asset name: " + name
                        );
                    }
                    key.erase(start, 13);
                    objects[key] = decodePresentation(manager->data(record.id));
                }
                else if (name == "recipe.pieces")
                {
                    pieces = decodePieces(manager->data(record.id));
                }
                else if (name == "recipe.lights")
                {
                    lights = decodeLights(manager->data(record.id));
                }
            }
            assets_ = std::move(manager);
            sprites_ = std::move(sprites);
            objects_ = std::move(objects);
            pieces_ = std::move(pieces);
            lights_ = std::move(lights);
            loaded_ = true;
        }
        catch (const std::exception& e)
        {
            SDL_Log("Asset load rejected: %s", e.what());
            loaded_ = true;
        }
    }
    RenderRectangle SceneSpriteLibrary::frame(
        const SceneSprite& sprite,
        bool animate
    ) const
    {
        const int index =
            animate && sprite.fps > 0
                ? int(std::fmod(seconds_ * sprite.fps, sprite.frames))
                : 0;
        const float width = float(sprite.texture->width() / sprite.frames);
        return {index * width, 0, width, float(sprite.texture->height())};
    }
    void SceneSpriteLibrary::submitWind(
        SceneDrawQueue& queue,
        const SceneSprite& sprite,
        const RenderRectangle& b,
        double depth,
        std::uint64_t id,
        int part,
        bool roof
    ) const
    {
        const auto f = frame(sprite);
        if (b.width < 8 || f.height < 4)
        {
            queue.submit({b, {}, depth, id, 0, part, sprite.texture.get(), f});
            return;
        }
        // Only the loose eave fringe moves on a roof; its structure stays
        // fixed. Grass bends more at the tips and stays rooted at the bottom.
        // Slice on whole source rows so nearest sampling stays stable.
        const float moving =
            roof ? std::max(1.F, std::floor(std::min(8.F, f.height * .12F)))
                 : f.height;
        const float start = f.height - moving;
        if (roof)
        {
            // Loose thatch moves over an intact roof. Independent band
            // rasterization can otherwise expose background at fitted joins.
            queue.submit({b, {}, depth, id, 0, part, sprite.texture.get(), f});
        }
        for (float row = start; row < f.height; row += 2)
        {
            const float count = std::min(2.F, f.height - row);
            const double t = (row - start + .5) / moving;
            const double strength = (roof ? t * .9 : (1 - t) * 1.3) *
                                    detailBlend(b.width / sprite.width, 24, 36);
            const float offset =
                float(std::round(
                    std::sin(seconds_ * 2.1 + (id % 97) * .31 - t * .6) *
                    strength
                )) *
                b.width / f.width;
            queue.submit(
                {{b.x + offset,
                  b.y + b.height * row / f.height,
                  b.width,
                  b.height * count / f.height},
                 {},
                 depth,
                 id,
                 0,
                 part,
                 sprite.texture.get(),
                 {f.x, f.y + row, f.width, count}}
            );
        }
    }
    bool SceneSpriteLibrary::placed(
        SceneDrawQueue& q,
        const SceneProjection& p,
        const std::string& name,
        double x,
        double y,
        double depth,
        std::uint64_t id,
        int part,
        double fittedWidth,
        double fittedHeight,
        double doorOpen
    ) const
    {
        const auto* sprite = find(name);
        const auto* variant = find(
            name + "." + std::to_string(1 + ((id ^ (id >> 3) ^ (id >> 17)) % 4))
        );
        if (variant)
        {
            sprite = variant;
        }
        if (!sprite)
        {
            return false;
        }
        const auto b = p.bounds(
            {x,
             y,
             sprite->elevation,
             fittedWidth > 0 ? fittedWidth : sprite->width,
             fittedHeight > 0 ? fittedHeight : sprite->height,
             sprite->pivotX,
             sprite->pivotY}
        );
        if (p.visible(b))
        {
            // Grounded props use their cached alpha silhouette, flattened
            // onto the receiving surface. No shadow textures are made here.
            const bool grounded =
                name.starts_with("home.bed.") ||
                name.starts_with("furniture.") || name == "home.detail.jars" ||
                name == "home.detail.basket" || name == "stockpile.crate" ||
                name == "stockpile.stack" || name == "fishing_grounds.station";
            const bool mounted = name == "home.detail.shutters" ||
                                 name == "home.detail.hide" ||
                                 name.ends_with(".door");
            if (shadowsEnabled_ && sprite->shadow && (grounded || mounted))
            {
                const auto shadow =
                    grounded
                        ? RenderRectangle{b.x + b.width * .08F, b.y + b.height * .70F, b.width * .96F, b.height * .40F}
                        : RenderRectangle{
                              b.x + float(p.tilePixels / 16.),
                              b.y + float(p.tilePixels / 16.),
                              b.width,
                              b.height
                          };
                q.submit(
                    {shadow,
                     {},
                     depth,
                     id,
                     0,
                     part - 1,
                     sprite->shadow.get(),
                     {0,
                      0,
                      float(sprite->shadow->width()),
                      float(sprite->shadow->height())}}
                );
                if (grounded)
                {
                    q.submit(
                        {{b.x + b.width * .16F,
                          b.y + b.height * .90F,
                          b.width * .70F,
                          std::max(float(p.tilePixels / 16), b.height * .10F)},
                         {57, 43, 60, 52},
                         depth,
                         id,
                         0,
                         part - 1}
                    );
                }
            }
            const auto roofSuffix = name.find(".roof.full");
            const bool thatch =
                name.starts_with("roof.thatch.full") ||
                (roofSuffix != std::string::npos &&
                 objectStyle(name.substr(0, roofSuffix)).roof == "roof.thatch");
            if (thatch)
            {
                SceneDrawItem item{
                    b,
                    {},
                    depth,
                    id,
                    0,
                    part,
                    sprite->texture.get(),
                    frame(*sprite)
                };
                item.thatchPixelPitch = float(p.tilePixels / 16.);
                item.ridgeAlongDepth = name.find(".side") != std::string::npos;
                item.windSeconds =
                    p.tilePixels >= AnimationDetailPixels ? seconds_ : 0;
                q.submit(item);
                return true;
            }
            if ((name.find(".roof.full") != std::string::npos ||
                 (name.starts_with("roof.") &&
                  name.find(".full") != std::string::npos)) &&
                p.tilePixels >= AnimationDetailPixels)
            {
                submitWind(q, *sprite, b, depth, id, part, true);
                return true;
            }
            q.submit(
                {b,
                 {},
                 depth,
                 id,
                 0,
                 part,
                 sprite->texture.get(),
                 frame(*sprite, p.tilePixels >= AnimationDetailPixels)}
            );
            const auto& d = sprite->doorRegion;
            if (p.tilePixels >= AnimationDetailPixels && doorOpen > 0 &&
                d.width > 0)
            {
                const auto f = frame(*sprite);
                const RenderRectangle opening{
                    b.x + b.width * d.x,
                    b.y + b.height * d.y,
                    b.width * d.width,
                    b.height * d.height
                };
                q.submit({opening, {8, 15, 27, 255}, depth, id, 0, part + 1});
                const float leafWidth =
                    opening.width *
                    float(std::cos(std::clamp(doorOpen, 0., 1.) * 1.42));
                q.submit(
                    {{opening.x, opening.y, leafWidth, opening.height},
                     {},
                     depth,
                     id,
                     0,
                     part + 2,
                     sprite->texture.get(),
                     {f.x + f.width * d.x,
                      f.y + f.height * d.y,
                      f.width * d.width,
                      f.height * d.height}}
                );
            }
        }
        return true;
    }
    const SceneSprite* SceneSpriteLibrary::find(const std::string& id) const
    {
        if (!environmentArtEnabled_ && id != "citizen" &&
            !id.starts_with("citizen.") && !id.starts_with("animal."))
        {
            return nullptr;
        }
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
        if (find(style.floor) || find(style.wall) || find(style.roof) ||
            find(style.sprite))
        {
            return true;
        }
        for (const char* suffix :
             {".roof.full",
              ".wall.north",
              ".wall.south",
              ".wall.east",
              ".wall.west",
              ".cap.south",
              ".cap.east",
              ".cap.west"})
        {
            if (find(id + suffix))
            {
                return true;
            }
        }
        for (const auto& p : pieces_)
        {
            if (p.object == id && find(p.sprite))
            {
                return true;
            }
        }
        return false;
    }
    bool SceneSpriteLibrary::submitTree(
        SceneDrawQueue& queue,
        const SceneProjection& projection,
        double x,
        double y,
        std::uint64_t id,
        double scale,
        double crownScale,
        TreeSpecies species
    ) const
    {
        std::uint64_t seed = id;
        const auto choice = [&]()
        {
            seed ^= seed >> 30;
            seed *= 0xbf58476d1ce4e5b9ULL;
            seed ^= seed >> 27;
            seed *= 0x94d049bb133111ebULL;
            seed ^= seed >> 31;
            return 1 + seed % 3;
        };
        const auto* trunk = find("tree.trunk." + std::to_string(choice()));
        const auto branchVariant = choice();
        const auto* branch = find("tree.branch." + std::to_string(branchVariant));
        const auto* crown = find("tree.crown." + std::to_string(choice()));
        if (!trunk || !branch || !crown)
        {
            return false;
        }
        const double phase = (id % 251) * .17;
        const double sway =
            projection.tilePixels >= AnimationDetailPixels
                ? std::round(std::sin(seconds_ * 1.3 + phase) * 1.1) / 24. *
                      detailBlend(projection.tilePixels, 24, 36)
                : 0;
        const auto part = [&](const SceneSprite& s,
                              double lift,
                              double size,
                              double dx,
                              int order)
        {
            const auto b = projection.bounds(
                {x + dx * scale,
                 y,
                 lift * scale,
                 s.width * scale * size,
                 s.height * scale * size,
                 .5,
                 1}
            );
            if (projection.visible(b))
            {
                queue.submit(
                    {b,
                     {},
                     y,
                     id,
                     0,
                     order,
                     s.texture.get(),
                     frame(s, projection.tilePixels >= AnimationDetailPixels)}
                );
            }
        };
        if (species == TreeSpecies::Birch)
        {
            const auto* bark =
                find("tree.birch-trunk." + std::to_string(1 + id % 3));
            part(bark ? *bark : *trunk, 0, 1, 0, 0);
            const auto* birchBranch =
                find("tree.birch-branch." + std::to_string(branchVariant));
            part(birchBranch ? *birchBranch : *branch,
                 .40 * crownScale, crownScale * .75, sway * .35, 1);
            part(
                *crown,
                .62 * crownScale,
                crownScale * .66,
                sway - .16 * crownScale,
                2
            );
            part(
                *crown,
                .80 * crownScale,
                crownScale * .55,
                sway + .16 * crownScale,
                3
            );
        }
        else if (species == TreeSpecies::Conifer)
        {
            // A pointed, tiered evergreen crown keeps the original foliage
            // texture language and fits the common tree-clearance envelope.
            part(*trunk, 0, .85, 0, 0);
            const auto* evergreen =
                find("tree.conifer-crown." + std::to_string(1 + id % 3));
            part(
                evergreen ? *evergreen : *crown,
                .20 * crownScale,
                crownScale,
                sway,
                1
            );
        }
        else
        {
            part(*trunk, 0, 1, 0, 0);
            part(*branch, .32, crownScale, sway * .35, 1);
            part(*crown, .48, crownScale, sway, 2);
        }
        return true;
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
        if (const auto* v = find(
                id + "." +
                std::to_string(
                    1 + ((stableId ^ (stableId >> 3) ^ (stableId >> 17)) % 4)
                )
            ))
        {
            s = v;
        }
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
            if (id == "grass.tuft" &&
                projection.tilePixels >= AnimationDetailPixels)
            {
                submitWind(queue, *s, bounds, y, stableId, 0, false);
                return true;
            }
            queue.submit(
                {bounds,
                 {},
                 y,
                 stableId,
                 0,
                 0,
                 s->texture.get(),
                 frame(*s, projection.tilePixels >= AnimationDetailPixels)}
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
        const auto source =
            frame(*s, projection.tilePixels >= AnimationDetailPixels);
        const auto overview = [&]()
        {
            // Art-only coarse LOD: never resurrect the placeholder under art.
            queue.submit(
                {bounds, {}, depth, stableId, 0, part, s->texture.get(), source}
            );
        };
        if (projection.tilePixels < StaticDetailPixels)
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
                     {source.x,
                      0,
                      float(source.width * width / w),
                      float(source.height * height / h)}}
                );
            }
        }
    }
} // namespace Paladin

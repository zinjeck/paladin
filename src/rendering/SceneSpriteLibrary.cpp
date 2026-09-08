#include "rendering/SceneSpriteLibrary.h"
#include "rendering/SceneDetail.h"
#include "rendering/SpriteStyle.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
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
        loaded_ = true;
        const auto base = std::filesystem::path(root);
        std::unordered_set<std::uint32_t> palette;
        auto settings = base;
        if (std::filesystem::exists(base / "../../config/city-objects.catalog"))
        {
            settings = base / "../../config";
        }
        std::ifstream paletteFile(settings / "art-palette.hex");
        std::string color;
        while (paletteFile >> color)
        {
            if (color.size() == 7 && color[0] == '#')
            {
                try
                {
                    palette.insert(std::stoul(color.substr(1), nullptr, 16));
                }
                catch (...)
                {
                }
            }
        }
        std::unordered_map<std::string, std::unordered_map<unsigned, unsigned>>
            materialColors;
        std::ifstream colorRules(base / "material-colors.catalog");
        std::string rule;
        while (std::getline(colorRules, rule))
        {
            if (rule.empty() || rule[0] == '#')
            {
                continue;
            }
            std::istringstream row(rule);
            std::string file;
            unsigned from, to;
            if (row >> file >> std::hex >> from >> to &&
                palette.contains(from) && palette.contains(to))
            {
                materialColors[file][from] = to;
            }
        }
        std::ifstream lights(settings / "lights.catalog");
        std::string lightLine;
        while (std::getline(lights, lightLine))
        {
            if (lightLine.empty() || lightLine[0] == '#')
            {
                continue;
            }
            BlueprintLight light;
            unsigned color = 0;
            std::istringstream row(lightLine);
            if (row >> light.object >> light.x >> light.y >> light.radius >>
                light.intensity >> std::hex >> color)
            {
                if (std::isfinite(light.x) && std::isfinite(light.y) &&
                    std::isfinite(light.radius) &&
                    std::isfinite(light.intensity) && std::abs(light.x) <= 64 &&
                    std::abs(light.y) <= 64 && light.radius > 0 &&
                    light.radius <= 16 && light.intensity >= 0 &&
                    light.intensity <= 4 && color <= 0xffffff)
                {
                    light.color = {
                        std::uint8_t(color >> 16),
                        std::uint8_t(color >> 8),
                        std::uint8_t(color),
                        255
                    };
                    lights_.push_back(light);
                }
            }
        }
        std::ifstream parts(settings / "pieces.catalog");
        std::string partLine;
        while (std::getline(parts, partLine))
        {
            if (partLine.empty() || partLine[0] == '#')
            {
                continue;
            }
            BuildingPiece p;
            std::istringstream row(partLine);
            if (row >> p.object >> p.sprite >> p.state >> p.x >> p.y >> p.depth)
            {
                // Optional deterministic variant selector; old rows always
                // draw.
                if (row >> p.choices)
                {
                    if (!(row >> p.choice) || p.choices == 0 ||
                        p.choices > 64 || p.choice >= p.choices)
                    {
                        continue;
                    }
                }
                if ((p.state == "always" || p.state == "roofed" ||
                     p.state == "cutaway") &&
                    std::isfinite(p.x) && std::isfinite(p.y) &&
                    std::isfinite(p.depth) && std::abs(p.x) <= 64 &&
                    std::abs(p.y) <= 64 && std::abs(p.depth) <= 64)
                {
                    pieces_.push_back(p);
                }
            }
        }
        std::ifstream styles(
            settings /
            (settings == base ? "objects.catalog" : "city-objects.catalog")
        );
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
            row >> style.decor;
            objects_[id] = std::move(style);
        }
        std::ifstream input(base / "sprites.catalog");
        std::unordered_map<std::string, std::shared_ptr<Texture>> textures;
        std::unordered_map<std::string, std::shared_ptr<Texture>> shadows;
        std::unordered_map<std::string, RenderColor> overviewColors;
        std::unordered_map<
            std::string,
            std::shared_ptr<const std::vector<RenderColor>>>
            materials;
        std::unordered_map<std::string, RenderColor> materialBases;
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
            // Optional horizontal animation strip: frame count and
            // frames/second.
            row >> std::ws;
            if (row.peek() != EOF && row.peek() != '#')
            {
                if (!(row >> sprite.frames >> sprite.fps) ||
                    sprite.frames < 1 || sprite.frames > 256 ||
                    !std::isfinite(sprite.fps) || sprite.fps < 0 ||
                    sprite.fps > 120)
                {
                    continue;
                }
            }
            row >> std::ws;
            if (row.peek() != EOF && row.peek() != '#')
            {
                std::string kind;
                auto& d = sprite.doorRegion;
                if (!(row >> kind >> d.x >> d.y >> d.width >> d.height) ||
                    kind != "door" || !std::isfinite(d.x) ||
                    !std::isfinite(d.y) || !std::isfinite(d.width) ||
                    !std::isfinite(d.height) || d.x < 0 || d.y < 0 ||
                    d.width <= 0 || d.height <= 0 || d.x + d.width > 1 ||
                    d.y + d.height > 1)
                {
                    continue;
                }
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
            const auto key = file + (smooth ? ":linear:" : ":nearest:") +
                             (id.starts_with("world.") ? "atlas:" : "scene:") +
                             std::to_string(sprite.frames) + ":" +
                             std::to_string(sprite.width) + ":" +
                             std::to_string(sprite.height);
            auto& texture = textures[key];
            if (!texture)
            {
                auto* source = IMG_Load((base / relative).string().c_str());
                if (!source)
                {
                    SDL_Log(
                        "Cannot load sprite %s: %s",
                        file.c_str(),
                        SDL_GetError()
                    );
                    continue;
                }
                bool valid = !palette.empty();
                if (!valid)
                {
                    SDL_Log(
                        "No artwork palette loaded; rejecting %s",
                        file.c_str()
                    );
                }
                if (!palette.empty())
                {
                    auto* rgba =
                        SDL_ConvertSurface(source, SDL_PIXELFORMAT_RGBA32);
                    if (!rgba)
                    {
                        valid = false;
                    }
                    else
                    {
                        for (int y = 0; y < rgba->h && valid; ++y)
                        {
                            const auto* bytes =
                                static_cast<const Uint8*>(rgba->pixels) +
                                y * rgba->pitch;
                            for (int x = 0; x < rgba->w; ++x)
                            {
                                const auto* p = bytes + x * 4;
                                if (p[3] && (p[3] != 255 ||
                                             !palette.contains(
                                                 (unsigned(p[0]) << 16) |
                                                 (unsigned(p[1]) << 8) | p[2]
                                             )))
                                {
                                    valid = false;
                                    break;
                                }
                            }
                        }
                        SDL_DestroySurface(rgba);
                    }
                }
                if (valid)
                {
                    if (auto* simplified = simplifySprite(
                            source,
                            sprite.width,
                            sprite.height,
                            sprite.frames,
                            !id.starts_with("world.") &&
                                palette.contains(0xA6CD59) &&
                                palette.contains(0x79B56D) &&
                                palette.contains(0x49975B) &&
                                palette.contains(0x337A58) &&
                                palette.contains(0xD9C79F) &&
                                palette.contains(0xB78350)
                        ))
                    {
                        SDL_DestroySurface(source);
                        source = simplified;
                    }
                    // Material-level recoloring is shared by every building
                    // recipe, done once, and restricted to the loaded palette.
                    if (const auto rules = materialColors.find(file);
                        rules != materialColors.end())
                    {
                        if (auto* rgba = SDL_ConvertSurface(
                                source,
                                SDL_PIXELFORMAT_RGBA32
                            ))
                        {
                            for (int y = 0; y < rgba->h; ++y)
                            {
                                for (int x = 0; x < rgba->w; ++x)
                                {
                                    auto* c =
                                        static_cast<Uint8*>(rgba->pixels) +
                                        y * rgba->pitch + x * 4;
                                    const unsigned rgb =
                                        (unsigned(c[0]) << 16) |
                                        (unsigned(c[1]) << 8) | c[2];
                                    if (const auto to = rules->second.find(rgb);
                                        c[3] && to != rules->second.end())
                                    {
                                        c[0] = Uint8(to->second >> 16);
                                        c[1] = Uint8(to->second >> 8);
                                        c[2] = Uint8(to->second);
                                    }
                                }
                            }
                            SDL_DestroySurface(source);
                            source = rgba;
                        }
                    }
                    // Sample once at load time; distant terrain retains an
                    // authored palette color without building tile textures.
                    RenderColor color{};
                    SDL_ReadSurfacePixel(
                        source,
                        source->w / sprite.frames / 2,
                        source->h / 2,
                        &color.red,
                        &color.green,
                        &color.blue,
                        &color.alpha
                    );
                    overviewColors[key] = color;
                    texture =
                        renderer.createTextureFromSurface(source, smooth != 0);
                    // Shadow masks inherit the actual outline, never the PNG
                    // rectangle or its RGB. Lighting is outside the art
                    // palette.
                    auto* rgba =
                        SDL_ConvertSurface(source, SDL_PIXELFORMAT_RGBA32);
                    if (rgba)
                    {
                        if (id.starts_with("world.") ||
                            id.find("terrain.") != std::string::npos ||
                            id.find(".floor") != std::string::npos ||
                            id.starts_with("wall.") ||
                            id.starts_with("tree.crown."))
                        {
                            auto paint =
                                std::make_shared<std::vector<RenderColor>>();
                            std::unordered_map<unsigned, int> counts;
                            int best = 0;
                            for (int y = 0; y < rgba->h; ++y)
                            {
                                for (int x = 0; x < rgba->w; ++x)
                                {
                                    const auto* c = static_cast<const Uint8*>(
                                                        rgba->pixels
                                                    ) +
                                                    y * rgba->pitch + x * 4;
                                    RenderColor color{c[0], c[1], c[2], c[3]};
                                    paint->push_back(color);
                                    const auto rgb = (unsigned(c[0]) << 16) |
                                                     (unsigned(c[1]) << 8) |
                                                     c[2];
                                    if (c[3] && ++counts[rgb] > best)
                                    {
                                        best = counts[rgb];
                                        materialBases[key] = color;
                                    }
                                }
                            }
                            materials[key] = paint;
                        }
                        const int sw = std::min(64, rgba->w / sprite.frames);
                        const int sh = std::min(128, rgba->h);
                        std::vector<RenderColor> mask(std::size_t(sw) * sh);
                        for (int my = 0; my < sh; ++my)
                        {
                            for (int mx = 0; mx < sw; ++mx)
                            {
                                const int sx =
                                    mx * (rgba->w / sprite.frames) / sw;
                                const int sy = my * rgba->h / sh;
                                const auto* p =
                                    static_cast<const Uint8*>(rgba->pixels) +
                                    sy * rgba->pitch + sx * 4;
                                mask[std::size_t(my) * sw + mx] =
                                    {57, 43, 60, std::uint8_t(p[3] ? 68 : 0)};
                            }
                        }
                        const auto coverage = mask;
                        for (int my = 0; my < sh; ++my)
                        {
                            for (int mx = 0; mx < sw; ++mx)
                            {
                                auto& p = mask[std::size_t(my) * sw + mx];
                                if (!p.alpha)
                                {
                                    continue;
                                }
                                for (int radius = 1; radius <= 2; ++radius)
                                {
                                    bool edge = false;
                                    for (const auto [dx, dy] :
                                         {std::pair{-radius, 0},
                                          {radius, 0},
                                          {0, -radius},
                                          {0, radius}})
                                    {
                                        const int px = mx + dx, py = my + dy;
                                        edge |=
                                            px < 0 || py < 0 || px >= sw ||
                                            py >= sh ||
                                            !coverage
                                                 [std::size_t(
                                                      std::clamp(py, 0, sh - 1)
                                                  ) * sw +
                                                  std::clamp(px, 0, sw - 1)]
                                                     .alpha;
                                    }
                                    if (edge)
                                    {
                                        p.alpha =
                                            std::uint8_t(radius == 1 ? 24 : 42);
                                        break;
                                    }
                                }
                            }
                        }
                        shadows[key] =
                            renderer.createTextureFromPixels(sw, sh, mask);
                        SDL_DestroySurface(rgba);
                    }
                }
                else
                {
                    SDL_Log(
                        "Rejected off-palette or partial-alpha artwork: %s",
                        file.c_str()
                    );
                }
                SDL_DestroySurface(source);
            }
            if (!texture)
            {
                continue;
            }
            if (texture->width() % sprite.frames)
            {
                SDL_Log("Invalid animation strip: %s", file.c_str());
                continue;
            }
            sprite.texture = texture;
            sprite.shadow = shadows[key];
            sprite.overviewColor = overviewColors[key];
            sprite.materialPixels = materials[key];
            sprite.materialBase = materialBases[key];
            sprite.materialWidth = texture->width();
            sprite.materialHeight = texture->height();
            sprites_[id] = std::move(sprite);
        }
        // New birch bark is generated once on the canonical grid and packed
        // with foliage. Keeping it textured preserves chunk mesh batching.
        if (const auto* original = find("tree.trunk.1");
            original && palette.contains(0xD7E0E3) &&
            palette.contains(0x7E9CAA) && palette.contains(0x4E3B39))
        {
            const auto prototype = *original;
            for (int variant = 1; variant <= 3; ++variant)
            {
                std::vector<RenderColor> pixels(9 * 13, {0, 0, 0, 0});
                for (int y = 0; y < 13; ++y)
                {
                    const int center = 4 + (variant == 2 && y < 6   ? 1
                                            : variant == 3 && y < 4 ? -1
                                                                    : 0);
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        pixels[y * 9 + center + dx] =
                            dx == 1 ? RenderColor{126, 156, 170, 255}
                                    : RenderColor{215, 224, 227, 255};
                    }
                    if ((y + variant) % 4 == 0)
                    {
                        pixels[y * 9 + center] = {78, 59, 57, 255};
                        pixels[y * 9 + center - 1] = {78, 59, 57, 255};
                    }
                }
                auto birch = prototype;
                birch.texture = renderer.createTextureFromPixels(9, 13, pixels);
                birch.width = 9. / 16.;
                birch.height = 13. / 16.;
                sprites_["tree.birch-trunk." + std::to_string(variant)] =
                    std::move(birch);
            }
        }
        // Derive a pointed evergreen silhouette from the existing authored
        // foliage clusters. Source PNGs are untouched; all colors stay in the
        // approved green ramp, and the result joins the cached foliage atlas.
        for (int variant = 1; variant <= 3; ++variant)
        {
            const auto* foliage = find("tree.crown." + std::to_string(variant));
            if (!foliage || !foliage->materialPixels)
            {
                continue;
            }
            auto evergreen = *foliage;
            std::vector<RenderColor> paint(24 * 24, {0, 0, 0, 0});
            for (int y = 0; y < 24; ++y)
            {
                const int half =
                    std::max(0, int(y * .44) - (y % 6 < 2 && y > 5 ? 1 : 0));
                for (int x = 11 - half; x <= 11 + half; ++x)
                {
                    const int sx = (x + variant * 3) % foliage->materialWidth;
                    const int sy = y * foliage->materialHeight / 24;
                    auto c = (*foliage->materialPixels)
                        [sy * foliage->materialWidth + sx];
                    const unsigned rgb = (unsigned(c.red) << 16) |
                                         (unsigned(c.green) << 8) | c.blue;
                    switch (rgb)
                    {
                    case 0xA6CD59:
                    case 0xD0E58A:
                        c = {121, 181, 109, 255};
                        break;
                    case 0x79B56D:
                        c = {73, 151, 91, 255};
                        break;
                    case 0x49975B:
                        c = {51, 122, 88, 255};
                        break;
                    default:
                        c = {35, 87, 71, 255};
                        break;
                    }
                    // Needle clusters break up the broad planes, with a lit
                    // left shoulder and deep interleaved branch pockets.
                    unsigned cluster = unsigned(x / 2) * 374761393u ^
                                       unsigned(y / 2) * 668265263u ^
                                       unsigned(variant) * 2246822519u;
                    cluster = (cluster ^ (cluster >> 13)) * 1274126177u;
                    if (cluster % 7 < 3)
                    {
                        c = x <= 11 ? RenderColor{51, 122, 88, 255}
                                    : RenderColor{35, 87, 71, 255};
                    }
                    if (x <= 11 && cluster % 11 < 4)
                    {
                        c = {73, 151, 91, 255};
                    }
                    if (x <= 10 && cluster % 19 < 2)
                    {
                        c = {121, 181, 109, 255};
                    }
                    if (x > 11 && cluster % 9 < 2)
                    {
                        c = {25, 62, 66, 255};
                    }
                    if (std::abs(x - 11) == half && y > 3)
                    {
                        c = {35, 87, 71, 255};
                    }
                    if (y % 6 == 5 && x > 10)
                    {
                        c = {25, 62, 66, 255};
                    }
                    paint[y * 24 + x] = c;
                }
            }
            evergreen.texture = renderer.createTextureFromPixels(24, 24, paint);
            evergreen.width = 1.5;
            evergreen.height = 1.5;
            sprites_["tree.conifer-crown." + std::to_string(variant)] =
                std::move(evergreen);
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
        const auto* branch = find("tree.branch." + std::to_string(choice()));
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
            part(*branch, .40 * crownScale, crownScale * .75, sway * .35, 1);
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

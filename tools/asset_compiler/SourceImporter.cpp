#include "SourceImporter.h"
#include "SpriteProcessor.h"
#include <SDL3_image/SDL_image.h>
#include <fstream>
#include <sstream>
#include <unordered_set>
namespace Paladin
{
    using RenderColor = AssetPixel;
    void SourceImporter::load(ImportImages& renderer, const std::string& root)
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
            else if (rule.find_first_not_of(" \t\r") != std::string::npos)
            {
                diagnostic("Invalid material color rule: %s", rule.c_str());
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
                diagnostic(
                    "Invalid object presentation recipe; using flat fallback"
                );
                continue;
            }
            style.outline = outline != 0;
            row >> style.decor;
            objects_[id] = std::move(style);
        }
        std::ifstream input(base / "sprites.catalog");
        std::unordered_map<std::string, std::shared_ptr<ImportedImage>>
            textures;
        std::unordered_map<std::string, std::shared_ptr<ImportedImage>> shadows;
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
            ImportedSprite sprite;
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
                diagnostic(
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
                    diagnostic("Invalid animation metadata in %s", id.c_str());
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
                    diagnostic("Invalid door region in %s", id.c_str());
                    continue;
                }
            }
            const auto relative = std::filesystem::path(file);
            if (relative.is_absolute() || relative.has_root_name() ||
                std::find(relative.begin(), relative.end(), "..") !=
                    relative.end())
            {
                diagnostic(
                    "Sprite row %d must use a path inside assets/sprites",
                    number
                );
                continue;
            }
            const auto resolvedRelative =
                std::filesystem::weakly_canonical(base / relative)
                    .lexically_relative(
                        std::filesystem::weakly_canonical(base)
                    );
            if (resolvedRelative.empty() || *resolvedRelative.begin() == "..")
            {
                diagnostic("Sprite path escapes source root: %s", file.c_str());
                continue;
            }
            const auto cacheKey = cacheName(base, settings, line, file);
            if (auto cached = readCached(cacheKey))
            {
                sprites_[id] = std::move(*cached);
                ++cacheHits;
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
                    diagnostic(
                        "Cannot load sprite %s: %s",
                        file.c_str(),
                        SDL_GetError()
                    );
                    continue;
                }
                bool valid = !palette.empty();
                if (!valid)
                {
                    diagnostic(
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
                                !id.starts_with("ui.") &&
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
                    diagnostic(
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
                diagnostic("Invalid animation strip: %s", file.c_str());
                continue;
            }
            sprite.texture = texture;
            sprite.shadow = shadows[key];
            sprite.overviewColor = overviewColors[key];
            sprite.materialPixels = materials[key];
            sprite.materialBase = materialBases[key];
            sprite.materialWidth = texture->width();
            sprite.materialHeight = texture->height();
            writeCached(cacheKey, sprite);
            ++rebuilt;
            sprites_[id] = std::move(sprite);
        }
        // Birch is a material variation of the authored tree, not a striped
        // rectangular replacement. Preserve each trunk/branch alpha silhouette
        // and logical dimensions so roots, forks and connecting limbs agree.
        if (palette.contains(0xD7E0E3) && palette.contains(0xAFC9D6) &&
            palette.contains(0x9AA7AF) && palette.contains(0x4E3B39))
        {
            for (const char* part : {"trunk", "branch"})
            {
                for (int variant = 1; variant <= 3; ++variant)
                {
                    const auto suffix = std::string(part) + "." + std::to_string(variant);
                    const auto* original = find("tree." + suffix);
                    if (!original || !original->texture) { continue; }
                    auto birch = *original;
                    auto pixels = original->texture->atlas.pixels;
                    std::unordered_map<unsigned, int> counts;
                    int best = 0;
                    for (auto& c : pixels)
                    {
                        if (!c.alpha) { continue; }
                        // Keep the source's highlight/shadow pattern; no full
                        // width horizontal bands and no brown branch seam.
                        const int value = (3 * int(c.red) + 6 * int(c.green) + c.blue) / 10;
                        const auto alpha = c.alpha;
                        c = value >= 150 ? RenderColor{215, 224, 227, alpha}
                            : value >= 115 ? RenderColor{175, 201, 214, alpha}
                            : value >= 80 ? RenderColor{154, 167, 175, alpha}
                                          : RenderColor{78, 59, 57, alpha};
                        const unsigned rgb = (unsigned(c.red) << 16) |
                                             (unsigned(c.green) << 8) | c.blue;
                        if (++counts[rgb] > best)
                        {
                            best = counts[rgb];
                            birch.materialBase = c;
                        }
                    }
                    const int width = original->texture->width();
                    const int height = original->texture->height();
                    birch.texture = renderer.createTextureFromPixels(width, height, pixels);
                    birch.materialPixels =
                        std::make_shared<const std::vector<RenderColor>>(std::move(pixels));
                    birch.materialWidth = width;
                    birch.materialHeight = height;
                    birch.overviewColor = birch.materialBase;
                    // The silhouette is identical, so its authored shadow is
                    // still valid. All RGB-derived metadata is refreshed above.
                    sprites_["tree.birch-" + suffix] = std::move(birch);
                }
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

    std::string SourceImporter::cacheName(
        const std::filesystem::path& base,
        const std::filesystem::path& settings,
        const std::string& row,
        const std::string& file
    )
    {
        if (cacheRoot.empty())
        {
            return {};
        }
        AssetWriter w;
        w.u32(AssetCompilerVersion);
        w.u32(AssetSchemaVersion);
#ifdef PALADIN_IMPORT_SIGNATURE
        w.text(PALADIN_IMPORT_SIGNATURE);
#endif
        w.text(row);
        for (auto p :
             {base / file,
              settings / "art-palette.hex",
              base / "material-colors.catalog"})
        {
            if (std::filesystem::exists(p))
            {
                w.text(assetDigest(readAssetFile(p)));
            }
            else
            {
                w.text("absent");
            }
        }
        return assetDigest(w.bytes);
    }
    std::optional<ImportedSprite> SourceImporter::readCached(
        const std::string& key
    )
    {
        if (key.empty())
        {
            return {};
        }
        auto p = cacheRoot / (key + ".ddc");
        if (!std::filesystem::exists(p))
        {
            return {};
        }
        try
        {
            auto bytes = readAssetFile(p);
            AssetReader r{bytes};
            auto hash = r.text();
            auto blob = r.bytes.subspan(r.pos);
            if (assetDigest(blob) != hash)
            {
                throw std::runtime_error("Bad cache checksum");
            }
            AssetReader b{blob};
            auto n = b.u32();
            ImportedSprite s;
            static_cast<SpriteAsset&>(s) = decodeSprite(b.take(n));
            n = b.u32();
            s.texture = std::make_shared<ImportedImage>();
            s.texture->atlas = decodeAtlas(b.take(n));
            n = b.u32();
            if (n)
            {
                s.shadow = std::make_shared<ImportedImage>();
                s.shadow->atlas = decodeAtlas(b.take(n));
            }
            b.end();
            return s;
        }
        catch (...)
        {
            return {};
        }
    }
    void SourceImporter::writeCached(
        const std::string& key,
        const ImportedSprite& in
    )
    {
        if (key.empty())
        {
            return;
        }
        auto s = in;
        s.pixelWidth = s.texture->width();
        s.pixelHeight = s.texture->height();
        s.linear = s.texture->atlas.linear;
        AssetWriter w;
        for (auto bytes :
             {encodeSprite(s),
              encodeAtlas(s.texture->atlas),
              s.shadow ? encodeAtlas(s.shadow->atlas) : AssetBytes{}})
        {
            w.u32(unsigned(bytes.size()));
            w.raw(bytes);
        }
        AssetWriter file;
        file.text(assetDigest(w.bytes));
        file.raw(w.bytes);
        writeAssetFile(cacheRoot / (key + ".ddc"), file.bytes);
    }
} // namespace Paladin

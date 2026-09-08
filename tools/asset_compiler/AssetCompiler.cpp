#include "AssetCompiler.h"
#include "SourceImporter.h"
#include "assets/AssetManager.h"
#include "assets/PresentationCodec.h"
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <functional>
#include <iostream>
#include <set>
#include <sstream>
namespace Paladin
{
    void ensureDevelopmentAssets(
        const std::filesystem::path& source,
        const std::filesystem::path& output,
        const std::filesystem::path& cache
    )
    {
        std::vector<std::filesystem::path> files;
        for (auto& e : std::filesystem::recursive_directory_iterator(source))
        {
            if (e.is_regular_file())
            {
                files.push_back(e.path());
            }
        }
        auto config = source / "../../config";
        if (std::filesystem::exists(config))
        {
            for (auto& e : std::filesystem::directory_iterator(config))
            {
                if (e.is_regular_file() &&
                    (e.path().extension() == ".catalog" ||
                     e.path().extension() == ".hex"))
                {
                    files.push_back(e.path());
                }
            }
        }
        std::sort(files.begin(), files.end());
        AssetWriter w;
        w.u32(AssetCompilerVersion);
#ifdef PALADIN_IMPORT_SIGNATURE
        w.text(PALADIN_IMPORT_SIGNATURE);
#endif
        for (auto& f : files)
        {
            w.text(f.generic_string());
            w.text(assetDigest(readAssetFile(f)));
        }
        auto digest = assetDigest(w.bytes);
        auto marker = output / "development.inputs";
        if (std::filesystem::exists(marker) &&
            std::filesystem::exists(output / "assets.manifest"))
        {
            auto b = readAssetFile(marker);
            if (std::string(b.begin(), b.end()) == digest)
            {
                return;
            }
        }
        compileAssets(source, output, cache);
        writeAssetFile(
            marker,
            {reinterpret_cast<const std::uint8_t*>(digest.data()),
             digest.size()}
        );
    }
    static std::string group(std::string_view n)
    {
        if (n.starts_with("world."))
        {
            return "world";
        }
        if (n.starts_with("citizen") || n.starts_with("animal"))
        {
            return "characters";
        }
        if (n.starts_with("ui."))
        {
            return "ui";
        }
        return "settlements";
    }
    static AssetEntry entry(
        std::string name,
        AssetType t,
        AssetBytes b,
        std::vector<AssetId> deps = {}
    )
    {
        name = assetName(name);
        return {
            {assetId(name),
             t,
             0,
             AssetSchemaVersion,
             0,
             0,
             0,
             name,
             std::move(deps)},
            std::move(b)
        };
    }
    AssetCompileStats compileAssets(
        const std::filesystem::path& source,
        const std::filesystem::path& output,
        const std::filesystem::path& cache,
        bool validateOnly
    )
    {
        auto base = source;
        if (std::filesystem::exists(base / "assets/sprites"))
        {
            base /= "assets/sprites";
        }
        auto settings = base;
        if (std::filesystem::exists(base / "../../config/city-objects.catalog"))
        {
            settings = base / "../../config";
        }
        SourceImporter imported;
        imported.cacheRoot = cache;
        ImportImages images;
        imported.load(images, base.string());
        // Check rows that were historically skipped, and duplicate/colliding
        // names.
        std::set<std::string> names;
        std::ifstream catalog(base / "sprites.catalog");
        if (!catalog)
        {
            throw std::runtime_error("Missing sprites.catalog");
        }
        std::string line;
        while (std::getline(catalog, line))
        {
            std::istringstream row(line);
            std::string id;
            if (!(row >> id) || id.starts_with('#'))
            {
                continue;
            }
            if (!names.insert(id).second)
            {
                throw std::runtime_error("Duplicate sprite name: " + id);
            }
            if (!imported.find(id))
            {
                throw std::runtime_error("Invalid/missing sprite: " + id);
            }
        }
        auto checkCount = [&](const std::filesystem::path& p, size_t actual)
        {
            if (!std::filesystem::exists(p))
            {
                return;
            }
            std::ifstream f(p);
            size_t n = 0;
            std::string l;
            while (std::getline(f, l))
            {
                std::istringstream row(l);
                std::string first;
                if (row >> first && !first.starts_with('#'))
                {
                    ++n;
                }
            }
            if (n != actual)
            {
                throw std::runtime_error(
                    "Invalid/duplicate metadata rows: " + p.string()
                );
            }
        };
        checkCount(settings / "pieces.catalog", imported.pieces_.size());
        checkCount(settings / "lights.catalog", imported.lights_.size());
        checkCount(
            settings /
                (settings == base ? "objects.catalog" : "city-objects.catalog"),
            imported.objects_.size()
        );
        // Existing native-size goods icons join the package without scene
        // recoloring.
        const std::pair<const char*, const char*> ui[] = {
            {"stone", "environment-v4/props/resource-stone.png"},
            {"lumber", "environment-v4/props/resource-lumber.png"},
            {"fish", "tribal-v14/fish.png"},
            {"meat", "tribal-v14/meat.png"}
        };
        std::set<unsigned> palette;
        std::ifstream pf(settings / "art-palette.hex");
        std::string hex;
        while (pf >> hex)
        {
            if (hex.size() == 7 && hex[0] == '#')
            {
                palette.insert(std::stoul(hex.substr(1), nullptr, 16));
            }
        }
        if (palette.empty())
        {
            throw std::runtime_error("Missing palette");
        }
        for (auto [name, path] : ui)
        {
            if (!std::filesystem::exists(base / path))
            {
                continue;
            }
            auto key = imported.cacheName(
                base,
                settings,
                std::string("native-ui:") + name,
                path
            );
            if (auto hit = imported.readCached(key))
            {
                imported.sprites_[std::string("ui.goods.") + name] = *hit;
                ++imported.cacheHits;
                continue;
            }
            auto* surface = IMG_Load((base / path).string().c_str());
            if (!surface)
            {
                throw std::runtime_error("Cannot read UI icon");
            }
            ImportedSprite s;
            s.texture = images.createTextureFromSurface(surface, false);
            SDL_DestroySurface(surface);
            for (auto c : s.texture->atlas.pixels)
            {
                if (c.alpha &&
                    (c.alpha != 255 || !palette.contains(
                                           (unsigned(c.red) << 16) |
                                           (unsigned(c.green) << 8) | c.blue
                                       )))
                {
                    throw std::runtime_error("Off-palette UI icon");
                }
            }
            s.width = s.texture->width() / 16.;
            s.height = s.texture->height() / 16.;
            imported.writeCached(key, s);
            ++imported.rebuilt;
            imported.sprites_[std::string("ui.goods.") + name] = std::move(s);
        }
        std::map<std::string, std::vector<AssetEntry>> packages;
        AssetWriter pal;
        pal.u32(unsigned(palette.size()));
        for (auto rgb : palette)
        {
            pal.u32(rgb);
        }
        auto paletteId = assetId("paladin:palette.official");
        packages["core"].push_back(
            entry("palette.official", AssetType::Palette, pal.bytes)
        );
        // Typed compiled recipes. Dependencies reference canonical assets, not
        // paths.
        auto dependency = [&](std::string_view n)
        { return assetId(assetName(n)); };
        for (auto& [id, s] : imported.objects_)
        {
            std::vector<AssetId> deps;
            for (auto& n : {s.floor, s.wall, s.roof, s.sprite})
            {
                if (n != "-" && !n.empty())
                {
                    deps.push_back(dependency(n));
                }
            }
            if (s.decor != "-")
            {
                deps.push_back(dependency("recipe.pieces"));
            }
            packages["settlements"].push_back(entry(
                "presentation." + id,
                AssetType::ObjectPresentation,
                encodePresentation(s),
                deps
            ));
        }
        std::vector<AssetId> partDeps;
        for (auto& p : imported.pieces_)
        {
            partDeps.push_back(dependency(p.sprite));
        }
        std::sort(partDeps.begin(), partDeps.end());
        partDeps.erase(
            std::unique(partDeps.begin(), partDeps.end()),
            partDeps.end()
        );
        packages["settlements"].push_back(entry(
            "recipe.pieces",
            AssetType::BuildingRecipe,
            encodePieces(imported.pieces_),
            partDeps
        ));
        packages["settlements"].push_back(entry(
            "recipe.lights",
            AssetType::Material,
            encodeLights(imported.lights_)
        ));
        struct Image
        {
            std::shared_ptr<ImportedImage> image;
            std::set<AssetId> sources;
            AssetId page = 0;
            unsigned x = 0, y = 0;
        };
        std::map<std::string, std::map<std::string, Image>> groups;
        std::map<std::string, std::pair<std::string, std::string>> placements,
            shadows;
        std::vector<std::string> ordered;
        for (auto& [id, s] : imported.sprites_)
        {
            ordered.push_back(id);
        }
        std::sort(ordered.begin(), ordered.end());
        for (auto& id : ordered)
        {
            auto& s = imported.sprites_.at(id);
            auto sourceId = dependency("source." + id);
            AssetWriter sourceData;
            sourceData.text(assetDigest(encodeAtlas(s.texture->atlas)));
            packages[group(id)].push_back(entry(
                "source." + id,
                AssetType::GenericData,
                sourceData.bytes,
                {paletteId}
            ));
            auto add = [&](std::shared_ptr<ImportedImage> image, bool shadow)
            {
                auto g = group(id) + "." +
                         (shadow ? "shadow"
                                 : (id.find("terrain") != std::string::npos
                                        ? "terrain"
                                        : "features")) +
                         (image->atlas.linear ? ".linear" : ".nearest");
                auto hash = assetDigest(encodeAtlas(image->atlas));
                auto& item = groups[g][hash];
                item.image = image;
                item.sources.insert(sourceId);
                return std::pair{g, hash};
            };
            placements[id] = add(s.texture, false);
            if (s.shadow)
            {
                shadows[id] = add(s.shadow, true);
            }
        }
        size_t pages = 0;
        for (auto& [g, items] : groups)
        {
            std::vector<std::pair<std::string, Image*>> sorted;
            for (auto& [hash, item] : items)
            {
                sorted.push_back({hash, &item});
            }
            std::sort(
                sorted.begin(),
                sorted.end(),
                [](auto& a, auto& b)
                {
                    auto& x = a.second->image->atlas;
                    auto& y = b.second->image->atlas;
                    if (x.height != y.height)
                    {
                        return x.height > y.height;
                    }
                    if (x.width != y.width)
                    {
                        return x.width > y.width;
                    }
                    return a.first < b.first;
                }
            );
            constexpr unsigned side = 1024;
            unsigned page = 0, x = 1, y = 1, row = 0;
            SpriteAtlas atlas{
                side,
                side,
                g.ends_with("linear"),
                std::vector<AssetPixel>(side * side, {0, 0, 0, 0})
            };
            std::set<AssetId> deps;
            auto pageName = [&]
            { return "atlas." + g + "." + std::to_string(page); };
            auto flush = [&]
            {
                if (deps.empty())
                {
                    return;
                }
                packages[g.substr(0, g.find('.'))].push_back(entry(
                    pageName(),
                    AssetType::SpriteAtlas,
                    encodeAtlas(atlas),
                    {deps.begin(), deps.end()}
                ));
                ++pages;
                ++page;
                atlas.pixels.assign(side * side, {0, 0, 0, 0});
                deps.clear();
                x = y = 1;
                row = 0;
            };
            for (auto& [hash, item] : sorted)
            {
                auto& img = item->image->atlas;
                if (img.width + 2 > side || img.height + 2 > side)
                {
                    throw std::runtime_error("Atlas placement overflow: " + g);
                }
                if (x + img.width + 1 > side)
                {
                    x = 1;
                    y += row;
                    row = 0;
                }
                if (y + img.height + 1 > side)
                {
                    flush();
                }
                item->page = dependency(pageName());
                item->x = x;
                item->y = y;
                deps.insert(item->sources.begin(), item->sources.end());
                for (int dy = -1; dy <= int(img.height); ++dy)
                {
                    for (int dx = -1; dx <= int(img.width); ++dx)
                    {
                        atlas.pixels[size_t(int(y) + dy) * side + int(x) + dx] =
                            img.pixels
                                [size_t(
                                     std::clamp(dy, 0, int(img.height) - 1)
                                 ) * img.width +
                                 std::clamp(dx, 0, int(img.width) - 1)];
                    }
                }
                x += img.width + 2;
                row = std::max(row, img.height + 2);
            }
            flush();
        }
        for (auto& id : ordered)
        {
            auto s = imported.sprites_.at(id);
            auto [g, key] = placements.at(id);
            auto& image = groups.at(g).at(key);
            s.atlas = image.page;
            s.x = image.x;
            s.y = image.y;
            s.pixelWidth = s.texture->width();
            s.pixelHeight = s.texture->height();
            s.linear = s.texture->atlas.linear;
            std::vector<AssetId> deps{
                s.atlas,
                paletteId,
                dependency("source." + id)
            };
            if (shadows.contains(id))
            {
                auto [sg, sk] = shadows.at(id);
                auto& sh = groups.at(sg).at(sk);
                s.shadowAtlas = sh.page;
                s.shadowX = sh.x;
                s.shadowY = sh.y;
                s.shadowWidth = s.shadow->width();
                s.shadowHeight = s.shadow->height();
                deps.push_back(sh.page);
            }
            packages[group(id)].push_back(entry(
                id,
                id.starts_with("ui.") ? AssetType::UiAsset : AssetType::Sprite,
                encodeSprite(s),
                deps
            ));
        }
        // Validate direct/reverse edges and cycles before emitting any package.
        std::map<AssetId, const AssetRecord*> records;
        std::map<AssetId, std::vector<AssetId>> reverse;
        for (auto& [g, entries] : packages)
        {
            for (auto& e : entries)
            {
                auto [it, ok] = records.emplace(e.record.id, &e.record);
                if (!ok)
                {
                    throw std::runtime_error(
                        "AssetId collision/duplicate: " + e.record.name +
                        " / " + it->second->name
                    );
                }
            }
        }
        std::map<AssetId, int> colors;
        std::function<void(AssetId)> visit = [&](AssetId id)
        {
            if (colors[id] == 1)
            {
                throw std::runtime_error(
                    "Illegal asset dependency cycle: " + records.at(id)->name
                );
            }
            if (colors[id] == 2)
            {
                return;
            }
            colors[id] = 1;
            for (auto d : records.at(id)->dependencies)
            {
                if (!records.contains(d))
                {
                    throw std::runtime_error(
                        "Missing dependency referenced by " +
                        records.at(id)->name + ": " + std::to_string(d)
                    );
                }
                reverse[d].push_back(id);
                visit(d);
            }
            colors[id] = 2;
        };
        for (auto& [id, record] : records)
        {
            visit(id);
        }
        AssetCompileStats
            stats{names.size(), imported.cacheHits, imported.rebuilt, pages, 0};
        std::ostringstream manifest;
        manifest << "PALADIN ASSET MANIFEST v1\n";
        for (auto& [g, entries] : packages)
        {
            auto bytes = AssetPackage::write(entries);
            stats.bytes += bytes.size();
            manifest << g << ".palpak " << assetDigest(bytes) << " "
                     << bytes.size() << "\n";
            std::sort(
                entries.begin(),
                entries.end(),
                [](auto& a, auto& b) { return a.record.name < b.record.name; }
            );
            for (auto& e : entries)
            {
                manifest << e.record.name << " " << e.record.id
                         << " type=" << unsigned(e.record.type)
                         << " dependencies=";
                for (auto d : e.record.dependencies)
                {
                    manifest << d << ",";
                }
                manifest << " dependents=" << reverse[e.record.id].size()
                         << "\n";
            }
            if (!validateOnly)
            {
                writeAssetFile(output / (g + ".palpak"), bytes);
            }
        }
        if (!validateOnly)
        {
            // Only remove obsolete packages named in this compiler's previous
            // manifest.
            std::ifstream previous(output / "assets.manifest");
            std::string oldLine;
            while (std::getline(previous, oldLine))
            {
                std::istringstream row(oldLine);
                std::string filename;
                row >> filename;
                auto path = std::filesystem::path(filename);
                if (path.filename() == path && path.extension() == ".palpak" &&
                    !packages.contains(path.stem().string()))
                {
                    std::filesystem::remove(output / path);
                }
            }
            auto text = manifest.str();
            writeAssetFile(
                output / "assets.manifest",
                {reinterpret_cast<const std::uint8_t*>(text.data()),
                 text.size()}
            );
            AssetManager verify;
            verify.mountDirectory(output);
        }
        return stats;
    }
} // namespace Paladin

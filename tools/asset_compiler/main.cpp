#include "AssetCompiler.h"
#include "assets/AssetManager.h"
#include <algorithm>
#include <iostream>
namespace fs = std::filesystem;
int main(int argc, char** argv)
{
    using namespace Paladin;
    try
    {
        if (argc < 3)
        {
            throw std::runtime_error(
                "Usage: PaladinAssetCompiler compile <source> <output> | "
                "validate <source> | list/stats <package-or-directory> | "
                "inspect <package> <name-or-id> | clean <cache-root>"
            );
        }
        std::string cmd = argv[1];
        fs::path path = argv[2];
        if (cmd == "compile" || cmd == "validate")
        {
            if (cmd == "compile" && argc < 4)
            {
                throw std::runtime_error("Output directory required");
            }
            auto root = fs::absolute(path);
            auto cache = root / ".cache/assets";
            if (root.filename() == "sprites")
            {
                cache = root / "../../.cache/assets";
            }
            auto s = compileAssets(
                root,
                argc > 3 ? fs::path(argv[3]) : fs::path{},
                cache,
                cmd == "validate"
            );
            std::cout << "sources=" << s.sources << " cache_hits=" << s.hits
                      << " rebuilt=" << s.rebuilt << " atlas_pages=" << s.pages
                      << " output_bytes=" << s.bytes
                      << " errors=0 warnings=0\n";
            return 0;
        }
        if (cmd == "clean")
        {
            auto target = fs::weakly_canonical(path);
            if (!fs::is_directory(target))
            {
                return 0;
            }
            size_t n = 0;
            for (auto& e : fs::directory_iterator(target))
            {
                if (e.is_regular_file() && e.path().extension() == ".ddc" &&
                    e.path().stem().string().size() == 64)
                {
                    fs::remove(e.path());
                    ++n;
                }
            }
            std::cout << "Removed " << n << " compiler cache entries\n";
            return 0;
        }
        std::vector<std::shared_ptr<AssetPackage>> packs;
        if (fs::is_directory(path))
        {
            std::vector<fs::path> files;
            for (auto& e : fs::directory_iterator(path))
            {
                if (e.path().extension() == ".palpak")
                {
                    files.push_back(e.path());
                }
            }
            std::sort(files.begin(), files.end());
            for (auto& p : files)
            {
                packs.push_back(AssetPackage::read(p));
            }
        }
        else
        {
            packs.push_back(AssetPackage::read(path));
        }
        size_t total = 0, count = 0;
        bool found = false;
        for (auto& p : packs)
        {
            total += fs::file_size(p->path);
            count += p->records().size();
            for (auto& [id, r] : p->records())
            {
                bool show = cmd == "list";
                if (cmd == "inspect")
                {
                    if (argc < 4)
                    {
                        throw std::runtime_error(
                            "Asset name or decimal ID required"
                        );
                    }
                    show = r.name == assetName(argv[3]) ||
                           std::to_string(id) == argv[3];
                }
                if (show)
                {
                    found = true;
                    std::cout << r.name << " id=" << id
                              << " type=" << unsigned(r.type)
                              << " bytes=" << r.uncompressedSize
                              << " dependencies=";
                    for (auto d : r.dependencies)
                    {
                        std::cout << d << ",";
                    }
                    std::cout << "\n";
                }
            }
        }
        if (cmd == "stats")
        {
            std::cout << "packages=" << packs.size() << " assets=" << count
                      << " bytes=" << total << "\n";
        }
        else if (cmd == "inspect" && !found)
        {
            throw std::runtime_error("Asset not found");
        }
        else if (cmd != "list" && cmd != "inspect")
        {
            throw std::runtime_error("Unknown command");
        }
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Asset error: " << e.what() << "\n";
        return 1;
    }
}

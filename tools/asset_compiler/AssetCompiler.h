#pragma once
#include "assets/AssetPackage.h"
namespace Paladin {
struct AssetCompileStats {size_t sources=0,hits=0,rebuilt=0,pages=0,bytes=0;};
void ensureDevelopmentAssets(const std::filesystem::path&,const std::filesystem::path&,const std::filesystem::path&);
AssetCompileStats compileAssets(const std::filesystem::path& source,const std::filesystem::path& output,const std::filesystem::path& cache,bool validateOnly=false);
}

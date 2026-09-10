#pragma once
#include "assets/AssetTypes.h"
#include <bit>
#include <span>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <limits>
#include <cmath>
namespace Paladin {
using AssetBytes=std::vector<std::uint8_t>;
struct AssetWriter {
 AssetBytes bytes;
 void u8(std::uint8_t v){bytes.push_back(v);} void u32(std::uint32_t v){for(int i=0;i<4;++i)u8(std::uint8_t(v>>(i*8)));} void u64(std::uint64_t v){for(int i=0;i<8;++i)u8(std::uint8_t(v>>(i*8)));}
 void real(double v){u64(std::bit_cast<std::uint64_t>(v));}
 void raw(std::span<const std::uint8_t> b){bytes.insert(bytes.end(),b.begin(),b.end());}
 void text(std::string_view s){u32(unsigned(s.size()));raw({reinterpret_cast<const std::uint8_t*>(s.data()),s.size()});}
 void color(AssetPixel c){u8(c.red);u8(c.green);u8(c.blue);u8(c.alpha);}
};
struct AssetReader {
 std::span<const std::uint8_t> bytes;std::size_t pos=0;
 std::span<const std::uint8_t> take(std::size_t n){if(n>bytes.size()-pos)throw std::runtime_error("Truncated asset data / invalid offset");auto s=bytes.subspan(pos,n);pos+=n;return s;}
 std::uint8_t u8(){return take(1)[0];} std::uint32_t u32(){std::uint32_t v=0;for(int i=0;i<4;++i)v|=std::uint32_t(u8())<<(i*8);return v;} std::uint64_t u64(){std::uint64_t v=0;for(int i=0;i<8;++i)v|=std::uint64_t(u8())<<(i*8);return v;}
 double real(){auto v=std::bit_cast<double>(u64());if(!std::isfinite(v))throw std::runtime_error("Non-finite asset metadata");return v;}
 std::string text(){auto n=u32();if(n>1048576)throw std::runtime_error("Asset string too large");auto s=take(n);return {reinterpret_cast<const char*>(s.data()),s.size()};}
 AssetPixel color(){auto r=u8(),g=u8(),b=u8(),a=u8();return {r,g,b,a};}
 void end(){if(pos!=bytes.size())throw std::runtime_error("Unexpected asset payload fields");}
};
inline AssetBytes readAssetFile(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("Cannot read "+p.string());auto n=f.tellg();if(n<0 || n>std::streamoff(1024ull*1024*1024))throw std::runtime_error("Invalid asset file size");AssetBytes b(static_cast<size_t>(n));f.seekg(0);if(!b.empty()&&!f.read(reinterpret_cast<char*>(b.data()),n))throw std::runtime_error("Asset read failed");return b;}
inline void writeAssetFile(const std::filesystem::path& p,std::span<const std::uint8_t> b){std::filesystem::create_directories(p.parent_path());auto tmp=p;tmp += ".tmp";{std::ofstream f(tmp,std::ios::binary|std::ios::trunc);f.write(reinterpret_cast<const char*>(b.data()),b.size());if(!f)throw std::runtime_error("Asset write failed: "+p.string());} if(std::filesystem::exists(p)&&readAssetFile(p)==AssetBytes(b.begin(),b.end())){std::filesystem::remove(tmp);return;} std::error_code removeError;std::filesystem::remove(p,removeError);if(removeError){std::ofstream f(p,std::ios::binary|std::ios::trunc);f.write(reinterpret_cast<const char*>(b.data()),b.size());if(!f)throw std::runtime_error("Asset replace failed: "+p.string()+": "+removeError.message());f.close();std::filesystem::remove(tmp);return;} std::filesystem::rename(tmp,p);}
std::string assetDigest(std::span<const std::uint8_t>);
inline std::string assetDigest(std::string_view s){return assetDigest(std::span(reinterpret_cast<const std::uint8_t*>(s.data()),s.size()));}
AssetBytes encodeSprite(const SpriteAsset&);SpriteAsset decodeSprite(std::span<const std::uint8_t>);
AssetBytes encodeAtlas(const SpriteAtlas&);SpriteAtlas decodeAtlas(std::span<const std::uint8_t>);
}

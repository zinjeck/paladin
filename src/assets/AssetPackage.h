#pragma once
#include "assets/AssetBinary.h"
#include <map>
namespace Paladin {
struct AssetEntry {AssetRecord record;AssetBytes data;};
class AssetPackage {
 AssetBytes bytes_;std::map<AssetId,AssetRecord> records_;
 public:std::string identity;std::filesystem::path path;
 static std::shared_ptr<AssetPackage> read(const std::filesystem::path&);
 static AssetBytes write(std::vector<AssetEntry>);
 const auto& records() const{return records_;}
 std::span<const std::uint8_t> data(AssetId id) const {const auto& r=records_.at(id);return std::span(bytes_).subspan(size_t(r.offset),size_t(r.uncompressedSize));}
};
}

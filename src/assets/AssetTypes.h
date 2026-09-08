#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
namespace Paladin {
struct AssetPixel { std::uint8_t red=255,green=255,blue=255,alpha=255; bool operator==(const AssetPixel&) const = default; };
struct AssetRectangle { float x=0,y=0,width=0,height=0; };
using AssetId=std::uint64_t;
inline constexpr unsigned AssetFormatVersion=1,AssetSchemaVersion=1,AssetCompilerVersion=1;
inline AssetId assetId(std::string_view name) { std::uint64_t h=14695981039346656037ull; for(unsigned char c:name){ h^=c;h*=1099511628211ull;} return h; }
inline std::string assetName(std::string_view name) { return name.find(':')==std::string_view::npos ? "paladin:"+std::string(name) : std::string(name); }
enum class AssetType:std::uint32_t { Sprite=1,SpriteAnimation,SpriteAtlas,Palette,Material,ObjectPresentation,BuildingRecipe,Font,Sound,Music,UiAsset,Localization,GenericData };
enum class AssetState { Unloaded,Requested,Loading,Resident,Failed };
struct AssetHandle { AssetId id=0;std::uint64_t generation=0;AssetType type=AssetType::Sprite; };
struct AssetRecord { AssetId id=0;AssetType type=AssetType::GenericData;std::uint32_t flags=0,schema=AssetSchemaVersion;std::uint64_t offset=0,compressedSize=0,uncompressedSize=0;std::string name;std::vector<AssetId> dependencies; };
struct SpriteAsset {
 AssetId atlas=0,shadowAtlas=0; unsigned x=0,y=0,pixelWidth=0,pixelHeight=0,shadowX=0,shadowY=0,shadowWidth=0,shadowHeight=0;
 double width=1,height=1,pivotX=.5,pivotY=1,elevation=0; int frames=1;double fps=0;
 AssetRectangle doorRegion{};AssetPixel overviewColor{0,0,0,0},materialBase{};
 int materialWidth=0,materialHeight=0;bool linear=false;
 std::shared_ptr<const std::vector<AssetPixel>> materialPixels;
};
struct SpriteAtlas { unsigned width=0,height=0;bool linear=false;std::vector<AssetPixel> pixels; };
}

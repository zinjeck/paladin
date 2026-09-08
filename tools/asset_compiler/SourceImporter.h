#pragma once
#include "assets/AssetBinary.h"
#include "assets/PresentationData.h"
#include <SDL3/SDL.h>
#include <unordered_map>
#include <optional>
#include <cstdio>
namespace Paladin {
struct ImportedImage { SpriteAtlas atlas; int width()const{return int(atlas.width);}int height()const{return int(atlas.height);} };
struct ImportedSprite:SpriteAsset {std::shared_ptr<ImportedImage> texture,shadow;};
struct ImportImages {
 std::shared_ptr<ImportedImage> createTextureFromPixels(int w,int h,std::span<const AssetPixel> pixels){auto p=std::make_shared<ImportedImage>();p->atlas={unsigned(w),unsigned(h),false,{pixels.begin(),pixels.end()}};return p;}
 std::shared_ptr<ImportedImage> createTextureFromSurface(SDL_Surface* s,bool linear){auto* rgba=SDL_ConvertSurface(s,SDL_PIXELFORMAT_RGBA32);if(!rgba)throw std::runtime_error("Cannot decode image surface");std::vector<AssetPixel> pixels;pixels.reserve(size_t(rgba->w)*rgba->h);for(int y=0;y<rgba->h;++y){auto* row=reinterpret_cast<const AssetPixel*>(static_cast<const unsigned char*>(rgba->pixels)+y*rgba->pitch);pixels.insert(pixels.end(),row,row+rgba->w);}auto p=createTextureFromPixels(rgba->w,rgba->h,pixels);p->atlas.linear=linear;SDL_DestroySurface(rgba);return p;}
};
class SourceImporter {
 bool loaded_=false;bool strict_;
 public:
 std::filesystem::path cacheRoot;std::size_t cacheHits=0,rebuilt=0;
 std::unordered_map<std::string,ImportedSprite> sprites_;
 std::unordered_map<std::string,ObjectPresentation> objects_;
 std::vector<BuildingPiece> pieces_;std::vector<BlueprintLight> lights_;
 explicit SourceImporter(bool strict=true):strict_(strict){}
 void load(ImportImages&,const std::string&);
 const ImportedSprite* find(const std::string& id)const {auto i=sprites_.find(id);return i==sprites_.end()?nullptr:&i->second;}
 template<class... A> void diagnostic(const char* fmt,A... a){char msg[2048];std::snprintf(msg,sizeof(msg),fmt,a...);if(strict_)throw std::runtime_error(msg);SDL_Log("%s",msg);}
 std::string cacheName(const std::filesystem::path&,const std::filesystem::path&,const std::string&,const std::string&);
 std::optional<ImportedSprite> readCached(const std::string&);
 void writeCached(const std::string&,const ImportedSprite&);
};
}

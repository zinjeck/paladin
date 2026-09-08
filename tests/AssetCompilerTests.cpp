#include "tools/asset_compiler/AssetCompiler.h"
#include "assets/AssetManager.h"
#include "TestFramework.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <iostream>
#include <algorithm>
#include <fstream>
using namespace Paladin;
int main(){try{
 PALADIN_CHECK(assetDigest(std::string_view("abc"))=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
 auto root=std::filesystem::path(SDL_GetBasePath())/"asset-compiler-fixture";std::filesystem::create_directories(root);
 std::ofstream(root/"art-palette.hex")<<"#FF0000\n#00FF00\n";
 std::ofstream(root/"sprites.catalog")<<"terrain.fixture tile.png .125 .125 .5 1 0 0\n";
 auto* surface=SDL_CreateSurface(2,2,SDL_PIXELFORMAT_RGBA32);PALADIN_CHECK(surface);
 const AssetPixel expected[]={{255,0,0,255},{0,255,0,255},{0,0,0,0},{255,0,0,255}};
 for(int y=0;y<2;++y)std::copy_n(expected+y*2,2,reinterpret_cast<AssetPixel*>(static_cast<unsigned char*>(surface->pixels)+y*surface->pitch));
 PALADIN_CHECK(IMG_SavePNG(surface,(root/"tile.png").string().c_str()));
 auto first=compileAssets(root,root/"one",root/"cache");auto second=compileAssets(root,root/"two",root/"cache");PALADIN_CHECK(second.hits>=1);
 for(auto& e:std::filesystem::directory_iterator(root/"one"))PALADIN_CHECK(readAssetFile(e.path())==readAssetFile(root/"two"/e.path().filename()));
 AssetManager manager;manager.mountDirectory(root/"one");auto id=assetId("paladin:terrain.fixture");auto sprite=decodeSprite(manager.data(id));auto atlas=decodeAtlas(manager.data(sprite.atlas));
 PALADIN_CHECK(sprite.pixelWidth==2&&sprite.pixelHeight==2&&sprite.width==.125&&sprite.pivotX==.5&&sprite.shadowAtlas&&sprite.materialPixels&&sprite.materialPixels->size()==4);
 for(unsigned y=0;y<2;++y)for(unsigned x=0;x<2;++x)PALADIN_CHECK(atlas.pixels[(sprite.y+y)*atlas.width+sprite.x+x]==expected[y*2+x]);
 auto h=manager.request(id,AssetType::Sprite);PALADIN_CHECK(manager.valid(h)&&!manager.uploads.empty());manager.mount(root/"two/settlements.palpak",10);PALADIN_CHECK(!manager.valid(h));manager.validateDependencies();
 auto bytes=readAssetFile(root/"one/core.palpak");bytes[8]=99;writeAssetFile(root/"bad.palpak",bytes);bool rejected=false;try{AssetPackage::read(root/"bad.palpak");}catch(...){rejected=true;}PALADIN_CHECK(rejected);
 reinterpret_cast<AssetPixel*>(surface->pixels)[0]={1,2,3,255};PALADIN_CHECK(IMG_SavePNG(surface,(root/"tile.png").string().c_str()));SDL_DestroySurface(surface);rejected=false;try{compileAssets(root,root/"bad",root/"cache");}catch(...){rejected=true;}PALADIN_CHECK(rejected);
 std::cout<<"Asset round-trip, pixel equivalence, cache/determinism, layering, generation and rejection checks passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}

#pragma once
#include "assets/AssetManager.h"
#include "rendering/Renderer.h"
#include "rendering/Texture.h"
namespace Paladin {
inline void uploadAssets(Renderer& renderer,AssetManager& manager){while(!manager.uploads.empty()){auto job=manager.uploads.pop();if(!manager.valid(job.handle))continue;try{std::shared_ptr<Texture> texture=renderer.createTextureFromPixels(job.image.width,job.image.height,job.image.pixels);if(!texture)throw std::runtime_error("Asset atlas upload failed");renderer.setTextureFiltering(*texture,job.image.linear);manager.completeUpload(job.handle,texture,job.image.pixels.size()*4);}catch(...){manager.failed(job.handle);throw;}}}
inline std::shared_ptr<Texture> assetTextureView(Renderer& renderer,AssetManager& manager,AssetId page,unsigned x,unsigned y,unsigned w,unsigned h){auto* s=manager.residency(page);if(!s||s->state!=AssetState::Resident)throw std::runtime_error("Atlas is not resident");return renderer.createTextureView(std::static_pointer_cast<Texture>(s->resource),int(x),int(y),int(w),int(h));}
}

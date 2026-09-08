#pragma once
#include "assets/AssetPackage.h"
#include <deque>
#include <atomic>
namespace Paladin {
struct AssetUpload {AssetHandle handle;SpriteAtlas image;};
class AssetUploadQueue {std::deque<AssetUpload> jobs_; public:void push(AssetUpload j){jobs_.push_back(std::move(j));}bool empty()const{return jobs_.empty();}AssetUpload pop(){auto j=std::move(jobs_.front());jobs_.pop_front();return j;}void clear(){jobs_.clear();}};
struct AssetResidency {AssetState state=AssetState::Unloaded;std::uint64_t generation=0,lastUsed=0;size_t cpuBytes=0,gpuBytes=0;unsigned requests=0;int priority=0;std::string package;std::shared_ptr<void> resource;};
class AssetManager {
 struct Mount {std::shared_ptr<AssetPackage> package;int priority;};
 struct Resolved {std::shared_ptr<AssetPackage> package;AssetRecord record;int priority;};
 std::vector<Mount> mounts_;std::map<AssetId,Resolved> registry_;std::map<AssetId,AssetResidency> residency_;std::uint64_t generation_=0,stamp_=0;
 inline static std::atomic<std::uint64_t> nextGeneration_{0};
 public:AssetUploadQueue uploads;
 void mount(const std::filesystem::path&,int priority=0);
 void mountDirectory(const std::filesystem::path&,int priority=0);
 void validateDependencies()const;
 std::vector<AssetRecord> records()const;
 const AssetRecord* resolve(AssetId)const;
 std::span<const std::uint8_t> data(AssetId)const;
 AssetHandle request(AssetId,AssetType);
 bool valid(AssetHandle h)const;
 const AssetResidency* residency(AssetId id)const;
 bool isResident(AssetHandle h)const;
 void completeUpload(AssetHandle,std::shared_ptr<void>,size_t gpuBytes=0);
 void failed(AssetHandle);
 void release(AssetHandle);
 size_t residentGpuBytes()const;
};
}

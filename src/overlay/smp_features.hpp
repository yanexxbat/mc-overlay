#pragma once
#include "esp_geometry.hpp"
#include "soft_glow.hpp"
#include <algorithm>
#include <array>
#include <span>

namespace mcov::smp {
inline constexpr std::array<std::uint8_t,13> kStorageKinds={kBlockChest,kBlockTrappedChest,kBlockEnderChest,
    kBlockShulker,kBlockBarrel,kBlockHopper,kBlockFurnace,kBlockBlastFurnace,kBlockSmoker,
    kBlockDispenser,kBlockDropper,kBlockCrafter,kBlockBrewingStand};
inline constexpr std::uint32_t kStorageMask=[] { std::uint32_t mask=0; for(auto kind:kStorageKinds) mask|=1u<<kind; return mask; }();
inline constexpr std::uint32_t kStashDefaultMask=(1u<<kBlockChest)|(1u<<kBlockTrappedChest)|(1u<<kBlockShulker)|(1u<<kBlockBarrel);
inline bool StorageKind(std::uint8_t kind) { return kind<kBlockKindCount&&(kStorageMask&(1u<<kind)); }
struct StorageSettings {
    std::uint32_t mask=kStorageMask;
    bool labels=false,tracers=false;
    std::array<ImVec4,kStorageKinds.size()> colours={ImVec4{.92f,.68f,.28f,1},ImVec4{1,.35f,.25f,1},ImVec4{.25f,.9f,.75f,1},
        ImVec4{.78f,.5f,1,1},ImVec4{.76f,.55f,.3f,1},ImVec4{.65f,.7f,.8f,1},ImVec4{1,.58f,.3f,1},
        ImVec4{.8f,.8f,.85f,1},ImVec4{.8f,.55f,.4f,1},ImVec4{.55f,.7f,1,1},ImVec4{.45f,.85f,1,1},
        ImVec4{.6f,1,.45f,1},ImVec4{1,.5f,.7f,1}};
    ImVec4 Colour(std::uint8_t kind) const {
        const auto it=std::find(kStorageKinds.begin(),kStorageKinds.end(),kind);
        return it==kStorageKinds.end()?ImVec4{1,1,1,1}:colours[it-kStorageKinds.begin()];
    }
};
struct StashSettings {
    bool enabled=false,labels=true,tracers=false,notifications=true;
    int radius=32,interval=1000,minContainers=8;
    float maxDraw=128;
    std::uint32_t mask=kStashDefaultMask;
    ImVec4 colour{1,.55f,.22f,.95f};
};
struct SpawnerSettings {
    bool enabled=false,labels=true,tracers=false;
    int radius=24,interval=1000;
    float maxDraw=64,glow=.75f;
    ImVec4 colour{.4f,1,.65f,.95f};
};
struct Config {
    StorageSettings storage{};
    StashSettings stash{};
    SpawnerSettings spawner{};
    void Clamp();
};
struct Stash {
    int chunkX=0,chunkZ=0,count=0;
    WorldPoint centre{};
    WorldBounds bounds{};
};
// Session discoveries, isolated by server and dimension. A finished sweep is
// consumed once. The bounded history survives walking out of scan range.
class StashFinder {
public:
    static constexpr int kMaxFinds=128;
    void Reset();
    void Clear() { count_=0; }
    int Update(const WorldSnapshot& snapshot,const StashSettings& settings);
    std::span<const Stash> Finds() const { return {finds_.data(),static_cast<std::size_t>(count_)}; }
    bool Full() const { return count_==kMaxFinds; }
    const Stash* Latest() const { return count_?&finds_[count_-1]:nullptr; }
private:
    std::array<Stash,kMaxFinds> finds_{};
    std::array<char,kNameLen> dimension_{};
    std::array<char,kAddrLen> server_{};
    std::uint64_t scan_=0;
    std::uint32_t mask_=0;
    int count_=0,minimum_=0;
};
void DrawBlocks(ImDrawList& draw,const WorldSnapshot& snapshot,const CameraProjection& projection,
                const Config& config,bool storageEnabled,float storageRange,float opacity,float glow,
                ImVec2 viewport,ImTextureID glowTexture=0);
void DrawStashes(ImDrawList& draw,const WorldSnapshot& snapshot,const CameraProjection& projection,
                 const StashSettings& settings,std::span<const Stash> finds,ImVec2 viewport);
} // namespace mcov::smp

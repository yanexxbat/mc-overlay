#pragma once
#include "esp_geometry.hpp"
#include "soft_glow.hpp"
#include "../common/base_render_state.hpp"
#include "../common/block_scan_results.hpp"
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
    int interval=1000,pulseDuration=1500;
    bool pulse=true;
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
struct BaseSettings {
    bool enabled=false,labels=true,tracers=false,textured=true;
    float textureOpacity=1;
    float maxDraw=192,padding=4,fillOpacity=.12f,lineWidth=1.8f;
    ImVec4 colour{.35f,.85f,1,.95f};
};
struct BaseFinderSettings {
    bool enabled=false,kelp=true,amethyst=true,labels=true,tracers=false;
    // Deepslate is supplied by Chunkfinder; old standalone growth profiles keep their schema.
    bool deepslate=false;
    int minDeepslate=1;
    int radius=32,interval=1000,minKelpHeight=18,minKelpColumns=3,minAmethyst=6;
    float maxDraw=192;
    ImVec4 colour{1,.75f,.3f,.95f};
    std::uint32_t Mask() const {
        return (kelp?1u<<kBlockKelp:0u)|(amethyst?(1u<<kBlockLargeAmethystBud)|(1u<<kBlockAmethystCluster):0u)|
            (deepslate?1u<<kBlockRotatedDeepslate:0u);
    }
};
struct Config {
    StorageSettings storage{};
    StashSettings stash{};
    SpawnerSettings spawner{};
    BaseSettings base{};
    BaseFinderSettings finder{};
    StashSettings Detection() const { auto c=stash; c.enabled=stash.enabled||base.enabled; return c; }
    void Clamp();
};
struct StorageMarker {
    BlockSnapshot block{};
    float alpha=0;
    double revealAt=0;
    bool present=false;
};
struct StorageHighlight { float strength=0,height=0; };
// Bounded presentation history: progressive discovery, steady known boxes and
// a short fade when a completed sweep no longer contains a container.
class StoragePulse {
    std::vector<StorageMarker> markers_;
    std::unordered_map<BlockPosition,std::size_t,BlockPositionHash> positions_;
    std::array<char,kNameLen> dimension_{};
    std::array<char,kAddrLen> server_{};
    std::uint64_t scan_=0;
    std::uint32_t mask_=0;
    double previous_=0,epoch_=0,nextReveal_=0;
public:
    StoragePulse() { markers_.reserve(kMaxBlocks); positions_.reserve(kMaxBlocks); }
    void Reset();
    void Update(const WorldSnapshot& snapshot,const StorageSettings& settings,bool enabled,float range,double now,bool reducedMotion=false);
    std::span<const StorageMarker> Markers() const { return markers_; }
    StorageHighlight Highlight(std::size_t index,const StorageSettings& settings,double now,bool reducedMotion=false) const;
};
struct PossibleBase {
    int chunkX=0,chunkZ=0,kelpColumns=0,tallestKelp=0,amethyst=0,deepslate=0;
    WorldPoint centre{};
    WorldBounds bounds{};
};
class BaseFinder {
public:
    static constexpr int kMaxFinds=128;
    void Reset() { *this=BaseFinder{}; }
    void Clear() { count_=0; }
    void Update(const WorldSnapshot& snapshot,const BaseFinderSettings& settings);
    std::span<const PossibleBase> Finds() const { return {finds_.data(),static_cast<std::size_t>(count_)}; }
    bool Matches(const WorldSnapshot& s) const { return !std::strncmp(dimension_.data(),s.dimension,kNameLen)&&!std::strncmp(server_.data(),s.serverAddress,kAddrLen); }
private:
    std::array<PossibleBase,kMaxFinds> finds_{};
    std::array<char,kNameLen> dimension_{};
    std::array<char,kAddrLen> server_{};
    std::uint64_t scan_=0;
    std::uint32_t mask_=0;
    int count_=0,height_=0,columns_=0,amethyst_=0,radius_=0,deepslate_=0;
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
    bool Matches(const WorldSnapshot& s) const { return !std::strncmp(dimension_.data(),s.dimension,kNameLen)&&!std::strncmp(server_.data(),s.serverAddress,kAddrLen); }
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
                ImVec2 viewport,ImTextureID glowTexture=0,const StoragePulse* pulse=nullptr,double now=0,bool reducedMotion=false);
void DrawStashes(ImDrawList& draw,const WorldSnapshot& snapshot,const CameraProjection& projection,
                 const StashSettings& settings,std::span<const Stash> finds,ImVec2 viewport);
void DrawBases(ImDrawList& draw,const WorldSnapshot& snapshot,const CameraProjection& projection,
               const BaseSettings& settings,std::span<const Stash> finds,ImVec2 viewport);
void DrawPossibleBases(ImDrawList& draw,const WorldSnapshot& snapshot,const CameraProjection& projection,
                       const BaseFinderSettings& settings,std::span<const PossibleBase> finds,ImVec2 viewport,
                       bool chunks=true,bool blocks=true);
WorldBounds PossibleChunkBounds(const PossibleBase& find,const WorldSnapshot& snapshot);
base::Request BuildBaseRequest(const WorldSnapshot& snapshot,const BaseSettings& settings,
                              std::span<const Stash> finds,bool fresh,std::uint64_t now);
} // namespace mcov::smp

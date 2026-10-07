#pragma once
#include "world_visuals.hpp"
#include "keybinds.hpp"
#include "hud_layout.hpp"
#include "../combat/target_selector.hpp"
#include <array>
#include <span>
#include <utility>

namespace mcov::rockstar {
// Eigene C++-Implementierungen der passenden Verhaltensweisen; kein Java-Port im Render-Thread.
enum class Feature { JumpCircles, Wings, Halo, GlowEsp, DamageMarkers, DamageInfo, TargetCircle,
    Crosshair, Arrows, NameProtect, HitSounds, ModuleSounds, MiddleFriend, DeathCoordinates,
    DetectPlayer, Notifications, ChestEsp, BeaconRadius, ChunkBorders, Waypoints, Count };
inline constexpr int kCount=static_cast<int>(Feature::Count);
inline constexpr int Index(Feature f) { return static_cast<int>(f); }
inline constexpr std::array<const char*,kCount> kNames={"Jump circles","Wings","Halo / Nimb","Glow ESP",
    "Damage markers","Damage vignette","Target circle","Crosshair","Offscreen arrows","Name protect",
    "Hit sounds","Module sounds","Middle click friend","Death coordinates","Detect players",
    "Notifications","Storage ESP","Beacon radius","Chunk borders","Waypoints"};
struct Options {
    bool enabled=false,auraColours=true;
    int mode=0;
    float range=64,size=1,lifetime=1.2f,speed=1,opacity=.8f,glow=.75f;
    ImVec4 start{.66f,.42f,1,1},end{.05f,1,.88f,1};
};
struct Config {
    std::array<Options,kCount> modules{};
    Config();
    Options& operator[](Feature f) { return modules[Index(f)]; }
    const Options& operator[](Feature f) const { return modules[Index(f)]; }
    bool Enabled(Feature f) const { return (*this)[f].enabled; }
    void Clamp();
};
struct Marker {
    bool used=false,death=false;
    WorldPoint p{};
    std::array<char,kNameLen> dimension{},label{};
    std::array<char,kAddrLen> server{};
};
struct Stats { float speed=0,verticalSpeed=0,received=0,observedDamage=0; double seconds=0,distance=0; int targetDeaths=0,leftCps=0,rightCps=0; };
class Pack final {
public:
    static constexpr int kMarkers=16,kDamage=96,kRings=32,kNotices=8;
    void ResetWorld();
    void Update(const WorldSnapshot& s,double now,const Config& c,std::uint32_t target);
    void Input(const keys::Input& input,bool allowed,double now);
    void Notify(const char* text,double now);
    bool AddMarker(const WorldSnapshot& s,const char* name,bool death=false);
    void ClearMarkers() { markers_={}; }
    auto& Markers() { return markers_; }
    const auto& Markers() const { return markers_; }
    Stats Metrics(double now) const;
    int DamageCount(double now,float lifetime) const;
    int RingCount(double now,float lifetime) const;
    bool TakeHitSound() { return std::exchange(hitSound_,false); }
    void DrawWorld(ImDrawList& draw,const WorldSnapshot& s,const CameraProjection& p,const Config& c,
                   const combat::Relations& friends,std::uint32_t target,ImVec2 viewport,
                   ImVec4 start,ImVec4 end,AuraBlend blend,ImTextureID glow,bool reduced=false) const;
    void DrawScreen(ImDrawList& draw,const WorldSnapshot& s,const CameraProjection& projection,const Config& c,ImVec2 viewport,
                    ImVec4 start,ImVec4 end,double now) const;
    void DrawHud(ImDrawList& draw,HudModule kind,const WorldSnapshot& s,const Config& c,
                 const combat::Relations& friends,ImVec2 size,double now) const;
    static const char* DisplayName(const char* name,bool protect,bool player);
private:
    struct Track { bool used=false; std::uint32_t id=0; combat::Identity uuid{}; float health=0; double seen=0; };
    struct Damage { WorldPoint p{}; float amount=0; double born=-1000; };
    struct Ring { WorldPoint p{}; double born=-1000; };
    struct Notice { std::array<char,112> text{}; double born=-1000; };
    std::array<Track,kMaxEntities> tracks_{};
    std::array<Damage,kDamage> damage_{};
    std::array<Ring,kRings> rings_{};
    std::array<Notice,kNotices> notices_{};
    std::array<Marker,kMarkers> markers_{};
    std::array<std::array<double,96>,2> clicks_{};
    std::array<int,2> clickHeads_{};
    keys::Input input_{};
    std::array<char,kNameLen> dimension_{};
    std::array<char,kAddrLen> server_{};
    WorldPoint previous_{};
    float previousHealth_=0,previousVertical_=0;
    double now_=0,sampled_=0,started_=0,hurtAt_=-1000,lastSoundAt_=-1000;
    std::uint64_t frame_=0;
    int damageHead_=0,ringHead_=0,noticeHead_=0;
    Stats stats_{};
    bool initialized_=false,hitSound_=false;
};
} // namespace mcov::rockstar

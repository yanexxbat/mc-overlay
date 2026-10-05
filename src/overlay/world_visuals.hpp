#pragma once
#include "esp_geometry.hpp"
#include "aura_renderer.hpp"
#include <array>

namespace mcov {
struct EffectPalette {
    bool auraColours=true;
    ImVec4 start{.66f,.42f,1.f,1.f},end{.05f,1.f,.88f,1.f};
};
enum class TrailSubjects { Self, Players, Target, Living };
enum class TrailShape { Ribbon, Line, Dots };
struct TrailConfig {
    bool enabled=false;
    TrailSubjects subjects=TrailSubjects::Self;
    TrailShape shape=TrailShape::Ribbon;
    float lifetime=1.2f,height=.85f,width=.035f,opacity=.65f,glow=1.f;
    EffectPalette colours{};
};
enum class ParticleShape { Orbs, Stars };
struct HitParticleConfig {
    bool enabled=false,targetOnly=false;
    ParticleShape shape=ParticleShape::Orbs;
    int amount=12;
    float lifetime=.9f,size=.065f,speed=1.6f,gravity=1.2f,opacity=.9f,glow=1.f;
    EffectPalette colours{};
};
enum class AmbientShape { Fireflies, Stars, Snow };
struct AmbientParticleConfig {
    bool enabled=false;
    AmbientShape shape=AmbientShape::Fireflies;
    int amount=48;
    float radius=7.f,size=.045f,speed=.3f,opacity=.55f,glow=1.f;
    EffectPalette colours{};
};
struct HitBubbleConfig {
    bool enabled=false,targetOnly=false;
    float lifetime=.75f,radius=.65f,width=.025f,opacity=.8f,glow=1.f;
    EffectPalette colours{};
};
struct KillEffectConfig {
    bool enabled=false,targetOnly=false;
    ParticleShape shape=ParticleShape::Stars;
    int amount=24;
    float lifetime=1.4f,radius=1.2f,size=.07f,speed=2.f,opacity=.9f,glow=1.f;
    EffectPalette colours{};
};
struct BreadCrumbConfig {
    bool enabled=false;
    float lifetime=2.f,spacing=.25f,size=.06f,opacity=.75f,glow=1.f;
    EffectPalette colours{};
};
struct WorldVisualsConfig {
    float maxDistance=64.f;
    TrailConfig trails{};
    HitParticleConfig hits{};
    AmbientParticleConfig ambient{};
    HitBubbleConfig bubbles{};
    KillEffectConfig kills{};
    BreadCrumbConfig crumbs{};
    bool Enabled() const noexcept {
        return trails.enabled||hits.enabled||ambient.enabled||bubbles.enabled||kills.enabled||crumbs.enabled;
    }
};
struct WorldVisualStats { int trails=0,hitParticles=0,ambientParticles=0,bubbles=0,kills=0,killParticles=0,crumbs=0; };

// Ausschließlich Snapshot-Daten. Weltpunkte und Historie bleiben in double;
// pro Effekt feste Speichergrenzen, keine eigenen Frame-Allokationen oder JNI.
class WorldVisualsRenderer final {
public:
    static constexpr int kTracks=64,kTrailSamples=96,kHitParticles=384,kAmbientParticles=160;
    static constexpr int kRings=64,kKillParticles=192,kCrumbs=96,kRingSegments=48;
    WorldVisualsRenderer() noexcept;
    static void Prepare(ImDrawList& draw);
    void Reset();
    void Update(const WorldSnapshot& snapshot,double now,const WorldVisualsConfig& config,
                std::uint32_t targetId=0);
    void Draw(ImDrawList& draw,const CameraProjection& projection,ImVec2 viewport,
              const WorldVisualsConfig& config,ImVec4 auraStart,ImVec4 auraEnd,
              AuraBlend blend={},ImTextureID glow=0,ImVec2 origin={},bool reduceMotion=false) const;
    WorldVisualStats Stats() const;
private:
    struct Sample { WorldPoint p{}; double born=0; };
    struct Track {
        bool valid=false,self=false,wasTarget=false;
        std::uint32_t id=0; combat::Identity uuid{};
        std::uint8_t flags=0;
        WorldPoint p{}; float health=0;
        std::uint64_t serial=0;
        double seen=0,lastSample=0;
        std::array<Sample,kTrailSamples> samples{};
        int count=0,head=0;
    };
    struct Particle {
        bool valid=false;
        WorldPoint p{},velocity{};
        double born=0;
        float phase=0,mix=0;
    };
    struct Ring {
        bool valid=false,kill=false;
        WorldPoint p{};
        double born=0;
        float mix=0;
        std::uint32_t id=0;
        combat::Identity uuid{};
    };
    Track* Observe(const EntitySnapshot& entity,bool self);
    void Burst(WorldPoint position,const HitParticleConfig& config);
    void KillBurst(WorldPoint position,const KillEffectConfig& config);
    void SpawnRing(WorldPoint position,const EntitySnapshot& entity,bool kill);
    float Random();
    void Clear();
    static bool Subject(const Track& track,TrailSubjects scope,std::uint32_t target);
    std::array<Track,kTracks> tracks_{};
    std::array<Particle,kHitParticles> hits_{};
    std::array<Particle,kAmbientParticles> ambient_{};
    std::array<Particle,kKillParticles> kills_{};
    std::array<Ring,kRings> rings_{};
    std::array<Sample,kCrumbs> crumbs_{};
    std::array<ImVec2,kRingSegments> circle_{};
    int crumbHead_=0,crumbCount_=0;
    WorldPoint local_{};
    std::array<char,kNameLen> dimension_{};
    std::array<char,kAddrLen> server_{};
    std::uint64_t frame_=0,serial_=0;
    std::uint32_t random_=0x4D434F56,target_=0;
    double now_=0,lastFrameAt_=0;
    float crumbLife_=2;
    bool initialized_=false;
};
} // namespace mcov

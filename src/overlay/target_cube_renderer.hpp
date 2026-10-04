#pragma once
#include "aura_renderer.hpp"
#include "esp_geometry.hpp"

namespace mcov {
struct TargetCubeConfig {
    bool enabled=true;
    bool animate=true;
    bool showDepth=true;
    float sizeFraction=.68f;                 // Würfelkante relativ zur Hitboxhöhe.
    float cornerFraction=.28f;
    float roundFraction=.045f;
    float strokeFraction=.05f;
    float swingDegrees=105.f;
    float directionCycleSeconds=7.f;         // Sanfter Richtungswechsel alle 3,5 Sekunden.
    float opacity=1.f;
    float glow=1.f;                          // Intensität der vorhandenen Gauß-Textur.
    float fadeSeconds=.16f;                  // Einblenden bei einem neuen Ziel; 0 = sofort.
    ImVec4 lime{.58f,1.f,.015f,1.f};
    ImVec4 yellow{.94f,1.f,.06f,1.f};
};
inline constexpr int kCubeCornerPoints=12;
struct TargetCubeWorld {
    WorldPoint center{};
    std::array<std::array<WorldPoint,kCubeCornerPoints>,8> corners{};
    float worldStroke{};
};
struct TargetCubeScreen {
    std::array<std::array<ImVec2,kCubeCornerPoints>,8> corners{};
    float stroke{};
};
// Rein visuell: Zielbindung kommt vom Combat-Report, Geometrie vom aktuellen
// interpolierten Snapshot. Es werden keine Entity-/JNI-Abfragen durchgeführt.
class TargetCubeRenderer final {
public:
    static constexpr int kGlowSamples=32;
    static constexpr int kMaxVertices=4096, kMaxIndices=8192;
    static void Prepare(ImDrawList& draw);
    void Reset() noexcept;
    void Update(const EntitySnapshot* target,float dt,bool animate,const TargetCubeConfig& config) noexcept;
    double Angle() const noexcept { return angle_; }
    float Alpha() const noexcept { return alpha_; }
    bool BuildWorld(const EntitySnapshot& target,const WorldSnapshot& camera,
                    const TargetCubeConfig& config,TargetCubeWorld& out) const noexcept;
    static bool Project(const CameraProjection& projection,const TargetCubeWorld& world,
                        ImVec2 viewport,TargetCubeScreen& out,ImVec2 origin={}) noexcept;
    void Draw(ImDrawList& draw,const TargetCubeScreen& screen,const TargetCubeConfig& config,
              AuraBlend blend,ImTextureID glowTexture) const;
private:
    std::uint32_t target_{};
    combat::Identity uuid_{};
    double phase_{},angle_{};
    float enterTime_{},alpha_{};
};
} // namespace mcov

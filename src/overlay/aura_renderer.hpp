#pragma once
#include "imgui.h"
#include <array>
#include <cstdint>

namespace mcov {

struct AuraConfig {
    bool enabled = false;
    bool animate = true;                    // Eigener Schalter, unabhängig von Menü-/Windows-Animationen.
    int orbCount = 4;                        // Wenige große, weiche Orbs statt radialer Strahlen.
    float orbitRadius = .88f;                // Umlaufbahn relativ zur halben Entitätshöhe.
    float orbSize = .20f;                    // Orb-Radius relativ zur Entitätsgröße.
    float trailDegrees = 100.f;              // Länge des gebogenen Schweifs auf der Umlaufbahn.
    float rotationDegreesPerSecond = 75.f;
    float pulseAmplitude = .035f;
    float pulseHz = .4f;
    float opacity = .9f;
    float fadeSeconds = .18f;                // 0 = sofortiger Zielwechsel.
    ImVec4 startColor{.66f, .42f, 1.f, 1.f};    // Violetter Halo um den weißen Kern.
    ImVec4 endColor{.05f, 1.f, .88f, .85f};    // Türkiser, weich auslaufender Schweif.
};

// Weltanker bleiben in double. Erst die Kamera zieht ihren Ursprung ab.
struct AuraAnchor {
    std::uint32_t entityId{};
    double x{}, y{}, z{};
    float worldRadius{};
};
struct AuraScreen {
    ImVec2 center{};
    float radius{};                          // ImGui-Koordinaten, keine Desktop-Koordinaten.
};
struct AuraBlend {
    ImDrawCallback begin{};
    void* data{};                            // Muss bis RenderDrawData() gültig bleiben.
};
inline constexpr int kAuraMaxOrbs = 8;
inline constexpr int kAuraTrailSamples = 32;
struct AuraWorldSample {
    double x{}, y{}, z{};
    float radius{};
};
struct AuraWorldFog {
    std::array<std::array<AuraWorldSample, kAuraTrailSamples>, kAuraMaxOrbs> trails{};
    int orbCount{};
};
struct AuraScreenSample {
    ImVec2 center{};
    float radius{};
    bool visible{};
};
struct AuraScreenFog {
    std::array<std::array<AuraScreenSample, kAuraTrailSamples>, kAuraMaxOrbs> trails{};
    int orbCount{};
};

// Rein visuell: kein JNI, keine Zielauswahl und keine Kameraabfrage.
class AuraRenderer final {
public:
    static constexpr int kMaxOrbs = kAuraMaxOrbs;
    static constexpr int kTrailSamples = kAuraTrailSamples;
    // Zwei Ziele, zwei weiche Lagen pro Trailpunkt und drei Lagen pro Orb-Kern.
    static constexpr int kMaxVertices = 2 * kMaxOrbs * (kTrailSamples * 2 + 3) * 4;
    static constexpr int kMaxIndices = kMaxVertices / 4 * 6;
    struct Target {
        AuraAnchor anchor{};
        float alpha{};
        bool valid{};
    };

    AuraRenderer() noexcept;
    AuraRenderer(const AuraRenderer&) = delete;
    AuraRenderer& operator=(const AuraRenderer&) = delete;

    // Einmal nach NewFrame() vorwärmen. ImGui behält die Kapazitäten danach.
    static void Prepare(ImDrawList& draw);
    void Reset() noexcept;
    void UpdateTarget(const AuraAnchor* selected, float deltaSeconds, const AuraConfig& config) noexcept;
    // Auch das ausblendende Ziel folgt seiner aktuellen Weltposition.
    void RefreshAnchor(const AuraAnchor& anchor) noexcept;
    const std::array<Target, 2>& Targets() const noexcept { return targets_; }
    void BuildWorldFog(const AuraAnchor& anchor, const AuraConfig& config, AuraWorldFog& out) noexcept;
    void Draw(ImDrawList& draw, const AuraScreenFog& screen, float targetAlpha,
              const AuraConfig& config, AuraBlend blend, ImTextureID glowTexture);

private:
    void CacheTrail(float degrees) noexcept;
    std::array<ImVec2, kTrailSamples> trailAngles_{};
    std::array<Target, 2> targets_{};           // 0: ausblendend, 1: aktuelles Ziel.
    float cachedTrailDegrees_ = -1;
    double rotation_{}, pulsePhase_{};
};
} // namespace mcov

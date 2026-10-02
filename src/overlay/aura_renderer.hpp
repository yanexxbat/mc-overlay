#pragma once
#include "imgui.h"
#include <array>
#include <cstdint>

namespace mcov {

struct AuraConfig {
    bool enabled = false;
    bool animate = true;                    // Aus: nur weiche Deckkraftwechsel.
    int rayCount = 24;                       // Gleichmäßig verteilte Strahlen, maximal 64.
    float rayLength = 1.35f;                 // Vielfaches des projizierten Entitätsradius.
    float rayWidth = .11f;                   // Maximale Breite, ebenfalls relativ zum Radius.
    float rotationDegreesPerSecond = 12.f;
    float pulseAmplitude = .06f;             // Relativer Größenhub: 0 = kein Pulsieren.
    float pulseHz = .6f;
    float opacity = .65f;
    float fadeSeconds = .18f;                // 0 = sofortiger Zielwechsel.
    ImVec4 startColor{.78f, .91f, 1.f, 1.f};
    ImVec4 endColor{.18f, .40f, 1.f, 0.f};
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

// Rein visuell: kein JNI, keine Zielauswahl und keine Kameraabfrage.
class AuraRenderer final {
public:
    static constexpr int kMaxRays = 64;
    static constexpr int kCircleSegments = 64;
    static constexpr int kRayBands = 8;
    static constexpr int kRingBands = 8;
    static constexpr int kCoreBands = 4;
    // Zwei Auren für den Zielwechsel, jeweils Ring + drei Strahlenlagen + Kern.
    static constexpr int kMaxVertices = 2 * (kCircleSegments * (kRingBands + kCoreBands) * 4
                                           + kMaxRays * kRayBands * 3 * 4);
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
    void Draw(ImDrawList& draw, const AuraScreen& screen, float targetAlpha,
              const AuraConfig& config, AuraBlend blend);

private:
    void CacheRays(int count) noexcept;
    std::array<ImVec2, kCircleSegments + 1> circle_{};
    std::array<ImVec2, kMaxRays> rays_{};
    std::array<ImVec2, kRayBands + 1> rayProfile_{}; // Normierte Längen und Breiten.
    std::array<Target, 2> targets_{};           // 0: ausblendend, 1: aktuelles Ziel.
    int cachedRayCount_{};
    double rotation_{}, pulsePhase_{};
};
} // namespace mcov

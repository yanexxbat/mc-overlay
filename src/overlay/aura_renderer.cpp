#include "aura_renderer.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace mcov {
namespace {
constexpr double kTau = 2. * std::numbers::pi;
float Safe(float value, float fallback, float low, float high) noexcept {
    return std::clamp(std::isfinite(value) ? value : fallback, low, high);
}
ImVec4 SafeColor(ImVec4 color) noexcept {
    return {Safe(color.x, 1, 0, 1), Safe(color.y, 1, 0, 1),
            Safe(color.z, 1, 0, 1), Safe(color.w, 1, 0, 1)};
}
bool Valid(const AuraAnchor& a) noexcept {
    return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z) &&
           std::isfinite(a.worldRadius) && a.worldRadius > 0;
}
ImVec2 At(ImVec2 center, ImVec2 direction, float distance) noexcept {
    return {center.x + direction.x * distance, center.y + direction.y * distance};
}
ImU32 Gradient(ImVec4 start, ImVec4 end, float t, float alpha) noexcept {
    return ImGui::ColorConvertFloat4ToU32({start.x + (end.x - start.x) * t,
        start.y + (end.y - start.y) * t, start.z + (end.z - start.z) * t,
        std::clamp((start.w + (end.w - start.w) * t) * alpha, 0.f, 1.f)});
}
// AddConvexPolyFilled schreibt ohne AA genau vier Vertices. Die individuelle
// Vertexfarbe ergibt einen echten, vom Rasterizer interpolierten Verlauf.
void Quad(ImDrawList& draw, const ImVec2 (&points)[4], ImU32 inner, ImU32 outer) {
    const int first = draw.VtxBuffer.Size;
    draw.AddConvexPolyFilled(points, 4, IM_COL32_WHITE);
    draw.VtxBuffer[first].col = inner;
    draw.VtxBuffer[first + 1].col = inner;
    draw.VtxBuffer[first + 2].col = outer;
    draw.VtxBuffer[first + 3].col = outer;
}
class DrawScope final {
    ImDrawList& draw_;
    ImDrawListFlags saved_;
public:
    DrawScope(ImDrawList& draw, AuraBlend blend) : draw_(draw), saved_(draw.Flags) {
        if (blend.begin) draw_.AddCallback(blend.begin, blend.data);
        // Die weichen Ränder entstehen durch die Geometrie und Alpha-Verläufe.
        // So benötigt AddConvexPolyFilled keinen temporären AA-Puffer.
        draw_.Flags &= ~ImDrawListFlags_AntiAliasedFill;
    }
    ~DrawScope() {
        draw_.Flags = saved_;
        // Der Backend-Callback setzt auch Shader, Textur und Blendzustand zurück.
        draw_.AddCallback(ImDrawCallback_ResetRenderState, nullptr);
    }
    DrawScope(const DrawScope&) = delete;
    DrawScope& operator=(const DrawScope&) = delete;
};
} // namespace

AuraRenderer::AuraRenderer() noexcept {
    for (int i = 0; i <= kCircleSegments; ++i) {
        const double angle = kTau * i / kCircleSegments;
        circle_[i] = {float(std::cos(angle)), float(std::sin(angle))};
    }
    CacheRays(24);
}
void AuraRenderer::Prepare(ImDrawList& draw) {
    draw.VtxBuffer.reserve(draw.VtxBuffer.Size + kMaxVertices);
    draw.IdxBuffer.reserve(draw.IdxBuffer.Size + kMaxIndices);
    draw.CmdBuffer.reserve(draw.CmdBuffer.Size + 8);
}
void AuraRenderer::Reset() noexcept {
    targets_ = {};
    rotation_ = pulsePhase_ = 0;
}
void AuraRenderer::CacheRays(int count) noexcept {
    if (count == cachedRayCount_) return;
    cachedRayCount_ = count;
    for (int i = 0; i < count; ++i) {
        const double angle = kTau * i / count;
        rays_[i] = {float(std::cos(angle)), float(std::sin(angle))};
    }
}
void AuraRenderer::RefreshAnchor(const AuraAnchor& anchor) noexcept {
    if (!Valid(anchor)) return;
    for (auto& target : targets_)
        if (target.valid && target.anchor.entityId == anchor.entityId) target.anchor = anchor;
}
void AuraRenderer::UpdateTarget(const AuraAnchor* selected, float deltaSeconds, const AuraConfig& config) noexcept {
    const float dt = Safe(deltaSeconds, 0, 0, .25f);
    if (!config.enabled || (selected && !Valid(*selected))) selected = nullptr;
    auto& old = targets_[0];
    auto& current = targets_[1];
    if (selected) {
        if (current.valid && current.anchor.entityId == selected->entityId) {
            current.anchor = *selected;
        } else if (old.valid && old.anchor.entityId == selected->entityId) {
            // Eine schnelle Rückkehr tauscht die Ziele, statt den Fade neu zu starten.
            std::swap(old, current);
            current.anchor = *selected;
        } else {
            if (current.valid) old = current;
            current = {*selected, 0.f, true};
        }
    } else if (current.valid) {
        old = current;
        current = {};
    }
    const float fade = Safe(config.fadeSeconds, .18f, 0, 2);
    const float step = fade > 0 ? dt / fade : 1.f;
    if (current.valid) current.alpha = std::min(1.f, current.alpha + step);
    if (old.valid) {
        old.alpha = std::max(0.f, old.alpha - step);
        if (old.alpha == 0) old = {};
    }
    if (config.animate) {
        rotation_ = std::remainder(rotation_ + Safe(config.rotationDegreesPerSecond, 12, -180, 180)
                                   * std::numbers::pi / 180. * dt, kTau);
        pulsePhase_ = std::remainder(pulsePhase_ + kTau * Safe(config.pulseHz, .6f, 0, 5) * dt, kTau);
    }
}
void AuraRenderer::Draw(ImDrawList& draw, const AuraScreen& screen, float targetAlpha,
                        const AuraConfig& config, AuraBlend blend) {
    if (!std::isfinite(screen.center.x) || !std::isfinite(screen.center.y) ||
        !std::isfinite(screen.radius) || screen.radius <= 0) return;
    const float opacity = Safe(config.opacity, .65f, 0, 1) * Safe(targetAlpha, 0, 0, 1);
    if (opacity <= 0) return;
    CacheRays(std::clamp(config.rayCount, 3, kMaxRays));
    const float pulse = 1.f + Safe(config.pulseAmplitude, .06f, 0, .3f) * float(std::sin(pulsePhase_));
    const float radius = screen.radius * pulse;
    const float reach = radius * Safe(config.rayLength, 1.35f, .5f, 3);
    const float halfWidth = radius * Safe(config.rayWidth, .11f, .01f, .5f) * .5f;
    const ImVec4 start = SafeColor(config.startColor), end = SafeColor(config.endColor);
    const auto clip = draw.GetClipRectMin(), clipMax = draw.GetClipRectMax();
    const float extent = reach * 1.12f + halfWidth * 2;
    if (screen.center.x + extent < clip.x || screen.center.x - extent > clipMax.x ||
        screen.center.y + extent < clip.y || screen.center.y - extent > clipMax.y) return;
    DrawScope scope(draw, blend);

    // 1. Weicher Ring: Innen- und Außenkante laufen auf Alpha null aus.
    for (int band = 0; band < kRingBands; ++band) {
        const float a = float(band) / kRingBands, b = float(band + 1) / kRingBands;
        const float innerRadius = reach * (.48f + a * .64f), outerRadius = reach * (.48f + b * .64f);
        const ImU32 ca = Gradient(start, end, .4f + .5f * a, opacity * .18f * std::sin(float(std::numbers::pi) * a));
        const ImU32 cb = Gradient(start, end, .4f + .5f * b, opacity * .18f * std::sin(float(std::numbers::pi) * b));
        for (int i = 0; i < kCircleSegments; ++i) {
            const ImVec2 points[] = {At(screen.center, circle_[i + 1], innerRadius),
                At(screen.center, circle_[i], innerRadius),
                At(screen.center, circle_[i], outerRadius), At(screen.center, circle_[i + 1], outerRadius)};
            Quad(draw, points, ca, cb);
        }
    }
    // 2. Strahlen: drei Breitenlagen erzeugen einen weichen seitlichen Saum.
    const float angle = float(rotation_);
    const float cs = std::cos(angle), sn = std::sin(angle);
    constexpr float widths[] = {2.8f, 1.7f, 1.f};
    constexpr float strengths[] = {.11f, .21f, .48f};
    for (int layer = 0; layer < 3; ++layer) for (int i = 0; i < cachedRayCount_; ++i) {
        const ImVec2 d{rays_[i].x * cs - rays_[i].y * sn, rays_[i].x * sn + rays_[i].y * cs};
        const ImVec2 n{-d.y, d.x};
        for (int band = 0; band < kRayBands; ++band) {
            const float a = float(band) / kRayBands, b = float(band + 1) / kRayBands;
            // Der Strahl beginnt im Kern und endet in einer schmalen Spitze.
            const float wa = halfWidth * widths[layer] * std::max(0.f,std::sin(float(std::numbers::pi) * a));
            const float wb = halfWidth * widths[layer] * std::max(0.f,std::sin(float(std::numbers::pi) * b));
            const auto pa = At(screen.center, d, reach * a), pb = At(screen.center, d, reach * b);
            const ImVec2 points[] = {{pa.x + n.x * wa, pa.y + n.y * wa},
                {pa.x - n.x * wa, pa.y - n.y * wa},
                {pb.x - n.x * wb, pb.y - n.y * wb}, {pb.x + n.x * wb, pb.y + n.y * wb}};
            Quad(draw, points, Gradient(start, end, a, opacity * strengths[layer] * (1-a)),
                 Gradient(start, end, b, opacity * strengths[layer] * (1-b)));
        }
    }
    // 3. Heller Kern mit radialem Verlauf; kein hartes, deckendes Rechteck.
    const float coreRadius = radius * .19f;
    for (int band = 0; band < kCoreBands; ++band) {
        const float a = float(band) / kCoreBands, b = float(band + 1) / kCoreBands;
        const ImU32 ca = Gradient(start, end, a * .4f, opacity * .8f * (1-a) * (1-a));
        const ImU32 cb = Gradient(start, end, b * .4f, opacity * .8f * (1-b) * (1-b));
        for (int i = 0; i < kCircleSegments; ++i) {
            const ImVec2 points[] = {At(screen.center, circle_[i + 1], coreRadius * a),
                At(screen.center, circle_[i], coreRadius * a),
                At(screen.center, circle_[i], coreRadius * b), At(screen.center, circle_[i + 1], coreRadius * b)};
            Quad(draw, points, ca, cb);
        }
    }
}
} // namespace mcov

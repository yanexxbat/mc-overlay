#include "aura_renderer.hpp"
#include "depth_tags.hpp"
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
ImVec4 Mix(ImVec4 a, ImVec4 b, float t) noexcept {
    return {a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t, a.z+(b.z-a.z)*t, a.w+(b.w-a.w)*t};
}
void Sprite(ImDrawList& draw, ImTextureID texture, const AuraScreenSample& point,
            float size, ImVec4 color, float alpha) {
    if (!point.visible || alpha <= 0 || !std::isfinite(point.center.x) || !std::isfinite(point.center.y)) return;
    const float radius = point.radius * size;
    const auto lo=draw.GetClipRectMin(), hi=draw.GetClipRectMax();
    if (!std::isfinite(radius) || radius <= 0 || point.center.x+radius<lo.x || point.center.x-radius>hi.x ||
        point.center.y+radius<lo.y || point.center.y-radius>hi.y) return;
    color.w = std::clamp(color.w * alpha, 0.f, 1.f);
    const int first=draw.VtxBuffer.Size;
    draw.AddImage(texture, {point.center.x-radius,point.center.y-radius},
                  {point.center.x+radius,point.center.y+radius}, {0,0}, {1,1}, ImGui::ColorConvertFloat4ToU32(color));
    depth::Tag(draw,first,point.depth);
}
class DrawScope final {
    ImDrawList& draw_;
public:
    DrawScope(ImDrawList& draw, AuraBlend blend, ImTextureID texture) : draw_(draw) {
        if (blend.begin) draw_.AddCallback(blend.begin, blend.data);
        // Ein Texturwechsel für den ganzen Effekt: alle Sprites werden gebatcht.
        draw_.PushTextureID(texture);
    }
    ~DrawScope() {
        draw_.PopTextureID();
        draw_.AddCallback(ImDrawCallback_ResetRenderState, nullptr);
    }
    DrawScope(const DrawScope&) = delete;
    DrawScope& operator=(const DrawScope&) = delete;
};
} // namespace

AuraRenderer::AuraRenderer() noexcept { CacheTrail(100.f); }
void AuraRenderer::Prepare(ImDrawList& draw) {
    draw.VtxBuffer.reserve(draw.VtxBuffer.Size + kMaxVertices);
    draw.IdxBuffer.reserve(draw.IdxBuffer.Size + kMaxIndices);
    draw.CmdBuffer.reserve(draw.CmdBuffer.Size + 12);
    draw._TextureIdStack.reserve(draw._TextureIdStack.Size + 2);
}
void AuraRenderer::Reset() noexcept { targets_ = {}; rotation_ = pulsePhase_ = 0; }
void AuraRenderer::CacheTrail(float degrees) noexcept {
    if (degrees == cachedTrailDegrees_) return;
    cachedTrailDegrees_ = degrees;
    for (int i=0;i<kTrailSamples;++i) {
        const double angle = degrees * std::numbers::pi / 180. * i / (kTrailSamples-1);
        trailAngles_[i] = {float(std::cos(angle)), float(std::sin(angle))};
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
    auto& old = targets_[0]; auto& current = targets_[1];
    if (selected) {
        if (current.valid && current.anchor.entityId == selected->entityId) current.anchor = *selected;
        else if (old.valid && old.anchor.entityId == selected->entityId) {
            // Beim Rückwechsel die bestehende Deckkraft weiterverwenden.
            std::swap(old, current); current.anchor = *selected;
        } else {
            if (current.valid) old = current;
            current = {*selected, 0.f, true};
        }
    } else if (current.valid) { old = current; current = {}; }
    const float fade = Safe(config.fadeSeconds, .18f, 0, 2);
    const float step = fade > 0 ? dt / fade : 1.f;
    if (current.valid) current.alpha = std::min(1.f, current.alpha + step);
    if (old.valid) {
        old.alpha = std::max(0.f, old.alpha - step);
        if (old.alpha == 0) old = {};
    }
    if (config.animate) {
        rotation_ = std::remainder(rotation_ + Safe(config.rotationDegreesPerSecond,75,-180,180)
                                   * std::numbers::pi / 180. * dt, kTau);
        pulsePhase_ = std::remainder(pulsePhase_ + kTau * Safe(config.pulseHz,.4f,0,5) * dt, kTau);
    }
}
void AuraRenderer::BuildWorldFog(const AuraAnchor& anchor, const AuraConfig& config, AuraWorldFog& out) noexcept {
    out.orbCount = 0;
    if (!Valid(anchor)) return;
    CacheTrail(Safe(config.trailDegrees,100,10,220));
    const float pulse = 1.f + Safe(config.pulseAmplitude,.035f,0,.3f) * float(std::sin(pulsePhase_));
    const double orbit = anchor.worldRadius * Safe(config.orbitRadius,.88f,.3f,2) * pulse;
    const float size = anchor.worldRadius * Safe(config.orbSize,.20f,.05f,.5f) * pulse;
    const float direction = config.rotationDegreesPerSecond < 0 ? -1.f : 1.f;
    out.orbCount = std::clamp(config.orbCount,1,kMaxOrbs);
    for (int i=0;i<out.orbCount;++i) {
        const double phase = rotation_ + kTau * i / out.orbCount;
        const double cs=std::cos(phase), sn=std::sin(phase);
        // Gegeneinander geneigte 3D-Bahnen, damit die Orbs den ganzen Körper umspielen.
        const double yaw = i * 1.13;
        const double cy=std::cos(yaw), sy=std::sin(yaw);
        const double tilt = (i%2 ? -1 : 1) * std::numbers::pi / 3.;
        const double ct=std::cos(tilt), st=std::sin(tilt);
        for (int j=0;j<kTrailSamples;++j) {
            const auto cached=trailAngles_[j];
            const double c=cs*cached.x + sn*cached.y*direction;
            const double s=sn*cached.x - cs*cached.y*direction;
            const double x=orbit*c, y=orbit*s*st, z=orbit*s*ct;
            const float age=float(j)/(kTrailSamples-1);
            // Keine Positionshistorie: der ganze Schweif bleibt am aktuellen Weltanker.
            out.trails[i][j] = {anchor.x+x*cy-z*sy, anchor.y+y, anchor.z+x*sy+z*cy,
                                size*(1.f-.72f*age)};
        }
    }
}
void AuraRenderer::Draw(ImDrawList& draw, const AuraScreenFog& screen, float targetAlpha,
                        const AuraConfig& config, AuraBlend blend, ImTextureID glowTexture) {
    const float opacity = Safe(config.opacity,.9f,0,1) * Safe(targetAlpha,0,0,1);
    if (!glowTexture || opacity <= 0 || screen.orbCount <= 0) return;
    DrawScope scope(draw,blend,glowTexture);
    const ImVec4 head=SafeColor(config.startColor), tail=SafeColor(config.endColor);
    for (int i=0;i<std::min(screen.orbCount,kMaxOrbs);++i) {
        // Überlappende Gauß-Sprites ergeben einen weichen, gebogenen Nebelschweif.
        // Kopf bei Index 0; zuerst das fast transparente Schweifende zeichnen.
        for (int j=kTrailSamples-1;j>=0;--j) {
            const float age=float(j)/(kTrailSamples-1);
            const float fade=(1-age)*(1-age);
            const ImVec4 color=Mix(head,tail,std::min(1.f,age*1.8f));
            const auto& point=screen.trails[i][j];
            Sprite(draw,glowTexture,point,3.1f,color,opacity*fade*.085f);
            Sprite(draw,glowTexture,point,1.05f,color,opacity*fade*.27f);
        }
        const auto& point=screen.trails[i][0];
        Sprite(draw,glowTexture,point,3.0f,head,opacity*.45f);
        const ImVec4 white=Mix(head,ImVec4(1,1,1,1),.88f);
        Sprite(draw,glowTexture,point,1.0f,white,opacity*.92f);
        Sprite(draw,glowTexture,point,.60f,ImVec4(1,1,1,1),opacity*.70f);
    }
}
} // namespace mcov

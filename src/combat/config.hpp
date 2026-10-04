#pragma once
#include "types.hpp"
#include <mutex>

namespace mcov::combat {
enum class TargetMode { Crosshair, NearestPlayer, LowestHealth, AngleDistance, NearestEntity };
enum class FovMode { DynamicDegrees, StaticPixels };
// Feste Adapter-Policy, kein weiterer Regler im Live-Menü oder in der IPC-Config.
enum class ExecutionMode { Smoothed, Continuous };
struct TargetConfig {
    bool enabled{true}, playersOnly{}, requireVisibility{true}, prioritizeEnemies{}, multiTarget{true};
    TargetMode mode{TargetMode::NearestEntity};
    FovMode fovMode{FovMode::DynamicDegrees};
    double coneDegrees{180.0}, radiusPixels{160.0}, range{3.0};
    double aimHeight{0.75}, angleWeight{0.7}, distanceWeight{0.3};
    double visibilityMaxAge{0.100}, switchImprovement{0.08};
    Range<double> switchDelay{0.080,0.180};
};
struct RotationConfig {
    bool enabled{true};
    Range<double> reactionDelay{0.010,0.040};
    Range<double> yawDuration{0.080,0.220}, pitchDuration{0.100,0.260};
    Range<double> overshootFraction{0.0,0.0}, jitterDegrees{0.0,0.0}, jitterPeriod{0.070,0.140};
    double maxOvershootDegrees{1.0}, correctionDuration{0.090};
    double smoothing{1.0}, maxYawPerTick{18.0}, maxPitchPerTick{12.0}, tickSeconds{0.050};
    double tangentFraction{0.33}, replanDegrees{1.0}, settleDegrees{0.08};
};
struct AttackConfig {
    bool enabled{};
    double range{3.0}, cooldownThreshold{0.98}, alignmentDegrees{1.5};
    Range<double> firstClickDelay{0.035,0.090};
};
struct SyncConfig {
    bool enabled{true};
    Range<double> interval{0.040,0.060};
    double poseMaxAge{0.150}, attackMaxAge{0.075};
    double positionDesync{0.5}, angleDesync{8.0}, correctionMaxAge{0.250};
    double recoverySeconds{0.200}, recoverySpeedScale{0.35};
};
struct HudConfig { bool enabled{}, target{true}, fov{}, trace{}, packetStats{}; };
struct Config {
    bool enabled{}; // Explizite Aktivierung; der Simulator aktiviert seinen eigenen Kern.
    std::uint64_t seed{0x4d434f56};
    double frameMaxAge{0.100};
    TargetConfig target{};
    RotationConfig rotation{};
    AttackConfig attack{};
    SyncConfig sync{};
    HudConfig hud{};
};

inline Config MakeKillAuraConfig(const Config& requested) {
    // Frühere technische Einstellungen dürfen die automatische Kill Aura nicht
    // bremsen. Nur Aktivierung, Entity-Typ, Reichweite und HUD werden übernommen.
    Config live;
    live.enabled=requested.enabled; live.seed=requested.seed;
    live.target.playersOnly=requested.target.playersOnly;
    live.target.multiTarget=false;
    live.target.range=live.attack.range=requested.attack.range;
    live.attack.enabled=true; live.attack.firstClickDelay={0,0};
    live.hud=requested.hud;
    return live;
}

inline bool Within(double v, double lo, double hi) { return std::isfinite(v) && v>=lo && v<=hi; }
inline bool ValidRange(Range<double> r, double lo, double hi) { return Within(r.min,lo,hi)&&Within(r.max,r.min,hi); }
inline bool Validate(const Config& c) {
    const auto& t=c.target; const auto& r=c.rotation; const auto& a=c.attack; const auto& s=c.sync;
    return Within(c.frameMaxAge,0.001,0.5) &&
        (t.mode==TargetMode::Crosshair || t.mode==TargetMode::NearestPlayer || t.mode==TargetMode::LowestHealth || t.mode==TargetMode::AngleDistance || t.mode==TargetMode::NearestEntity) &&
        (t.fovMode==FovMode::DynamicDegrees || t.fovMode==FovMode::StaticPixels) &&
        Within(t.coneDegrees,1,180) && Within(t.radiusPixels,10,2000) && Within(t.range,0.1,64) &&
        Within(t.aimHeight,0.1,0.95) && Within(t.angleWeight,0,1) && Within(t.distanceWeight,0,1) &&
        t.angleWeight+t.distanceWeight>0 && Within(t.visibilityMaxAge,0.001,0.5) &&
        Within(t.switchImprovement,0,1) && ValidRange(t.switchDelay,0,1) &&
        ValidRange(r.reactionDelay,0.010,0.040) && ValidRange(r.yawDuration,0.010,2) && ValidRange(r.pitchDuration,0.010,2) &&
        ValidRange(r.overshootFraction,0,0.2) && ValidRange(r.jitterDegrees,0,0.5) && ValidRange(r.jitterPeriod,0.010,1) &&
        Within(r.maxOvershootDegrees,0,3) && Within(r.correctionDuration,0.010,1) && Within(r.smoothing,0.25,4) &&
        Within(r.maxYawPerTick,0.1,90) && Within(r.maxPitchPerTick,0.1,90) && Within(r.tickSeconds,0.010,0.2) &&
        Within(r.tangentFraction,0,1) &&
        Within(r.replanDegrees,0.01,20) && Within(r.settleDegrees,0,1) &&
        Within(a.range,0.1,6) && Within(a.cooldownThreshold,0.8,1) && Within(a.alignmentDegrees,0.1,5) &&
        ValidRange(a.firstClickDelay,0,0.5) && ValidRange(s.interval,0.010,0.200) &&
        Within(s.poseMaxAge,0.010,0.5) && Within(s.attackMaxAge,0.010,0.2) &&
        Within(s.positionDesync,0.01,8) && Within(s.angleDesync,0.1,90) && Within(s.correctionMaxAge,0.010,1) &&
        Within(s.recoverySeconds,0.010,1) && Within(s.recoverySpeedScale,0.05,1);
}
struct ConfigVersion { Config values{}; std::uint64_t revision{}; };
class ConfigStore {
public:
    ConfigVersion Read() const { const std::scoped_lock lock(mutex_); return state_; }
    bool Publish(const Config& next) {
        if (!Validate(next)) return false; // Alles oder nichts, kein teilweise gültiger Hot-Reload.
        const std::scoped_lock lock(mutex_);
        state_={next,state_.revision+1}; return true;
    }
private:
    mutable std::mutex mutex_;
    ConfigVersion state_{};
};
} // namespace mcov::combat

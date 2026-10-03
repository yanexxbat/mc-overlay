#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <numeric>
#include <optional>
#include <random>
#include <span>

namespace mcov::combat {

using Seconds = double; // Monotone Zeit, niemals Systemuhr oder ImGui-Framerate.
inline constexpr double kPi = std::numbers::pi_v<double>;
inline constexpr double kDegrees = 180.0 / kPi;
inline constexpr std::size_t kQueueCapacity = 128;
inline constexpr std::size_t kTraceCapacity = 128;
inline constexpr std::size_t kRelationCapacity = 128;

struct Vec3 { double x{}, y{}, z{}; };
struct Angles { double yaw{}, pitch{}; };
struct Pose { Vec3 position{}; Angles rotation{}; };
struct Identity {
    std::uint64_t high{}, low{}; // UUID, keine kurzlebige Entity-ID.
    friend bool operator==(Identity, Identity) = default;
};
inline bool Finite(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
inline bool Finite(Angles a) { return std::isfinite(a.yaw) && std::isfinite(a.pitch); }
inline double Wrap(double a) { return std::remainder(a, 360.0); }
inline Angles Normalize(Angles a) { return {Wrap(a.yaw), std::clamp(a.pitch, -90.0, 90.0)}; }
inline double Length(Vec3 v) { return std::hypot(v.x, v.y, v.z); }
inline Vec3 Difference(Vec3 a, Vec3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
inline Vec3 Direction(Angles a) {
    const double y=a.yaw/kDegrees, p=a.pitch/kDegrees;
    return {-std::sin(y)*std::cos(p), -std::sin(p), std::cos(y)*std::cos(p)};
}
inline double AngularDistance(Angles a, Angles b) {
    const auto x=Direction(a), y=Direction(b);
    return std::acos(std::clamp(x.x*y.x+x.y*y.y+x.z*y.z, -1.0, 1.0))*kDegrees;
}
inline Angles LookAt(Vec3 eye, Vec3 point) {
    const auto d=Difference(point,eye);
    return Normalize({std::atan2(-d.x,d.z)*kDegrees, -std::atan2(d.y,std::hypot(d.x,d.z))*kDegrees});
}

template<class T> struct Range { T min{}, max{}; };
// Eigene Umrechnung: identische Zufallsfolge unabhängig von uniform_real_distribution.
class Random {
public:
    explicit Random(std::uint64_t seed=0) : generator_(seed) {}
    void Seed(std::uint64_t seed) { generator_.seed(seed); }
    double Sample(Range<double> r) {
        const double unit=static_cast<double>(generator_() >> 11)*0x1.0p-53;
        return r.min+(r.max-r.min)*unit;
    }
private:
    std::mt19937_64 generator_;
};

struct Visibility { bool visible{}; Seconds sampledAt{}; };
struct Entity {
    std::uint32_t id{};
    Identity uuid{};
    std::array<char,40> name{};
    Vec3 boundsMin{}, boundsMax{}; // Absolute Double-Koordinaten der aktuellen Hitbox.
    double health{};
    bool player{}, living{}, self{};
    std::optional<Visibility> visibility; // Unbekannt wird niemals zu "sichtbar".
};
struct ServerCorrection {
    std::uint64_t serial{}, world{};
    Seconds sampledAt{};
    Pose server{}, expectedAtSameTime{}; // Gleichzeitige Zustände; kein Vergleich mit einem alten Ack.
};
struct Frame {
    Seconds now{}, sampledAt{}, dt{};
    std::uint64_t world{}, tick{};
    Vec3 eye{}, playerPosition{};
    Angles observedRotation{}; // Kamera bzw. zuletzt tatsächlich gesendete Rotation bei Silent.
    Angles cameraRotation{}; // FOV und Crosshair-Auswahl bleiben an der sichtbaren Kamera.
    double verticalFovDeg{70.0}, viewportHeight{1080.0};
    std::uint32_t crosshairId{};
    std::uint32_t attackHitId{}; // Frischer, wandgeprüfter Raycast entlang observedRotation.
    std::optional<double> attackCooldown; // Vom Spiel, 0..1; fehlend blockiert Angriffe.
    std::optional<double> gameAttackReach;
    std::optional<ServerCorrection> correction;
    std::span<const Entity> entities;
    bool inputAllowed{}, silentSupported{};
};
struct Target {
    std::uint32_t id{};
    Identity uuid{};
    std::array<char,40> name{};
    Vec3 aimPoint{};
    Angles desired{};
    double range{}, angle{}, score{};
    bool enemy{};
};
enum class CommandKind { Pose, Attack };
struct Command {
    CommandKind kind{CommandKind::Pose};
    std::uint64_t sequence{}, world{};
    std::uint32_t target{};
    Identity identity{};
    Seconds sampledAt{}, due{};
    Pose pose{};
    bool silent{};
};
struct PacketStats {
    std::uint64_t sent{}, discarded{}, coalesced{}, failed{}, corrections{};
    std::size_t buffered{};
};
struct TracePoint { Seconds time{}; Angles desired{}, actual{}; };
struct DebugSnapshot {
    std::optional<Target> target;
    Angles current{};
    double error{}, fovRadiusPx{}, cooldown{};
    bool enabled{}, reacting{}, resyncing{}, silentUnavailable{}, cooldownUnavailable{};
    PacketStats packets{};
    std::array<TracePoint,kTraceCapacity> trace{};
    std::size_t traceCount{};
};

} // namespace mcov::combat

#pragma once
#include <cmath>
#include <cstdint>
#include <type_traits>

namespace mcov::knife {
inline constexpr std::uint64_t kLeaseMs=500;
enum class Status : std::uint32_t { Unsupported, Disabled, Waiting, Ready, Failed };
struct Config {
    bool enabled=false,deploy=true,trails=true;
    int skin=0;
    float speed=1,size=1,trailSeconds=.14f;
    bool glow=true;
    float glowStrength=1.2f,glowWidth=1;
    std::uint32_t glowColour=0x69ddd5;
};
inline bool Valid(const Config& c) {
    return c.skin>=0&&c.skin<=2&&std::isfinite(c.speed)&&c.speed>=.5f&&c.speed<=2&&
        std::isfinite(c.size)&&c.size>=.5f&&c.size<=2&&std::isfinite(c.trailSeconds)&&c.trailSeconds>=.05f&&c.trailSeconds<=.35f&&
        std::isfinite(c.glowStrength)&&c.glowStrength>=0&&c.glowStrength<=3&&std::isfinite(c.glowWidth)&&c.glowWidth>=.25f&&c.glowWidth<=3&&c.glowColour<=0xffffff;
}
struct Request {
    std::uint64_t sentAtMs{},inspectSerial{};
    Config config{};
    bool inspectHeld=false;
};
inline bool Fresh(const Request& r,std::uint64_t now) { return Valid(r.config)&&r.sentAtMs&&now>=r.sentAtMs&&now-r.sentAtMs<=kLeaseMs; }
struct Report { std::uint64_t sampledAtMs{}; Status status=Status::Unsupported; };
static_assert(std::is_trivially_copyable_v<Request>&&std::is_trivially_copyable_v<Report>);
} // namespace mcov::knife

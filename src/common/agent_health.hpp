#pragma once
#include <cstdint>

namespace mcov {
// Connection liveness is wall-clock based, independent of overlay/game FPS.
class AgentHeartbeat {
    std::uint32_t value_ = 0;
    std::uint64_t lastSeenMs_ = 0;
    bool seen_ = false;
public:
    static constexpr std::uint64_t kTimeoutMs = 2000;
    void Observe(std::uint32_t value, std::uint64_t nowMs) noexcept {
        if (!seen_ || value != value_) {
            seen_ = true;
            value_ = value;
            lastSeenMs_ = nowMs;
        }
    }
    std::uint64_t AgeMs(std::uint64_t nowMs) const noexcept { return seen_ ? nowMs - lastSeenMs_ : 0; }
    bool Alive(std::uint64_t nowMs) const noexcept { return seen_ && AgeMs(nowMs) <= kTimeoutMs; }
};
} // namespace mcov

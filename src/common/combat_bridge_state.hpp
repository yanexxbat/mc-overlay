#pragma once
#include "../combat/target_selector.hpp"
#include <atomic>
#include <cstring>
#include <type_traits>

namespace mcov {
// Jede Mailbox besitzt genau einen Writer. Atomare Wörter vermeiden Datenrennen
// auch innerhalb desselben Prozesses; beide Programme verwenden dieselbe ABI.
template<class T> struct SharedMailbox {
    static_assert(std::is_trivially_copyable_v<T>);
    static constexpr std::size_t kWords=(sizeof(T)+sizeof(std::uint64_t)-1)/sizeof(std::uint64_t);
    std::atomic<std::uint32_t> sequence{};
    std::array<std::atomic<std::uint64_t>,kWords> words{};
    void Publish(const T& value) noexcept {
        std::array<std::uint64_t,kWords> copy{};
        std::memcpy(copy.data(),&value,sizeof(value));
        sequence.fetch_add(1,std::memory_order_seq_cst);
        for(std::size_t i=0;i<kWords;++i) words[i].store(copy[i],std::memory_order_seq_cst);
        sequence.fetch_add(1,std::memory_order_seq_cst);
    }
    bool Read(T& out) const noexcept {
        std::array<std::uint64_t,kWords> copy{};
        for(int attempt=0;attempt<3;++attempt) {
            const auto before=sequence.load(std::memory_order_seq_cst);
            if(before&1) continue;
            for(std::size_t i=0;i<kWords;++i) copy[i]=words[i].load(std::memory_order_seq_cst);
            if(before==sequence.load(std::memory_order_seq_cst)) { std::memcpy(&out,copy.data(),sizeof(out)); return true; }
        }
        return false;
    }
};
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
inline constexpr std::uint64_t kCombatLeaseMs=250;
enum class CombatBridgeStatus : std::uint32_t { Unavailable, Ready, Disabled, Paused, NoWorld, Active, NoTarget, Error };
inline const char* CombatBridgeText(CombatBridgeStatus s) {
    switch(s) {
        case CombatBridgeStatus::Ready:return "Client tick connected";
        case CombatBridgeStatus::Disabled:return "Disabled";
        case CombatBridgeStatus::Paused:return "Paused (menu / focus / input)";
        case CombatBridgeStatus::NoWorld:return "Waiting for a world";
        case CombatBridgeStatus::Active:return "Active";
        case CombatBridgeStatus::NoTarget:return "No target in range";
        case CombatBridgeStatus::Error:return "JNI error - combat stopped";
        default:return "Client tick bridge unavailable";
    }
}
struct CombatRequest {
    std::uint64_t sentAtMs{}, revision{};
    combat::Config config{};
    combat::Relations relations{};
    double verticalFov{70}, viewportHeight{1080};
    bool inputAllowed{};
};
struct CombatReport {
    std::uint64_t sampledAtMs{}, attacks{}, tick{};
    CombatBridgeStatus status{};
    combat::DebugSnapshot debug{};
};
inline bool CombatLeaseValid(const CombatRequest& r,std::uint64_t now) {
    return r.sentAtMs && r.sentAtMs<=now && now-r.sentAtMs<=kCombatLeaseMs && r.inputAllowed;
}
} // namespace mcov

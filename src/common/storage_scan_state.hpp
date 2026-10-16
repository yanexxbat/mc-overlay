#pragma once
#include <cstdint>
namespace mcov::storage {
struct Request {
    std::uint64_t sentAtMs=0;
    std::uint32_t radius=24,intervalMs=1000,mask=0;
    bool enabled=false;
};
enum class State : std::uint8_t { Idle,Scanning,Ready,Unavailable };
}

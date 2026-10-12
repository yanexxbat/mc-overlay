#pragma once
#include <array>
#include <cstdint>
#include <type_traits>

namespace mcov::base {
inline constexpr int kMaxRegions=8,kMaxBlocks=16384,kMaxVertices=786432;
inline constexpr std::uint64_t kLeaseMs=750;
enum class Status : std::uint32_t { Unsupported, Disabled, Waiting, Building, Ready, Empty, Failed };
struct Region { double minX{},minY{},minZ{},maxX{},maxY{},maxZ{}; };
struct Request {
    std::uint64_t sentAtMs{},revision{};
    bool enabled{};
    std::uint8_t pad[3]{};
    std::uint32_t count{};
    float opacity=1;
    char dimension[40]{},server[64]{};
    std::array<Region,kMaxRegions> regions{};
};
struct Report {
    std::uint64_t sampledAtMs{},revision{};
    Status status=Status::Unsupported;
    std::uint32_t scanned{},blocks{},vertices{},skipped{};
    bool limited{};
    std::uint8_t pad[7]{};
};
static_assert(std::is_trivially_copyable_v<Request> && std::is_trivially_copyable_v<Report>);
} // namespace mcov::base

#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include "../combat/target_selector.hpp"
namespace mcov::antivanish {
inline constexpr int kRows=32;
struct Config { bool enabled=false,notifications=true,invisible=true;float range=64; };
inline bool Valid(const Config& c){return std::isfinite(c.range)&&c.range>=8&&c.range<=128;}
enum class Status:std::uint32_t { Unsupported,Disabled,Waiting,Active,Failed };
struct Request {std::uint64_t sentAtMs{};Config config{};};
enum Evidence:std::uint32_t {Hidden=1,Nearby=2,Invisible=4,Spectator=8,Targetable=16,Sound=32,Particle=64,Critical=128,Visible=256};
struct Entry {combat::Identity id{};char name[32]{},reason[96]{};std::uint32_t evidence{};};
struct Report {std::uint64_t sampledAtMs{};Status status=Status::Unsupported;std::uint32_t count{};bool limited=false,packetHooks=false;std::array<Entry,kRows> rows{};};
}

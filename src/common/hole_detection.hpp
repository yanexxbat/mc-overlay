#pragma once
#include <cstdint>
#include <string_view>
#include <initializer_list>

namespace mcov::holes {
enum class Type : std::uint8_t { None, Bedrock, Obsidian, Mixed };
inline constexpr std::int8_t kAir=-2,kBedrock=-3,kObsidian=-4;
inline constexpr std::uint32_t kMaxResults=128;
inline constexpr int kMinRadius=4,kMaxRadius=16;
inline std::int8_t Material(std::string_view key) {
    if(key=="block.minecraft.air"||key=="block.minecraft.cave_air"||key=="block.minecraft.void_air") return kAir;
    if(key=="block.minecraft.bedrock") return kBedrock;
    if(key=="block.minecraft.obsidian") return kObsidian;
    return -1;
}
// Candidate feet are air. Require a floor, all four walls and three air blocks
// of headroom; water, missing states and other materials fail closed.
template<class Read> Type Detect(Read read) {
    const auto floor=read(0,-1,0);
    if(floor!=kBedrock&&floor!=kObsidian) return Type::None;
    if(read(0,1,0)!=kAir||read(0,2,0)!=kAir) return Type::None;
    bool bedrock=floor==kBedrock,obsidian=floor==kObsidian;
    for(const auto offset:{-1,1}) for(int axis=0;axis<2;++axis) {
        const auto wall=read(axis==0?offset:0,0,axis==1?offset:0);
        if(wall!=kBedrock&&wall!=kObsidian) return Type::None;
        bedrock|=wall==kBedrock; obsidian|=wall==kObsidian;
    }
    return bedrock&&obsidian?Type::Mixed:bedrock?Type::Bedrock:Type::Obsidian;
}
inline const char* Label(Type type) {
    return type==Type::Bedrock?"Bedrock":type==Type::Obsidian?"Obsidian":"Mixed";
}
}

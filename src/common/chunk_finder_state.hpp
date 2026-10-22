#pragma once
#include <cstdint>
#include <array>
namespace mcov::chunks {
// Settings order is shared with the Java evaluator; keep flags explicit.
enum Signal : unsigned { Placed,IgnoreExposed,Base,Cobble,Masonry,Obsidian,Pistons,Stairs,Shafts,Rooms,Ore,Storage,Items,Mobs,Farm,Repeaters,Bees,Floors,Plants,Workshops,Geodes,SignalCount };
inline constexpr std::uint32_t kDefaultSignals=((1u<<SignalCount)-1)&~(1u<<Ore);
inline constexpr int kMaxFinds=2048,kMaxEvidence=1000;
struct Detection {
    std::uint32_t signals=kDefaultSignals;
    int deepslateMin=1,rotatedMin=2,cobbledMin=4,endStoneMin=2,baseScore=8,farmChance=50,geodeSensitivity=5;
};
struct Request {
    std::uint64_t sentAtMs=0,clearId=0;
    Detection detection{};
    int threads=2,chunksPerTick=3,alertsPerMinute=6;
    bool enabled=false,chat=true;
};
enum class Status : int { Unavailable,Idle,Waiting,Scanning,Failed };
struct Find {
    int x=0,z=0,category=0,score=0,chance=0,minY=-64,maxY=320;
    std::uint32_t evidence=0;
    std::uint64_t alert=0;
};
// Low byte: Signal; upper bytes: detector material kind. Zero material supports older reports.
struct Evidence { int x=0,y=0,z=0,signal=0; };
struct ScannedChunk {int x=0,z=0;};
struct Report {
    std::uint64_t sampledAtMs=0,world=0,revision=0;
    Status status=Status::Unavailable;
    int count=0,blockCount=0,scanned=0,pending=0,scannedCount=0;
    bool limited=false;
    std::array<Find,kMaxFinds> finds{};
    std::array<Evidence,kMaxEvidence> blocks{};
    std::array<ScannedChunk,4096> scannedChunks{};
};
}

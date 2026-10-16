#include "common/block_scan_volume.hpp"
#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace mcov;
namespace {
void Require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
void SmallVolumes() {
    for(const int centreY:{-70,-64,0,64,319,325}) {
        BlockScanVolume scan; scan.Reset(-17,centreY,31,4);
        std::array<bool,729> visited{};
        const int low=std::max(-64,centreY-4),high=std::min(319,centreY+4);
        Require(scan.Cells()==81ull*std::max(0,high-low+1),"Clipped volume includes invalid world heights");
        for(std::uint64_t i=0;i<scan.Cells();++i) {
            const auto p=scan.At(i);
            Require(p.x>=-21&&p.x<=-13&&p.z>=27&&p.z<=35&&p.y>=low&&p.y<=high,"Scan escaped the requested cube");
            const auto index=(p.y-low)*81+(p.z-27)*9+p.x+21;
            Require(!visited[index],"Resuming the scan revisits a position"); visited[index]=true;
        }
        Require(scan.Progress(0)==(scan.Cells()?0u:100u)&&scan.Progress(scan.Cells())==100,"Clipped progress does not complete");
    }
}
void LargeVolume() {
    BlockScanVolume scan; scan.Reset(-17,64,31,kMaxBlockScanRadius);
    Require(scan.Cells()==403440000ull&&scan.Height()==384,"512-block radius was truncated or overflowed");
    Require(scan.Progress(scan.Cells()/2)==50&&scan.Progress(scan.Cells()+1)==100,"Large-volume progress overflowed");
    const auto first=scan.At(0);
    Require(first.x==-17&&first.y==64&&first.z==31,"Scan starts far from the player");
    const int span=2*kMaxBlockScanRadius+1;
    std::vector<bool> columns(span*span); int previousRing=0;
    for(std::uint64_t column=0;column<columns.size();++column) {
        const auto p=scan.At(column*scan.Height());
        const int dx=p.x+17,dz=p.z-31,ring=std::max(std::abs(dx),std::abs(dz));
        Require(ring<=kMaxBlockScanRadius&&ring>=previousRing&&p.y==64,"Large scan does not walk nearby columns first");
        const int index=(dz+kMaxBlockScanRadius)*span+dx+kMaxBlockScanRadius;
        Require(!columns[index],"Large scan duplicated a column"); columns[index]=true; previousRing=ring;
    }
    std::array<bool,384> heights{};
    for(int i=0;i<scan.Height();++i) {
        const int y=scan.At(i).y;
        Require(y>=-64&&y<320&&!heights[y+64],"Large scan skipped or duplicated a valid height"); heights[y+64]=true;
    }
    scan.Reset(0,319,0,512);
    Require(scan.At(0).y==319&&scan.At(383).y==-64,"Top-of-world scan failed to prioritise reachable heights");
    scan.Reset(0,5000,0,512); Require(scan.Cells()==0,"Out-of-world player triggered an empty billion-cell scan");
    scan.Reset(0,64,0,100000); Require(scan.Cells()==403440000ull,"Untrusted radius escaped the hard limit");
}
}
int main() {
    try { SmallVolumes(); LargeVolume(); std::cout<<"PASS block scan: full 512 radius, clipped heights, complete unique coverage, nearby priority, bounded storage, progress\n"; return 0; }
    catch(const std::exception& e) { std::cerr<<"FAIL block scan: "<<e.what()<<'\n'; return 1; }
}

#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace mcov {
inline constexpr int kMinBlockScanRadius=4;
inline constexpr int kMaxBlockScanRadius=512;
inline constexpr int kMaxBlockDrawDistance=1024;

// Constant-memory traversal: nearby columns first, then square rings in X/Z.
// Clip the vertical range before walking so large radii never spend slices on
// out-of-world cells. Within a column, start at the player's height.
class BlockScanVolume {
    int x_=0,y_=0,z_=0,radius_=0,low_=0,high_=-1,centreY_=0;
public:
    struct Position { int x,y,z; };
    void Reset(int x,int y,int z,int radius) {
        x_=x; y_=y; z_=z; radius_=std::clamp(radius,kMinBlockScanRadius,kMaxBlockScanRadius);
        low_=std::max(-64,y_-radius_); high_=std::min(319,y_+radius_);
        centreY_=low_<=high_?std::clamp(y_,low_,high_):y_;
    }
    int Height() const { return std::max(0,high_-low_+1); }
    std::uint64_t Cells() const {
        const auto span=std::uint64_t(radius_*2+1);
        return span*span*Height();
    }
    Position At(std::uint64_t index) const {
        const int height=Height();
        if(!height) return {x_,y_,z_};
        const auto column=index/height;
        const int ring=(int(std::sqrt(double(column)))+1)/2;
        int dx=0,dz=0;
        if(ring) {
            const auto inner=std::uint64_t(2*ring-1);
            const int edge=int(column-inner*inner),side=2*ring;
            if(edge<side) { dx=-ring+edge; dz=-ring; }
            else if(edge<2*side) { dx=ring; dz=-ring+edge-side; }
            else if(edge<3*side) { dx=ring-(edge-2*side); dz=ring; }
            else { dx=-ring; dz=ring-(edge-3*side); }
        }
        const int vertical=int(index%height),up=high_-centreY_,down=centreY_-low_;
        const int paired=2*std::min(up,down)+1;
        const int dy=vertical<paired?((vertical+1)/2)*(vertical%2?1:-1):
            (std::min(up,down)+vertical-paired+1)*(up>down?1:-1);
        return {x_+dx,centreY_+dy,z_+dz};
    }
    unsigned Progress(std::uint64_t completed) const {
        const auto count=Cells();
        return count?unsigned(100*std::min(completed,count)/count):100;
    }
};
}

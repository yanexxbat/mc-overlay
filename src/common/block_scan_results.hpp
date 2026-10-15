#pragma once
#include "shared_state.hpp"
#include <algorithm>
#include <array>
#include <span>
#include <unordered_map>

namespace mcov {
struct BlockPosition {
    int x{},y{},z{};
    bool operator==(const BlockPosition&) const = default;
    static BlockPosition Of(const BlockSnapshot& b) { return {int(b.x),int(b.y),int(b.z)}; }
};
struct BlockPositionHash {
    std::size_t operator()(BlockPosition p) const {
        std::size_t h=std::hash<int>{}(p.x);
        for(int v:{p.y,p.z}) h^=std::hash<int>{}(v)+0x9e3779b9u+(h<<6)+(h>>2);
        return h;
    }
};
// Keep the previous sweep visible while publishing new matches incrementally.
// Only a successful completed sweep replaces the retained set (including an
// empty set). Consumers still use blockScanComplete before counting clusters.
class BlockScanResults {
    std::array<BlockSnapshot,kMaxBlocks> visible_{};
    std::unordered_map<BlockPosition,std::size_t,BlockPositionHash> positions_;
    std::size_t count_=0,merged_=0;
public:
    BlockScanResults() { positions_.reserve(kMaxBlocks); }
    void Reset() { count_=merged_=0; positions_.clear(); }
    void Begin() {
        merged_=0; positions_.clear();
        for(std::size_t i=0;i<count_;++i) positions_.emplace(BlockPosition::Of(visible_[i]),i);
    }
    void Merge(std::span<const BlockSnapshot> accumulated,bool commit) {
        accumulated=accumulated.first(std::min(accumulated.size(),std::size_t(kMaxBlocks)));
        if(commit) {
            count_=accumulated.size(); std::copy(accumulated.begin(),accumulated.end(),visible_.begin());
        } else for(std::size_t i=merged_;i<accumulated.size();++i) {
            const auto& b=accumulated[i]; const auto key=BlockPosition::Of(b);
            if(const auto it=positions_.find(key);it!=positions_.end()) visible_[it->second]=b;
            else if(count_<kMaxBlocks) { positions_.emplace(key,count_); visible_[count_++]=b; }
        }
        merged_=accumulated.size();
    }
    std::span<const BlockSnapshot> Results() const { return {visible_.data(),count_}; }
};
} // namespace mcov

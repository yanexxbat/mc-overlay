#pragma once
#include "shared_state.hpp"
#include <array>

namespace mcov {
// Vom Besitzer synchronisiert. GPU-Capture und Sampler duerfen in beliebiger
// Reihenfolge fertig werden; die Zuordnung erfolgt ausschliesslich per Epoche.
class DepthSnapshots final {
    std::array<WorldSnapshot,4> history_{};
    std::size_t next_{};
public:
    void Put(const WorldSnapshot& snapshot) {
        if(!snapshot.renderEpoch||(snapshot.renderEpoch&1)) return;
        for(auto& entry:history_) if(entry.renderEpoch==snapshot.renderEpoch) { entry=snapshot; return; }
        history_[next_]=snapshot; next_=(next_+1)%history_.size();
    }
    bool Get(std::uint64_t epoch,WorldSnapshot& out) const {
        if(!epoch||(epoch&1)) return false;
        for(const auto& entry:history_) if(entry.renderEpoch==epoch) { out=entry; return true; }
        return false;
    }
    std::uint64_t Latest() const {
        std::uint64_t latest=0; for(const auto& entry:history_) if(entry.renderEpoch>latest) latest=entry.renderEpoch;
        return latest;
    }
};
}

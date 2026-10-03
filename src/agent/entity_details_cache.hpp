#pragma once
#include "../common/shared_state.hpp"
#include <array>

namespace mcov {
// Begrenzter Cache ohne Java-Referenzen. Weltwechsel und veraltete Einträge
// dürfen nie Ausrüstung/Ping einer anderen Entität auf einem Nametag anzeigen.
class EntityDetailsCache final {
    struct Entry {
        EntityDetailsSnapshot data;
        std::uint64_t tick{};
        std::uint32_t id{};
        bool valid{};
    };
    std::array<Entry,kMaxEntities> entries_{};
    std::size_t next_{};
public:
    void Clear() noexcept { for (auto& e:entries_) e.valid=false; next_=0; }
    const EntityDetailsSnapshot* Find(std::uint32_t id,std::uint64_t now,
                                      std::uint64_t maxAge=5000) const noexcept {
        for (const auto& e:entries_)
            if (e.valid && e.id==id && now>=e.tick && now-e.tick<=maxAge) return &e.data;
        return nullptr;
    }
    void Put(std::uint32_t id,std::uint64_t now,const EntityDetailsSnapshot& data) noexcept {
        for (auto& e:entries_) if (e.valid && e.id==id) { e.data=data; e.tick=now; return; }
        entries_[next_]={data,now,id,true}; next_=(next_+1)%entries_.size();
    }
};
} // namespace mcov

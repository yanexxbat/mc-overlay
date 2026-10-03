#pragma once
#include "../combat/target_selector.hpp"
#include "../common/combat_bridge_state.hpp"

namespace mcov {
class CombatMenu {
public:
    enum class Section { KillAura, Modules, Targets, Rotation, Attacks, Sync, Friends, Debug };
    Section DrawNavigation();
    float NavigationHeight() const { return navigationHeight_; }
    Section Current() const { return section_; }
    void DrawSettings();
    bool DrawKillAura();
    void Report(const CombatReport& report) { report_=report; }
    void Reset();
    void Select(Section section) { section_=section; selectNext_=true; }
    combat::ConfigStore& Store() { return store_; }
    const combat::ConfigStore& Store() const { return store_; }
    combat::FriendEnemyBook& Relations() { return relations_; }
    // UUIDs stammen von Spielern, niemals von wiederverwendeten Entity-IDs.
    static std::optional<combat::Identity> ParseUuid(const char* text);
private:
    void DrawFriends();
    combat::ConfigStore store_{};
    combat::FriendEnemyBook relations_{};
    CombatReport report_{};
    Section section_{Section::KillAura};
    bool selectNext_{};
    float navigationHeight_{};
    char uuid_[40]{};
    int relation_{};
    const char* message_{};
};
} // namespace mcov

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
    void Report(const CombatReport& report) {
        report_=report;
        if(report.debug.target) lastTarget_=report.debug.target;
        else if(report.status==CombatBridgeStatus::NoWorld) lastTarget_.reset();
    }
    void Reset();
    void Select(Section section) {
        section_=section; selectNext_=true;
        advancedOpen_=section!=Section::KillAura && section!=Section::Modules && section!=Section::Friends;
        if(advancedOpen_) { advancedSection_=section; selectAdvanced_=true; }
    }
    combat::ConfigStore& Store() { return store_; }
    const combat::ConfigStore& Store() const { return store_; }
    combat::FriendEnemyBook& Relations() { return relations_; }
    // UUIDs stammen von Spielern, niemals von wiederverwendeten Entity-IDs.
    static std::optional<combat::Identity> ParseUuid(const char* text);
private:
    void DrawFriends();
    void DrawAdvancedSettings();
    combat::ConfigStore store_{};
    combat::FriendEnemyBook relations_{};
    CombatReport report_{};
    std::optional<combat::Target> lastTarget_{};
    Section section_{Section::KillAura};
    bool selectNext_{};
    Section advancedSection_{Section::Rotation};
    bool advancedOpen_{},selectAdvanced_{};
    float navigationHeight_{};
    char uuid_[40]{};
    const char* message_{};
};
} // namespace mcov

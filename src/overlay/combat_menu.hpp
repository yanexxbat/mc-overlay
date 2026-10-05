#pragma once
#include "../combat/target_selector.hpp"
#include "../common/combat_bridge_state.hpp"

namespace mcov {
class CombatMenu {
public:
    enum class Section { KillAura, AimAssist, Triggerbot, AutoTotem, Friends, CrystalMacro, AnchorMacro, Debug };
    CombatMenu() { store_.Publish(combat::MakeKillAuraConfig({})); }
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
        diagnosticsOpen_=section==Section::Debug;
    }
    combat::ConfigStore& Store() { return store_; }
    const combat::ConfigStore& Store() const { return store_; }
    combat::FriendEnemyBook& Relations() { return relations_; }
    // UUIDs stammen von Spielern, niemals von wiederverwendeten Entity-IDs.
    static std::optional<combat::Identity> ParseUuid(const char* text);
private:
    void DrawFriends();
    void DrawDiagnostics();
    combat::ConfigStore store_{};
    combat::FriendEnemyBook relations_{};
    CombatReport report_{};
    std::optional<combat::Target> lastTarget_{};
    Section section_{Section::KillAura};
    bool selectNext_{};
    bool diagnosticsOpen_{};
    float navigationHeight_{};
    char uuid_[40]{};
    const char* message_{};
};
} // namespace mcov

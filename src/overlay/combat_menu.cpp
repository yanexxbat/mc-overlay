#include "combat_menu.hpp"
#include "control_panel.hpp"
#include "../common/flat_style.hpp"
#include <cstdio>
#include <cstring>

namespace mcov {
namespace {
namespace panel=control_panel;
bool Number(const char* label,double& value,double low,double high,const char* format="%.2f",double scale=1) {
    double displayed=value*scale;
    if(!panel::SliderDoubleRow(label,&displayed,low*scale,high*scale,format)) return false;
    value=std::clamp(displayed/scale,low,high); return true;
}
template<class Draw> void Columns(const char* id,Draw draw) {
    const int count=ImGui::GetContentRegionAvail().x>=540 ? 2 : 1;
    style::StyleScope spacing; spacing.Var(ImGuiStyleVar_CellPadding,ImVec2(4*panel::Scale(),0));
    if(!ImGui::BeginTable(id,count,ImGuiTableFlags_SizingStretchSame)) return;
    for(int group=0;group<2;++group) { ImGui::TableNextColumn(); draw(group); }
    ImGui::EndTable();
}
void Header(const char* text) { panel::Section(text); }
}
CombatMenu::Section CombatMenu::DrawNavigation() {
    constexpr const char* labels[]={"Kill Aura","Aim Assist","Triggerbot","Auto Totem","Friends","Crystal Macro","Anchor Macro","Hit sounds","Spinbot","Auto Trident"};
    const auto requested=section_==Section::Debug ? Section::KillAura : section_;
    const float start=ImGui::GetCursorPosY();
    const auto selected=static_cast<Section>(panel::SubTabs("combat_sections",labels,IM_ARRAYSIZE(labels),static_cast<int>(requested),selectNext_));
    if(selected!=Section::KillAura || section_!=Section::Debug) section_=selected;
    selectNext_=false;
    navigationHeight_=ImGui::GetCursorPosY()-start;
    return section_;
}
void CombatMenu::DrawSettings() {
    if(section_==Section::Friends) { DrawFriends(); return; }
    DrawKillAura();
    ImGui::Spacing();
    ImGui::SetNextItemOpen(diagnosticsOpen_,ImGuiCond_Always);
    diagnosticsOpen_=ImGui::CollapsingHeader("Diagnostics");
    section_=diagnosticsOpen_ ? Section::Debug : Section::KillAura;
    if(diagnosticsOpen_) DrawDiagnostics();
}
void CombatMenu::DrawDiagnostics() {
    auto next=combat::MakeKillAuraConfig(store_.Read().values);
    bool changed=false;
    Columns("combat_diagnostics",[&](int column) {
        Header(column==0 ? "HUD & traces" : "Live state");
        if(!panel::BeginRows(column==0 ? "combat_hud" : "combat_config")) return;
        if(column==0) {
            changed|=panel::CheckRow("Debug HUD",&next.hud.enabled);
            changed|=panel::CheckRow("Target information",&next.hud.target);
            changed|=panel::CheckRow("Rotation trace",&next.hud.trace);
            changed|=panel::CheckRow("Command statistics",&next.hud.packetStats);
        } else {
            panel::ValueRow("Rotation","Direct / every game tick");
            panel::ValueRow("Target lock","Until death or target loss");
            panel::ValueRow("Attack timing","Minecraft cooldown");
            char hits[32]; std::snprintf(hits,sizeof(hits),"%llu",static_cast<unsigned long long>(report_.attacks));
            panel::ValueRow("Attacks executed",hits);
        }
        ImGui::EndTable();
    });
    if(changed) message_=store_.Publish(next) ? nullptr : "Invalid configuration. Previous values were kept.";
    if(message_) ImGui::TextWrapped("%s",message_);
}
bool CombatMenu::DrawKillAura() {
    ImGui::TextWrapped("Lock the nearest target in reach. Keep aiming and attacking until it dies or leaves sight/reach.");
    auto next=combat::MakeKillAuraConfig(store_.Read().values);
    bool changed=false;
    if(panel::BeginRows("kill_aura_controls")) {
        if(panel::CheckRow("Enable Kill Aura",&next.enabled)) {
            changed=true;
        }
        constexpr const char* filters[]={"Players and mobs","Players only"};
        int filter=next.target.playersOnly ? 1 : 0;
        if(panel::ComboRow("Targets",&filter,filters,IM_ARRAYSIZE(filters))) { next.target.playersOnly=filter==1; changed=true; }
        if(Number("Reach",next.attack.range,.1,6,"%.1f m")) { next.target.range=next.attack.range; changed=true; }
        panel::ValueRow("Status",CombatBridgeText(report_.status));
        char target[112]="None yet";
        if(lastTarget_) std::snprintf(target,sizeof(target),"%.*s (%.1f m)",static_cast<int>(lastTarget_->name.size()),lastTarget_->name.data(),lastTarget_->range);
        panel::ValueRow("Last target",target);
        ImGui::EndTable();
    }
    ImGui::TextWrapped("Close the menu to run. Camera stays independent; friends are skipped. Minecraft controls reach and attack cooldown.");
    if(changed) store_.Publish(next);
    return store_.Read().values.enabled;
}
std::optional<combat::Identity> CombatMenu::ParseUuid(const char* text) {
    if(!text) return {};
    const auto length=std::strlen(text); if(length!=32 && length!=36) return {};
    combat::Identity id; std::size_t digits=0;
    for(std::size_t i=0;i<length;++i) {
        if(length==36 && (i==8 || i==13 || i==18 || i==23)) { if(text[i]!='-') return {}; continue; }
        const char c=text[i]; const int value=c>='0' && c<='9' ? c-'0' : c>='a' && c<='f' ? c-'a'+10 : c>='A' && c<='F' ? c-'A'+10 : -1;
        if(value<0) return {};
        auto& half=digits<16 ? id.high : id.low; half=(half<<4)|static_cast<std::uint64_t>(value); ++digits;
    }
    if(digits!=32 || (id.high==0 && id.low==0)) return {};
    return id;
}
void CombatMenu::DrawFriends() {
    ImGui::TextWrapped("Friends are never targeted. Add their player UUID below.");
    ImGui::TextWrapped("Friends automatically wear a translucent glowing hat. Remove a friend to hide it.");
    ImGui::SetNextItemWidth(std::min(360.f,ImGui::GetContentRegionAvail().x));
    ImGui::InputText("##player_uuid",uuid_,sizeof(uuid_));
    ImGui::TextDisabled("Player UUID, with or without hyphens");
    if(ImGui::Button("Add friend")) {
        const auto id=ParseUuid(uuid_);
        if(!id) message_="Enter a valid player UUID.";
        else if(!relations_.Set(*id,combat::Relation::Friend)) message_="The friend list is full.";
        else { message_=nullptr; uuid_[0]='\0'; }
    }
    if(message_) ImGui::TextWrapped("%s",message_);
    const auto entries=relations_.Read(); std::size_t count=0;
    for(const auto& entry:entries) if(entry.used) ++count;
    if(!count) { ImGui::TextDisabled("No friends added yet."); return; }
    Header("Saved players");
    for(std::size_t i=0;i<entries.size();++i) if(entries[i].used) {
        const auto& e=entries[i]; ImGui::PushID(static_cast<int>(i));
        ImGui::Text("%s",e.relation==combat::Relation::Friend ? "Friend" : "Enemy"); ImGui::SameLine();
        if(ImGui::SmallButton("Remove")) relations_.Set(e.uuid,combat::Relation::Neutral);
        ImGui::TextDisabled("%016llx%016llx",static_cast<unsigned long long>(e.uuid.high),static_cast<unsigned long long>(e.uuid.low));
        ImGui::PopID();
    }
}
void CombatMenu::Reset() {
    store_.Publish(combat::MakeKillAuraConfig({}));
    for(const auto& entry:relations_.Read()) if(entry.used) relations_.Set(entry.uuid,combat::Relation::Neutral);
    uuid_[0]='\0'; message_=nullptr; lastTarget_.reset(); Select(Section::KillAura);
}
} // namespace mcov

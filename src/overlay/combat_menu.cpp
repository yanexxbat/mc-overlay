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
bool Range(const char* label,combat::Range<double>& range,double low,double high,const char* format,double scale=1) {
    char text[96]; bool changed=false;
    std::snprintf(text,sizeof(text),"%s min",label);
    if(Number(text,range.min,low,high,format,scale)) { range.max=std::max(range.max,range.min); changed=true; }
    std::snprintf(text,sizeof(text),"%s max",label);
    if(Number(text,range.max,low,high,format,scale)) { range.min=std::min(range.min,range.max); changed=true; }
    return changed;
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
    constexpr const char* labels[]={"Kill Aura","Modules","Targets","Rotation","Attacks","Sync","Friends","Debug"};
    style::StyleScope colors;
    const auto requested=section_;
    const float start=ImGui::GetCursorPosY();
    colors.Color(ImGuiCol_TabSelected,ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));
    colors.Color(ImGuiCol_TabHovered,ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
    colors.Color(ImGuiCol_TabSelectedOverline,ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));
    if(ImGui::BeginTabBar("combat_sections",ImGuiTabBarFlags_FittingPolicyScroll)) {
        for(int i=0;i<IM_ARRAYSIZE(labels);++i) {
            const auto flags=selectNext_ && static_cast<int>(requested)==i ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
            if(ImGui::BeginTabItem(labels[i],nullptr,flags)) {
                section_=static_cast<Section>(i); ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar(); selectNext_=false;
    }
    ImGui::Dummy(ImVec2(0,5*panel::Scale()));
    navigationHeight_=ImGui::GetCursorPosY()-start;
    return section_;
}
void CombatMenu::DrawSettings() {
    ImGui::TextWrapped("Kill Aura: automatic target selection, visible rotation and cooldown-based attacks.");
    if(section_==Section::KillAura) { DrawKillAura(); return; }
    if(section_==Section::Friends) { DrawFriends(); return; }
    auto next=store_.Read().values;
    bool changed=false;
    Columns("combat_assist_settings",[&](int column) {
        switch(section_) {
            case Section::Targets: {
                auto& t=next.target;
                Header(column==0 ? "Target selection" : "Priority & switching");
                if(!panel::BeginRows(column==0 ? "target_selection" : "target_priority")) break;
                if(column==0) {
                    changed|=panel::CheckRow("Target selector",&t.enabled);
                    constexpr const char* modes[]={"Crosshair","Nearest player","Lowest HP","Angle + distance"};
                    int mode=static_cast<int>(t.mode);
                    if(panel::ComboRow("Selection mode",&mode,modes,IM_ARRAYSIZE(modes))) { t.mode=static_cast<combat::TargetMode>(mode); changed=true; }
                    changed|=panel::CheckRow("Players only",&t.playersOnly);
                    changed|=panel::CheckRow("Visibility check",&t.requireVisibility);
                    constexpr const char* fovModes[]={"Dynamic angle","Static pixels"};
                    int fov=static_cast<int>(t.fovMode);
                    if(panel::ComboRow("FOV mode",&fov,fovModes,IM_ARRAYSIZE(fovModes))) { t.fovMode=static_cast<combat::FovMode>(fov); changed=true; }
                    if(t.fovMode==combat::FovMode::DynamicDegrees) changed|=Number("FOV half-angle",t.coneDegrees,1,180,"%.0f deg");
                    else changed|=Number("FOV radius",t.radiusPixels,10,2000,"%.0f px");
                    changed|=Number("Selection range",t.range,.1,64,"%.1f m");
                    changed|=Number("Aim height",t.aimHeight,.1,.95,"%.0f%%",100);
                } else {
                    changed|=panel::CheckRow("Enemy priority",&t.prioritizeEnemies);
                    changed|=panel::CheckRow("Multi-target",&t.multiTarget);
                    changed|=Number("Angle weight",t.angleWeight,0,1);
                    changed|=Number("Distance weight",t.distanceWeight,0,1);
                    changed|=Number("Visibility age",t.visibilityMaxAge,.001,.5,"%.0f ms",1000);
                    changed|=Number("Switch improvement",t.switchImprovement,0,1);
                    changed|=Range("Switch delay",t.switchDelay,0,1,"%.0f ms",1000);
                }
                ImGui::EndTable(); break;
            }
            case Section::Rotation: {
                auto& r=next.rotation;
                Header(column==0 ? "Bezier & reaction" : "Overshoot & jitter");
                if(!panel::BeginRows(column==0 ? "rotation_curve" : "rotation_noise")) break;
                if(column==0) {
                    changed|=panel::CheckRow("Rotation engine",&r.enabled);
                    changed|=Number("Smoothing",r.smoothing,.25,4);
                    changed|=Range("Reaction",r.reactionDelay,.010,.040,"%.0f ms",1000);
                    changed|=Range("Yaw duration",r.yawDuration,.010,2,"%.0f ms",1000);
                    changed|=Range("Pitch duration",r.pitchDuration,.010,2,"%.0f ms",1000);
                    changed|=Number("Yaw per tick",r.maxYawPerTick,.1,90,"%.1f deg");
                    changed|=Number("Pitch per tick",r.maxPitchPerTick,.1,90,"%.1f deg");
                    changed|=Number("Tick duration",r.tickSeconds,.010,.2,"%.0f ms",1000);
                    changed|=Number("Curve tangent",r.tangentFraction,0,1);
                    changed|=Number("Replan angle",r.replanDegrees,.01,20,"%.2f deg");
                    changed|=Number("Settle angle",r.settleDegrees,0,1,"%.2f deg");
                } else {
                    changed|=Range("Overshoot",r.overshootFraction,0,.2,"%.1f%%",100);
                    changed|=Number("Overshoot cap",r.maxOvershootDegrees,0,3,"%.2f deg");
                    changed|=Number("Correction time",r.correctionDuration,.010,1,"%.0f ms",1000);
                    changed|=Range("Jitter",r.jitterDegrees,0,.5,"%.2f deg");
                    changed|=Range("Jitter period",r.jitterPeriod,.010,1,"%.0f ms",1000);
                }
                ImGui::EndTable(); break;
            }
            case Section::Attacks: {
                auto& a=next.attack;
                Header(column==0 ? "Attack controller" : "Timing & silent rotation");
                if(!panel::BeginRows(column==0 ? "attack_logic" : "attack_timing")) break;
                if(column==0) {
                    changed|=panel::CheckRow("Attack controller",&a.enabled);
                    changed|=Number("Attack range",a.range,.1,6,"%.1f m");
                    changed|=Number("Cooldown threshold",a.cooldownThreshold,.8,1,"%.0f%%",100);
                    changed|=Number("Alignment tolerance",a.alignmentDegrees,.1,5,"%.1f deg");
                    panel::ValueRow("Reach limit","Game attack reach");
                } else {
                    changed|=Range("First click",a.firstClickDelay,0,.5,"%.0f ms",1000);
                    ImGui::BeginDisabled(); panel::CheckRow("Silent rotation",&a.silentRotation); ImGui::EndDisabled();
                    panel::ValueRow("Rotation adapter","Visible camera only");
                    panel::ValueRow("On target loss","Cancel pending attack");
                    panel::ValueRow("Cooldown source","Required from game");
                }
                ImGui::EndTable(); break;
            }
            case Section::Sync: {
                auto& s=next.sync;
                Header(column==0 ? "Queue & intervals" : "Desync recovery");
                if(!panel::BeginRows(column==0 ? "sync_queue" : "sync_recovery")) break;
                if(column==0) {
                    changed|=panel::CheckRow("Sync layer",&s.enabled);
                    changed|=Range("Send interval",s.interval,.010,.2,"%.0f ms",1000);
                    changed|=Number("Pose max age",s.poseMaxAge,.010,.5,"%.0f ms",1000);
                    changed|=Number("Attack max age",s.attackMaxAge,.010,.2,"%.0f ms",1000);
                    panel::ValueRow("Queue capacity","128 commands");
                    panel::ValueRow("Order","FIFO / pose coalescing");
                } else {
                    changed|=Number("Position difference",s.positionDesync,.01,8,"%.2f m");
                    changed|=Number("Angle difference",s.angleDesync,.1,90,"%.1f deg");
                    changed|=Number("Correction max age",s.correctionMaxAge,.010,1,"%.0f ms",1000);
                    changed|=Number("Recovery time",s.recoverySeconds,.010,1,"%.0f ms",1000);
                    changed|=Number("Recovery speed",s.recoverySpeedScale,.05,1,"%.0f%%",100);
                }
                ImGui::EndTable(); break;
            }
            case Section::Debug: {
                Header(column==0 ? "HUD & traces" : "Configuration");
                if(!panel::BeginRows(column==0 ? "combat_hud" : "combat_config")) break;
                if(column==0) {
                    changed|=panel::CheckRow("Debug HUD",&next.hud.enabled);
                    changed|=panel::CheckRow("Target information",&next.hud.target);
                    changed|=panel::CheckRow("FOV circle",&next.hud.fov);
                    changed|=panel::CheckRow("Rotation trace",&next.hud.trace);
                    changed|=panel::CheckRow("Packet statistics",&next.hud.packetStats);
                } else {
                    changed|=panel::CheckRow("Enable Kill Aura",&next.enabled);
                    ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TextUnformatted("Random seed"); ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1);
                    changed|=ImGui::InputScalar("##combat_seed",ImGuiDataType_U64,&next.seed);
                    changed|=Number("Snapshot max age",next.frameMaxAge,.001,.5,"%.0f ms",1000);
                    panel::ValueRow("Hot reload","On the next assist step");
                    panel::ValueRow("Live bridge",CombatBridgeText(report_.status));
                }
                ImGui::EndTable(); break;
            }
            case Section::Modules: case Section::Friends: case Section::KillAura: break;
        }
    });
    if(changed) message_=store_.Publish(next) ? nullptr : "Invalid configuration. Previous values were kept.";
    if(message_) ImGui::TextWrapped("%s",message_);
}
bool CombatMenu::DrawKillAura() {
    Header("Kill Aura");
    auto next=store_.Read().values;
    bool changed=false;
    if(panel::BeginRows("kill_aura_controls")) {
        if(panel::CheckRow("Enable Kill Aura",&next.enabled)) {
            if(next.enabled) { next.target.enabled=true; next.rotation.enabled=true; next.attack.enabled=true; next.sync.enabled=true; next.attack.silentRotation=false; }
            changed=true;
        }
        changed|=panel::CheckRow("Players only",&next.target.playersOnly);
        if(Number("Attack range",next.attack.range,.1,6,"%.1f m")) { next.target.range=next.attack.range; changed=true; }
        panel::ValueRow("Status",CombatBridgeText(report_.status));
        char hits[32]; std::snprintf(hits,sizeof(hits),"%llu",static_cast<unsigned long long>(report_.attacks));
        panel::ValueRow("Attacks",hits);
        ImGui::EndTable();
    }
    if(ImGui::Button("Use Kill Aura preset")) {
        // Preset lässt Aktivierung explizit beim Benutzer; Einstellungen sind sofort sichtbar.
        const bool enabled=next.enabled; next=combat::Config{}; next.enabled=enabled;
        next.attack.enabled=true; next.target.mode=combat::TargetMode::AngleDistance;
        next.target.coneDegrees=180; next.target.range=next.attack.range=3;
        next.rotation.overshootFraction={0,0}; next.rotation.jitterDegrees={0,0};
        changed=true;
    }
    ImGui::TextDisabled("Automatic while menu is closed. 180 deg half-angle = all around.");
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
    Header("Friends / enemies");
    ImGui::SetNextItemWidth(std::min(360.f,ImGui::GetContentRegionAvail().x));
    ImGui::InputText("##player_uuid",uuid_,sizeof(uuid_));
    ImGui::TextDisabled("Player UUID, with or without hyphens");
    constexpr const char* kinds[]={"Friend","Enemy"};
    ImGui::SetNextItemWidth(140); ImGui::Combo("Relation",&relation_,kinds,IM_ARRAYSIZE(kinds));
    if(ImGui::Button("Add / update")) {
        const auto id=ParseUuid(uuid_);
        if(!id) message_="Enter a valid player UUID.";
        else if(!relations_.Set(*id,relation_==0 ? combat::Relation::Friend : combat::Relation::Enemy)) message_="The relation list is full.";
        else { message_=nullptr; uuid_[0]='\0'; }
    }
    if(message_) ImGui::TextWrapped("%s",message_);
    const auto entries=relations_.Read(); std::size_t count=0;
    for(const auto& entry:entries) if(entry.used) ++count;
    if(!count) { ImGui::TextDisabled("No friends or enemies added."); return; }
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
    store_.Publish(combat::Config{});
    for(const auto& entry:relations_.Read()) if(entry.used) relations_.Set(entry.uuid,combat::Relation::Neutral);
    uuid_[0]='\0'; relation_=0; message_=nullptr; Select(Section::Modules);
}
} // namespace mcov

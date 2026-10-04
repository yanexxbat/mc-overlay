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
    constexpr const char* labels[]={"Kill Aura","Other modules","Friends"};
    constexpr Section pages[]={Section::KillAura,Section::Modules,Section::Friends};
    style::StyleScope colors;
    const auto requested=section_==Section::Modules || section_==Section::Friends ? section_ : Section::KillAura;
    const float start=ImGui::GetCursorPosY();
    colors.Color(ImGuiCol_TabSelected,ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));
    colors.Color(ImGuiCol_TabHovered,ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
    colors.Color(ImGuiCol_TabSelectedOverline,ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));
    if(ImGui::BeginTabBar("combat_sections",ImGuiTabBarFlags_FittingPolicyScroll)) {
        for(int i=0;i<IM_ARRAYSIZE(labels);++i) {
            const auto flags=selectNext_ && requested==pages[i] ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
            if(ImGui::BeginTabItem(labels[i],nullptr,flags)) {
                if(pages[i]!=Section::KillAura || section_==Section::Modules || section_==Section::Friends) section_=pages[i];
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar(); selectNext_=false;
    }
    ImGui::Dummy(ImVec2(0,5*panel::Scale()));
    navigationHeight_=ImGui::GetCursorPosY()-start;
    return section_;
}
void CombatMenu::DrawSettings() {
    if(section_==Section::Friends) { DrawFriends(); return; }
    DrawKillAura();
    ImGui::Spacing();
    const bool wasOpen=advancedOpen_;
    ImGui::SetNextItemOpen(advancedOpen_,ImGuiCond_Always);
    advancedOpen_=ImGui::CollapsingHeader("Advanced settings");
    if(!advancedOpen_) { section_=Section::KillAura; return; }
    if(!wasOpen) selectAdvanced_=true;
    constexpr const char* labels[]={"Targeting","Rotation","Attacks","Timing","Diagnostics"};
    constexpr Section pages[]={Section::Targets,Section::Rotation,Section::Attacks,Section::Sync,Section::Debug};
    const auto requested=advancedSection_;
    if(ImGui::BeginTabBar("combat_advanced_sections",ImGuiTabBarFlags_FittingPolicyScroll)) {
        for(int i=0;i<IM_ARRAYSIZE(pages);++i) {
            const auto flags=selectAdvanced_ && requested==pages[i] ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
            if(ImGui::BeginTabItem(labels[i],nullptr,flags)) { advancedSection_=pages[i]; ImGui::EndTabItem(); }
        }
        ImGui::EndTabBar(); selectAdvanced_=false;
    }
    section_=advancedSection_;
    DrawAdvancedSettings();
}
void CombatMenu::DrawAdvancedSettings() {
    auto next=store_.Read().values;
    bool changed=false;
    Columns("combat_assist_settings",[&](int column) {
        switch(section_) {
            case Section::Targets: {
                auto& t=next.target;
                Header(column==0 ? "Selection rules" : "Aim point");
                if(!panel::BeginRows(column==0 ? "target_selection" : "target_priority")) break;
                if(column==0) {
                    panel::ValueRow("Target","Nearest living entity");
                    panel::ValueRow("Search","All around you");
                    panel::ValueRow("Sight","Clear line of sight required");
                    panel::ValueRow("Friends","Always ignored");
                } else {
                    changed|=Number("Aim height",t.aimHeight,.1,.95,"%.0f%%",100);
                    changed|=Number("Visibility age",t.visibilityMaxAge,.001,.5,"%.0f ms",1000);
                }
                ImGui::EndTable(); break;
            }
            case Section::Rotation: {
                auto& r=next.rotation;
                Header(column==0 ? "Bezier & reaction" : "Overshoot & jitter");
                if(!panel::BeginRows(column==0 ? "rotation_curve" : "rotation_noise")) break;
                if(column==0) {
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
                Header(column==0 ? "Attack controller" : "Timing");
                if(!panel::BeginRows(column==0 ? "attack_logic" : "attack_timing")) break;
                if(column==0) {
                    changed|=Number("Cooldown threshold",a.cooldownThreshold,.8,1,"%.0f%%",100);
                    changed|=Number("Alignment tolerance",a.alignmentDegrees,.1,5,"%.1f deg");
                    panel::ValueRow("Reach limit","Game attack reach");
                } else {
                    changed|=Range("First click",a.firstClickDelay,0,.5,"%.0f ms",1000);
                    panel::ValueRow("Rotation adapter","Player aims / camera independent");
                    panel::ValueRow("On target loss","Cancel pending attack");
                    panel::ValueRow("Cooldown source","Required from game");
                }
                ImGui::EndTable(); break;
            }
            case Section::Sync: {
                auto& s=next.sync;
                Header(column==0 ? "Action timing" : "Local commands");
                if(!panel::BeginRows(column==0 ? "sync_queue" : "sync_statistics")) break;
                if(column==0) {
                    changed|=Range("Action interval",s.interval,.010,.2,"%.0f ms",1000);
                    changed|=Number("Pose max age",s.poseMaxAge,.010,.5,"%.0f ms",1000);
                    changed|=Number("Attack max age",s.attackMaxAge,.010,.2,"%.0f ms",1000);
                    panel::ValueRow("Queue capacity","128 commands");
                    panel::ValueRow("Order","FIFO / pose coalescing");
                } else {
                    const auto& stats=report_.debug.packets;
                    const char* labels[]={"Buffered","Executed","Discarded","Failed"};
                    const unsigned long long values[]={static_cast<unsigned long long>(stats.buffered),
                        static_cast<unsigned long long>(stats.sent),static_cast<unsigned long long>(stats.discarded),
                        static_cast<unsigned long long>(stats.failed)};
                    for(int i=0;i<IM_ARRAYSIZE(labels);++i) {
                        char count[32]; std::snprintf(count,sizeof(count),"%llu",values[i]);
                        panel::ValueRow(labels[i],count);
                    }
                }
                ImGui::EndTable(); break;
            }
            case Section::Debug: {
                Header(column==0 ? "HUD & traces" : "Configuration");
                if(!panel::BeginRows(column==0 ? "combat_hud" : "combat_config")) break;
                if(column==0) {
                    changed|=panel::CheckRow("Debug HUD",&next.hud.enabled);
                    changed|=panel::CheckRow("Target information",&next.hud.target);
                    changed|=panel::CheckRow("Rotation trace",&next.hud.trace);
                    changed|=panel::CheckRow("Command statistics",&next.hud.packetStats);
                } else {
                    ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TextUnformatted("Random seed"); ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1);
                    changed|=ImGui::InputScalar("##combat_seed",ImGuiDataType_U64,&next.seed);
                    changed|=Number("Snapshot max age",next.frameMaxAge,.001,.5,"%.0f ms",1000);
                    panel::ValueRow("Hot reload","On the next assist step");
                    panel::ValueRow("Live bridge",CombatBridgeText(report_.status));
                    char hits[32]; std::snprintf(hits,sizeof(hits),"%llu",static_cast<unsigned long long>(report_.attacks));
                    panel::ValueRow("Attacks executed",hits);
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
    ImGui::TextWrapped("Aim and attack the nearest living target in reach. Your camera stays independent.");
    auto next=store_.Read().values;
    bool changed=false;
    if(panel::BeginRows("kill_aura_controls")) {
        if(panel::CheckRow("Enable Kill Aura",&next.enabled)) {
            if(next.enabled) { next.target.enabled=true; next.rotation.enabled=true; next.attack.enabled=true; next.sync.enabled=true; }
            changed=true;
        }
        constexpr const char* filters[]={"Players and mobs","Players only"};
        int filter=next.target.playersOnly ? 1 : 0;
        if(panel::ComboRow("Targets",&filter,filters,IM_ARRAYSIZE(filters))) { next.target.playersOnly=filter==1; changed=true; }
        if(Number("Reach",next.attack.range,.1,6,"%.1f m")) { next.target.range=next.attack.range; changed=true; }
        changed|=Number("Smoothing",next.rotation.smoothing,.25,4,"%.1f x");
        panel::ValueRow("Status",CombatBridgeText(report_.status));
        char target[112]="None yet";
        if(lastTarget_) std::snprintf(target,sizeof(target),"%.*s (%.1f m)",static_cast<int>(lastTarget_->name.size()),lastTarget_->name.data(),lastTarget_->range);
        panel::ValueRow("Last target",target);
        ImGui::EndTable();
    }
    ImGui::TextWrapped("Close the menu to run. Friends are skipped; Minecraft limits reach and attack cooldown.");
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
    store_.Publish(combat::Config{});
    for(const auto& entry:relations_.Read()) if(entry.used) relations_.Set(entry.uuid,combat::Relation::Neutral);
    uuid_[0]='\0'; message_=nullptr; lastTarget_.reset(); Select(Section::KillAura);
}
} // namespace mcov

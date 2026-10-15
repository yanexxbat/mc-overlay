#include "overlay.hpp"
namespace mcov {
void Overlay::UpdateAntiVanish() {
    const auto now=GetTickCount64();
    if(!antiVanishConfig_.enabled||panicActive_||!agentConnected_||!haveSnapshot_||!heartbeatClock_.Alive(now)||!antiVanishReport_.sampledAtMs||now<antiVanishReport_.sampledAtMs||now-antiVanishReport_.sampledAtMs>1500||antiVanishReport_.status!=antivanish::Status::Active){antiVanishPrevious_={};return;}
    if(antiVanishPrevious_.sampledAtMs==antiVanishReport_.sampledAtMs)return;
    if(antiVanishConfig_.notifications&&rockstarConfig_.Enabled(rockstar::Feature::Notifications)) {
        for(std::uint32_t i=0;i<std::min(antiVanishReport_.count,std::uint32_t(antivanish::kRows));++i){const auto& row=antiVanishReport_.rows[i];bool found=false;
            for(std::uint32_t j=0;j<std::min(antiVanishPrevious_.count,std::uint32_t(antivanish::kRows));++j)if(antiVanishPrevious_.rows[j].id==row.id&&antiVanishPrevious_.rows[j].evidence==row.evidence){found=true;break;}
            if(!found){char text[112];const char* name=rockstar::Pack::DisplayName(row.name,rockstarConfig_.Enabled(rockstar::Feature::NameProtect),true);std::snprintf(text,sizeof(text),"Anti Vanish: %s - %s",name,row.evidence&1?"hidden from TAB":"invisible player");rockstarPack_.Notify(text,double(now)/1000);}
        }
    }
    antiVanishPrevious_=antiVanishReport_;
}
void Overlay::DrawAntiVanishSettings() {
    auto& c=antiVanishConfig_;bool dirty=false;
    if(control_panel::BeginRows("antivanish_controls")){
        dirty|=control_panel::CheckRow("Enable Anti Vanish",&c.enabled);DrawBind(keys::Action::AntiVanish,true);
        dirty|=control_panel::SliderRow("Nearby range",&c.range,8,128,"%.0f m");
        dirty|=control_panel::CheckRow("Include invisible players",&c.invisible);
        dirty|=control_panel::CheckRow("Alerts in Notifications HUD",&c.notifications);ImGui::EndTable();
    }
    ImGui::TextWrapped("Looks for previously listed players who become hidden from TAB, including players whose nearby entity remains loaded. Checks twice per second, with a short confirmation delay.");
    ImGui::TextWrapped("These are possible vanish indicators. Servers can hide all evidence. The optional invisible-player check can also match ordinary invisibility potions.");
    if(c.notifications&&!rockstarConfig_.Enabled(rockstar::Feature::Notifications)){
        if(ImGui::Button("Enable Notifications HUD")){rockstarConfig_[rockstar::Feature::Notifications].enabled=true;hud_.visible=true;dirty=true;}
    }
    control_panel::Section("Live status");
    auto now=GetTickCount64();bool fresh=agentConnected_&&antiVanishReport_.sampledAtMs&&now>=antiVanishReport_.sampledAtMs&&now-antiVanishReport_.sampledAtMs<=1500;
    if(!c.enabled)ImGui::TextWrapped("Anti Vanish is off.");
    else if(!agentConnected_||!haveSnapshot_)ImGui::TextWrapped("Connect to Minecraft 1.21.11 and join a world.");
    else if(std::strcmp(lastSnapshot_.gameVersion,"1.21.11"))ImGui::TextWrapped("Anti Vanish requires Minecraft 1.21.11.");
    else if(!fresh)ImGui::TextWrapped("Waiting for the updated agent. Restart Minecraft after updating.");
    else switch(antiVanishReport_.status){
        case antivanish::Status::Active:ImGui::TextWrapped("Monitoring player-list hints. Fully hidden server data cannot be detected.");break;
        case antivanish::Status::Failed:ImGui::TextWrapped("Sampling failed. Toggle Anti Vanish off and on to retry.");break;
        case antivanish::Status::Unsupported:ImGui::TextWrapped("Player-list bridge unavailable. Restart Minecraft with the updated client.");break;
        default:ImGui::TextWrapped("Waiting for a world and its player list.");break;
    }
    if(fresh&&c.enabled&&antiVanishReport_.status==antivanish::Status::Active){
        if(antiVanishReport_.limited)ImGui::TextWrapped("Large player list: monitoring is limited to 512 entries and 256 nearby entities.");
        control_panel::Section("Possible hidden players");
        if(!antiVanishReport_.count)ImGui::TextWrapped("No current indicators. This does not confirm that nobody is vanished.");
        for(std::uint32_t i=0;i<std::min(antiVanishReport_.count,std::uint32_t(antivanish::kRows));++i){const auto& r=antiVanishReport_.rows[i];ImGui::TextWrapped("%s: %s%s%s",rockstar::Pack::DisplayName(r.name,rockstarConfig_.Enabled(rockstar::Feature::NameProtect),true),r.evidence&1?"hidden from TAB":"",r.evidence&2?" / entity nearby":"",r.evidence&4?" / invisible":"");}
    }
    if(dirty){panicActive_=false;preferencesDirty_=true;preferenceSaveAt_=GetTickCount64()+500;PushScanRequest();}
}
}

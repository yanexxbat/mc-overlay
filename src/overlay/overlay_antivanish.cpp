#include "overlay.hpp"
namespace mcov {
namespace {
const char* AntiVanishName(const antivanish::Entry& row,bool protect) {
    const bool anonymous=(row.evidence&(antivanish::Sound|antivanish::Particle))&&!(row.evidence&antivanish::Nearby);
    return anonymous?row.name:rockstar::Pack::DisplayName(row.name,protect,true);
}
const char* AntiVanishTag(std::uint32_t evidence) {
    if(evidence&antivanish::Critical)return "ALERT";
    if(evidence&antivanish::Visible)return "VISIBLE";
    if(evidence&antivanish::Spectator)return "WATCH";
    if(evidence&antivanish::Invisible)return "INVIS";
    if(evidence&antivanish::Sound)return "SOUND";
    if(evidence&antivanish::Particle)return "PARTICLE";
    return "VANISH";
}
}
void Overlay::UpdateAntiVanish() {
    const auto now=GetTickCount64();
    if(!antiVanishConfig_.enabled||panicActive_||!agentConnected_||!haveSnapshot_||!heartbeatClock_.Alive(now)||!antiVanishReport_.sampledAtMs||now<antiVanishReport_.sampledAtMs||now-antiVanishReport_.sampledAtMs>1500||antiVanishReport_.status!=antivanish::Status::Active){antiVanishPrevious_={};return;}
    if(antiVanishPrevious_.sampledAtMs==antiVanishReport_.sampledAtMs)return;
    if(now-antiVanishNoticeWindow_>=4000){antiVanishNoticeWindow_=now;antiVanishNoticeCount_=0;}
    if(antiVanishConfig_.notifications&&rockstarConfig_.Enabled(rockstar::Feature::Notifications)) {
        for(std::uint32_t i=0;i<std::min(antiVanishReport_.count,std::uint32_t(antivanish::kRows));++i){const auto& row=antiVanishReport_.rows[i];bool found=false;
            for(std::uint32_t j=0;j<std::min(antiVanishPrevious_.count,std::uint32_t(antivanish::kRows));++j)if(antiVanishPrevious_.rows[j].id==row.id&&antiVanishPrevious_.rows[j].evidence==row.evidence){found=true;break;}
            if(!found&&antiVanishNoticeCount_<6){++antiVanishNoticeCount_;char text[176];const char* name=AntiVanishName(row,rockstarConfig_.Enabled(rockstar::Feature::NameProtect));std::snprintf(text,sizeof(text),"Anti Vanish: %s - %s",name,row.reason);rockstarPack_.Notify(text,double(now)/1000);}
        }
    }
    antiVanishPrevious_=antiVanishReport_;
}
void Overlay::DrawAntiVanishHud() {
    const auto now=GetTickCount64();
    if(!antiVanishConfig_.enabled||panicActive_||!agentConnected_||!haveSnapshot_||!heartbeatClock_.Alive(now)||antiVanishReport_.status!=antivanish::Status::Active||!antiVanishReport_.sampledAtMs||now<antiVanishReport_.sampledAtMs||now-antiVanishReport_.sampledAtMs>1500)return;
    std::array<const antivanish::Entry*,5> rows{};int count=0,regular=0;bool critical=false;
    for(std::uint32_t i=0;i<std::min(antiVanishReport_.count,std::uint32_t(antivanish::kRows));++i){const auto& r=antiVanishReport_.rows[i];
        if(r.evidence&antivanish::Critical){if(critical)continue;critical=true;}else if(regular++>=4)continue;
        rows[count++]=&r;if(count==5)break;
    }
    if(!count)return;
    ImGui::PushFont(style::hud::Font());
    auto* draw=ImGui::GetBackgroundDrawList();const auto viewport=ImGui::GetIO().DisplaySize;
    const float width=std::min(380.f,viewport.x-24.f),line=ImGui::GetTextLineHeight(),rowHeight=line*2+10;
    const ImVec2 start{std::max(12.f,(viewport.x-width)*.5f),54};
    const ImVec2 end{start.x+width,std::min(viewport.y-12,start.y+line+20+count*rowHeight)};
    style::hud::Panel(*draw,start,end);
    draw->PushClipRect({start.x+10,start.y+6},{end.x-10,end.y-6},true);
    style::hud::Label(*draw,{start.x+12,start.y+9},style::hud::Muted,"Anti Vanish");
    float y=start.y+line+19;
    for(int i=0;i<count;++i){const auto& r=*rows[i];char title[96];
        std::snprintf(title,sizeof(title),"%s  %s",AntiVanishTag(r.evidence),AntiVanishName(r,rockstarConfig_.Enabled(rockstar::Feature::NameProtect)));
        const auto color=r.evidence&antivanish::Critical?style::hud::Danger:r.evidence&antivanish::Visible?style::hud::Text:style::hud::Warning;
        style::hud::Label(*draw,{start.x+12,y},color,title);style::hud::Label(*draw,{start.x+12,y+line+2},style::hud::Muted,r.reason);y+=rowHeight;
    }
    draw->PopClipRect();
    ImGui::PopFont();
}
void Overlay::DrawAntiVanishSettings() {
    auto& c=antiVanishConfig_;bool dirty=false;
    if(control_panel::BeginRows("antivanish_controls")){
        dirty|=control_panel::CheckRow("Enable Anti Vanish",&c.enabled);DrawBind(keys::Action::AntiVanish,true);
        dirty|=control_panel::SliderRow("Nearby range",&c.range,8,128,"%.0f m");
        dirty|=control_panel::CheckRow("Include invisible players",&c.invisible);
        dirty|=control_panel::CheckRow("Alerts in Notifications HUD",&c.notifications);ImGui::EndTable();
    }
    ImGui::TextWrapped("Uses only the 21 staff names from the supplied Anubis list. Checks TAB removals, spectator changes and missing staff in command suggestions every 5 seconds. No chat message is sent.");
    ImGui::TextWrapped("Sound and particle hints are filtered for visible players, your actions, containers, redstone and explosions. Unidentified hints show distance and direction; they do not identify a staff member.");
    ImGui::TextWrapped("Current hints appear at the top centre of the HUD for up to 20 seconds. These are possible vanish indicators; invisible players can also be using potions. Fully hidden server data cannot be detected.");
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
        case antivanish::Status::Active:ImGui::TextWrapped(antiVanishReport_.packetHooks?"Staff monitoring and packet sensors are active.":"Player-list checks only. Packet sensors are unavailable; restart Minecraft with the updated client.");break;
        case antivanish::Status::Failed:ImGui::TextWrapped("Sampling failed. Toggle Anti Vanish off and on to retry.");break;
        case antivanish::Status::Unsupported:ImGui::TextWrapped("Player-list bridge unavailable. Restart Minecraft with the updated client.");break;
        default:ImGui::TextWrapped("Waiting for a world and its player list.");break;
    }
    if(fresh&&c.enabled&&antiVanishReport_.status==antivanish::Status::Active){
        if(antiVanishReport_.limited)ImGui::TextWrapped("Monitoring reached its processing limit. Some roster entries or sensor events were skipped.");
        control_panel::Section("Current hints");
        if(!antiVanishReport_.count)ImGui::TextWrapped("No current indicators. This does not confirm that nobody is vanished.");
        for(std::uint32_t i=0;i<std::min(antiVanishReport_.count,std::uint32_t(antivanish::kRows));++i){const auto& r=antiVanishReport_.rows[i];ImGui::TextWrapped("%s  %s: %s",AntiVanishTag(r.evidence),AntiVanishName(r,rockstarConfig_.Enabled(rockstar::Feature::NameProtect)),r.reason);}
    }
    if(dirty){panicActive_=false;preferencesDirty_=true;preferenceSaveAt_=GetTickCount64()+500;PushScanRequest();}
}
}

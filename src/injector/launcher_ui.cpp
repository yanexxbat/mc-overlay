#include "launcher_ui.hpp"
#include "../common/compact_style.hpp"
#include "../overlay/control_panel.hpp"
#include <algorithm>

namespace mcov::launcher {
namespace {
void NavigationIcon(ImDrawList& draw,ImVec2 p,int tab,ImU32 ink) {
    if(tab==0) {
        draw.AddRect({p.x-6,p.y-5},{p.x+6,p.y+4},ink,2,0,1.25f);
        draw.AddLine({p.x-3,p.y+7},{p.x+3,p.y+7},ink,1.25f);
        draw.AddLine({p.x,p.y+4},{p.x,p.y+7},ink,1.25f);
    } else {
        draw.AddRect({p.x-5,p.y-6},{p.x+5,p.y+6},ink,2,0,1.25f);
        for(int y:{-2,2}) draw.AddLine({p.x-2,p.y+float(y)},{p.x+2,p.y+float(y)},ink,1.25f);
    }
}
void InstanceList(View& v,float height) {
    style::StyleScope listStyle;
    listStyle.Color(ImGuiCol_ChildBg,style::Color(0xFFFFFF,.025f));
    listStyle.Var(ImGuiStyleVar_WindowPadding,ImVec2(8,8));
    ImGui::BeginChild("instances",{0,height},ImGuiChildFlags_AlwaysUseWindowPadding);
    if(v.instances.empty()) {
        ImGui::TextWrapped("Kein Minecraft-Fenster gefunden. Starte das Spiel bis zum Hauptmenue und klicke auf Aktualisieren.");
    }
    ImGui::BeginDisabled(v.busy);
    for(int i=0;i<static_cast<int>(v.instances.size());++i) {
        const auto& p=v.instances[i]; ImGui::PushID(static_cast<int>(p.pid));
        const auto row=ImGui::GetCursorScreenPos();
        const float width=ImGui::GetContentRegionAvail().x;
        if(ImGui::Selectable("##instance",v.selected==i,ImGuiSelectableFlags_None,{0,43})) v.selected=i;
        auto& draw=*ImGui::GetWindowDrawList();
        const auto ink=ImGui::GetColorU32(ImGuiCol_Text);
        const auto muted=ImGui::GetColorU32(ImGuiCol_TextDisabled);
        const auto accent=ImGui::GetColorU32(ImGuiCol_CheckMark);
        draw.PushClipRect(row,{row.x+width,row.y+43},true);
        draw.AddCircle({row.x+11,row.y+14},4.5f,v.selected==i?accent:muted,16,1.25f);
        if(v.selected==i) draw.AddCircleFilled({row.x+11,row.y+14},2,accent,12);
        draw.AddText({row.x+25,row.y+5},ink,p.title.c_str());
        const std::string detail="PID "+std::to_string(p.pid)+" | "+p.java+
            (p.running?" | Client laeuft":p.connected?" | Verbunden":"");
        draw.AddText({row.x+25,row.y+23},muted,detail.c_str());
        draw.PopClipRect();
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s\n%s",p.title.c_str(),detail.c_str());
        ImGui::PopID();
    }
    ImGui::EndDisabled(); ImGui::EndChild();
}
}

Actions Render(View& v,ImVec2 size) {
    style::CompactStyle scoped; Actions a;
    const float sidebar=size.x>=620?142.f:112.f;
    v.tab=std::clamp(v.tab,0,1);
    auto& draw=*ImGui::GetBackgroundDrawList();
    const auto ink=ImGui::GetColorU32(ImGuiCol_Text);
    const auto muted=ImGui::GetColorU32(ImGuiCol_TextDisabled);
    const auto accent=ImGui::GetColorU32(ImGuiCol_CheckMark);
    draw.AddRectFilled({12,12},{size.x-12,size.y-12},ImGui::GetColorU32(ImGuiCol_WindowBg),12);
    draw.AddRectFilled({12,12},{12+sidebar,size.y-12},ImGui::GetColorU32(style::Color(0x15151B,.22f)),12,ImDrawFlags_RoundCornersLeft);
    draw.AddLine({12+sidebar,24},{12+sidebar,size.y-24},ImGui::GetColorU32(ImGuiCol_Separator));
    draw.AddLine({sidebar+32,58},{size.x-28,58},ImGui::GetColorU32(ImGuiCol_Separator));
    draw.AddCircle({33,35},6,accent,20,1.5f); draw.AddCircleFilled({33,35},2.3f,accent,16);
    draw.AddText(style::MenuFont(true),14,{48,28},ink,"mcov");
    draw.AddText(style::MenuFont(true),14,{sidebar+32,28},ink,v.tab==0?"Client starten":"Diagnose");

    // Native caption dragging owns the rest of this strip; exclude these controls.
    const ImVec2 closeMin{size.x-44,22},closeMax{size.x-22,44},minMin{size.x-70,22},minMax{size.x-48,44};
    const auto hit=[&](ImVec2 low,ImVec2 high) {
        const auto m=ImGui::GetIO().MousePos;
        const bool hover=m.x>=low.x&&m.y>=low.y&&m.x<high.x&&m.y<high.y;
        draw.AddRectFilled(low,high,ImGui::GetColorU32(hover?ImGuiCol_ButtonHovered:ImGuiCol_Button),4);
        return hover&&ImGui::IsMouseClicked(0);
    };
    a.close=hit(closeMin,closeMax); a.minimize=hit(minMin,minMax);
    draw.AddLine({size.x-37,29},{size.x-29,37},ink,1.25f); draw.AddLine({size.x-29,29},{size.x-37,37},ink,1.25f);
    draw.AddLine({size.x-64,36},{size.x-54,36},ink,1.25f);

    ImGui::SetNextWindowPos({12,58}); ImGui::SetNextWindowSize({size.x-24,size.y-70});
    style::StyleScope shell; shell.Var(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    constexpr auto flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("##mcov_launcher",nullptr,flags);
    {
        style::StyleScope padding; padding.Var(ImGuiStyleVar_WindowPadding,ImVec2(10,14));
        ImGui::BeginChild("launcher_navigation",{sidebar,0},ImGuiChildFlags_AlwaysUseWindowPadding,ImGuiWindowFlags_NoBackground);
        const char* tabs[]={"Start","Diagnose"};
        for(int i=0;i<2;++i) {
            const bool active=v.tab==i; const auto p=ImGui::GetCursorScreenPos();
            style::StyleScope selected;
            selected.Var(ImGuiStyleVar_ButtonTextAlign,ImVec2(0,.5f));
            selected.Var(ImGuiStyleVar_FramePadding,ImVec2(32,4));
            selected.Color(ImGuiCol_Button,active?style::Color(0xFFFFFF,.065f):ImVec4(0,0,0,0));
            selected.Color(ImGuiCol_Text,active?style::Color(0xF5D0C4):style::Color(0xC2C2CC));
            if(ImGui::Button(tabs[i],{-1,29})) { v.tab=i; ImGui::SetWindowFocus(); }
            NavigationIcon(*ImGui::GetWindowDrawList(),{p.x+15,p.y+14.5f},i,active?accent:muted);
        }
        ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY()+16,ImGui::GetWindowHeight()-44));
        ImGui::TextDisabled("Windows x64");
        ImGui::EndChild();
    }
    ImGui::SameLine(0,20); ImGui::BeginGroup();
    const float width=std::max(1.f,ImGui::GetContentRegionAvail().x-16);
    const float footer=v.tab==0?66.f:36.f;
    const ImVec2 contentSize{width,std::max(1.f,ImGui::GetContentRegionAvail().y-footer)};
    {
        style::StyleScope padding; padding.Var(ImGuiStyleVar_WindowPadding,ImVec2(0,14));
        ImGui::BeginChild("launcher_content",contentSize,ImGuiChildFlags_AlwaysUseWindowPadding,ImGuiWindowFlags_NoBackground);
        if(v.tab==0) {
            ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted("Minecraft auswaehlen");
            const float refreshWidth=ImGui::CalcTextSize("Aktualisieren").x+14;
            if(ImGui::GetContentRegionAvail().x>=refreshWidth+12) ImGui::SameLine(0,12);
            ImGui::BeginDisabled(v.busy);
            if(ImGui::SmallButton("Aktualisieren")) a.refresh=true;
            ImGui::EndDisabled();
            ImGui::TextWrapped("Verbinden und Client starten mit einem Klick.");
            ImGui::Dummy({0,3});
            InstanceList(v,std::clamp(size.y*.28f,96.f,180.f));
            control_panel::Section(v.busy?"Verbindung":"Status");
            if(v.busy) {
                ImGui::TextWrapped("%s",v.phase.c_str());
                ImGui::TextDisabled("Das Fenster bleibt bedienbar.");
            } else {
                style::StyleScope status;
                if(v.error) status.Color(ImGuiCol_Text,style::Color(0xFFA388));
                ImGui::TextWrapped("%s",v.status.c_str());
                if(v.needsJdk&&ImGui::Button("JDK-Ordner auswaehlen")) a.chooseJdk=true;
                if(v.error&&ImGui::SmallButton("Details anzeigen")) v.tab=1;
            }
            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
            ImGui::TextWrapped("Nach dem Start zu Minecraft wechseln. Standard-Tasten: INSERT oeffnet das Menue, F6 den HUD-Editor und END beendet den Client.");
        } else {
            control_panel::Section("Verbindungsdetails");
            ImGui::TextWrapped("%s",v.diagnostic.empty()?"Noch kein Verbindungsversuch. Waehle unter Start deine Minecraft-Instanz aus.":v.diagnostic.c_str());
            if(ImGui::Button("Protokollordner oeffnen")) a.openLogs=true;
            const float copyWidth=ImGui::CalcTextSize("Protokoll kopieren").x+14;
            if(ImGui::GetContentRegionAvail().x>=copyWidth+9) ImGui::SameLine();
            if(ImGui::Button("Protokoll kopieren")) ImGui::SetClipboardText((v.diagnostic+"\n"+v.log).c_str());
            control_panel::Section("Protokoll");
            ImGui::TextWrapped("%s",v.log.empty()?"Noch keine Eintraege.":v.log.c_str());
        }
        ImGui::EndChild();
    }
    if(v.tab==0) {
        const bool selected=v.selected>=0&&v.selected<static_cast<int>(v.instances.size());
        const bool running=selected&&v.instances[v.selected].running;
        ImGui::BeginDisabled(v.busy||!selected);
        {
            style::StyleScope primary;
            primary.Color(ImGuiCol_Button,style::Color(0xEF8B70));
            primary.Color(ImGuiCol_ButtonHovered,style::Color(0xFFA388));
            primary.Color(ImGuiCol_ButtonActive,style::Color(0xDC7D64));
            primary.Color(ImGuiCol_Text,style::Color(0x221B1B));
            if(ImGui::Button(v.busy?"Verbindung wird vorbereitet...":running?"Zu Minecraft wechseln":"Client starten",{width,34})) a.start=true;
        }
        ImGui::EndDisabled();
    }
    ImGui::TextDisabled("mcov"); ImGui::SameLine(std::max(0.f,width-80));
    if(ImGui::Button("Schliessen",{80,22})) a.close=true;
    ImGui::EndGroup(); ImGui::End(); return a;
}
} // namespace mcov::launcher

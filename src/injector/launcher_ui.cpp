#include "launcher_ui.hpp"
#include "../common/liquid_style.hpp"
#include "../common/liquid_glass.hpp"
#include "../overlay/control_panel.hpp"
#include <algorithm>

namespace mcov::launcher {
Actions Render(View& v,ImVec2 size) {
    style::LiquidStyle scoped; Actions a;
    const ImVec2 top{12,12},bottom{size.x-12,size.y-12};
    auto* draw=ImGui::GetBackgroundDrawList();
    LiquidGlass::Panel(*draw,top,bottom,22);
    draw->AddText(style::MenuHeadingFont(),style::MenuHeadingFont()->FontSize,{29,24},ImGui::GetColorU32(ImGuiCol_Text),"mcov");
    draw->AddText({102,32},ImGui::GetColorU32(ImGuiCol_TextDisabled),"Client starten");
    const ImVec2 closeMin{size.x-44,22},closeMax{size.x-22,44},minMin{size.x-70,22},minMax{size.x-48,44};
    const auto hit=[&](ImVec2 low,ImVec2 high) {
        const auto m=ImGui::GetIO().MousePos;
        const bool hover=m.x>=low.x&&m.y>=low.y&&m.x<high.x&&m.y<high.y;
        draw->AddRectFilled(low,high,ImGui::GetColorU32(hover?ImGuiCol_ButtonHovered:ImGuiCol_Button),7);
        return hover&&ImGui::IsMouseClicked(0);
    };
    a.close=hit(closeMin,closeMax); a.minimize=hit(minMin,minMax);
    const auto ink=ImGui::GetColorU32(ImGuiCol_Text);
    draw->AddLine({size.x-37,29},{size.x-29,37},ink); draw->AddLine({size.x-29,29},{size.x-37,37},ink);
    draw->AddLine({size.x-64,36},{size.x-54,36},ink);

    ImGui::SetNextWindowPos({20,58}); ImGui::SetNextWindowSize({size.x-40,size.y-80});
    ImGui::Begin("##mcov_launcher",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoSavedSettings);
    const float width=ImGui::GetContentRegionAvail().x;
    const char* tabs[]={"Start","Diagnose"};
    for(int i=0;i<2;++i) {
        if(i) ImGui::SameLine(0,6);
        style::StyleScope selected;
        if(v.tab==i) selected.Color(ImGuiCol_Button,style::Color(0x818CF8,.34f));
        if(ImGui::Button(tabs[i],{(width-6)*.5f,34})) v.tab=i;
    }
    ImGui::Dummy({0,6});
    const float remaining=std::max(40.f,ImGui::GetContentRegionAvail().y-(v.tab==0?87.f:39.f));
    ImGui::BeginChild("launcher_content",{0,remaining},ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::Indent(0);
    if(v.tab==0) {
        ImGui::TextUnformatted("Minecraft auswaehlen"); ImGui::SameLine();
        ImGui::BeginDisabled(v.busy);
        if(ImGui::SmallButton("Aktualisieren")) a.refresh=true;
        ImGui::EndDisabled();
        ImGui::TextDisabled("Verbinden und Client starten mit einem Klick.");
        const float listHeight=std::clamp(size.y*.27f,88.f,150.f);
        ImGui::BeginChild("instances",{-8,listHeight},ImGuiChildFlags_Borders);
        if(v.instances.empty()) {
            ImGui::TextWrapped("Kein Minecraft-Fenster gefunden. Starte das Spiel bis zum Hauptmenue und klicke auf Aktualisieren.");
        }
        ImGui::BeginDisabled(v.busy);
        for(int i=0;i<static_cast<int>(v.instances.size());++i) {
            const auto& p=v.instances[i]; ImGui::PushID(static_cast<int>(p.pid));
            const auto row=ImGui::GetCursorScreenPos();
            if(ImGui::Selectable("##instance",v.selected==i,ImGuiSelectableFlags_None,{0,36})) v.selected=i;
            auto* list=ImGui::GetWindowDrawList();
            list->PushClipRect(row,{row.x+ImGui::GetContentRegionAvail().x,row.y+36},true);
            list->AddText({row.x+5,row.y+2},ink,p.title.c_str());
            const std::string detail="PID "+std::to_string(p.pid)+" | "+p.java+(p.running?" | Client laeuft":p.connected?" | Verbunden":"");
            list->AddText({row.x+5,row.y+18},ImGui::GetColorU32(ImGuiCol_TextDisabled),detail.c_str());
            list->PopClipRect(); ImGui::PopID();
        }
        ImGui::EndDisabled(); ImGui::EndChild();
        control_panel::Section(v.busy?"Verbindung":"Status");
        if(v.busy) {
            ImGui::TextWrapped("%s",v.phase.c_str());
            ImGui::TextDisabled("Das Fenster bleibt bedienbar.");
        } else {
            style::StyleScope status;
            if(v.error) status.Color(ImGuiCol_Text,ImVec4(1,.62f,.58f,1));
            ImGui::TextWrapped("%s",v.status.c_str());
            if(v.needsJdk&&ImGui::Button("JDK-Ordner auswaehlen")) a.chooseJdk=true;
            if(v.error&&ImGui::SmallButton("Details anzeigen")) v.tab=1;
        }
        ImGui::Spacing();
        ImGui::TextWrapped("Nach dem Start zu Minecraft wechseln. Standard-Tasten: INSERT oeffnet das Menue, F6 den HUD-Editor und END beendet den Client.");
    } else {
        control_panel::Section("Verbindungsdetails");
        ImGui::TextWrapped("%s",v.diagnostic.empty()?"Noch kein Verbindungsversuch. Waehle unter Start deine Minecraft-Instanz aus.":v.diagnostic.c_str());
        if(ImGui::Button("Protokollordner oeffnen")) a.openLogs=true;
        ImGui::SameLine();
        if(ImGui::Button("Protokoll kopieren")) ImGui::SetClipboardText((v.diagnostic+"\n"+v.log).c_str());
        control_panel::Section("Protokoll"); ImGui::TextWrapped("%s",v.log.empty()?"Noch keine Eintraege.":v.log.c_str());
    }
    ImGui::Unindent(0); ImGui::EndChild();
    if(v.tab==0) {
        const bool selected=v.selected>=0&&v.selected<static_cast<int>(v.instances.size());
        const bool running=selected&&v.instances[v.selected].running;
        ImGui::BeginDisabled(v.busy||!selected);
        {
            style::StyleScope primary;
            primary.Color(ImGuiCol_Button,style::Color(0x6F79E8,.80f));
            primary.Color(ImGuiCol_ButtonHovered,style::Color(0x818CF8));
            if(ImGui::Button(v.busy?"Verbindung wird vorbereitet...":running?"Zu Minecraft wechseln":"Client starten",{-1,40})) a.start=true;
        }
        ImGui::EndDisabled();
    }
    ImGui::Dummy({0,5});
    ImGui::TextDisabled("mcov | Windows x64");
    ImGui::SameLine(std::max(0.f,width-140));
    if(ImGui::Button("Schliessen",{140,27})) a.close=true;
    ImGui::End(); return a;
}
} // namespace mcov::launcher

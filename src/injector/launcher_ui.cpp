#include "launcher_ui.hpp"
#include "../common/classic_style.hpp"
#include "../overlay/control_panel.hpp"
#include <algorithm>

namespace mcov::launcher {
Actions Render(View& v,ImVec2 size) {
    style::ClassicStyle scoped; Actions a;
    const ImVec2 top{12,12},bottom{size.x-12,size.y-12};
    auto* draw=ImGui::GetBackgroundDrawList();
    draw->AddRectFilled(top,bottom,ImGui::GetColorU32(ImGuiCol_WindowBg));
    draw->AddRect(top,bottom,ImGui::GetColorU32(ImGuiCol_Border));
    draw->AddLine({13,13},{size.x-13,13},ImGui::GetColorU32(ImGuiCol_CheckMark),2);
    draw->AddText({21,26},ImGui::GetColorU32(ImGuiCol_Text),"mcov - Client");
    const ImVec2 closeMin{size.x-44,22},closeMax{size.x-22,44},minMin{size.x-70,22},minMax{size.x-48,44};
    const auto hit=[&](ImVec2 low,ImVec2 high) {
        const auto m=ImGui::GetIO().MousePos;
        const bool hover=m.x>=low.x&&m.y>=low.y&&m.x<high.x&&m.y<high.y;
        draw->AddRectFilled(low,high,ImGui::GetColorU32(hover?ImGuiCol_ButtonHovered:ImGuiCol_Button));
        draw->AddRect(low,high,ImGui::GetColorU32(ImGuiCol_Border));
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
        if(i) ImGui::SameLine(0,0);
        style::StyleScope selected;
        if(v.tab==i) selected.Color(ImGuiCol_Button,ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
        if(ImGui::Button(tabs[i],{width*.5f,28})) v.tab=i;
    }
    ImGui::Dummy({0,6});
    const float remaining=std::max(60.f,ImGui::GetContentRegionAvail().y-29);
    ImGui::BeginChild("launcher_content",{0,remaining},ImGuiChildFlags_Borders);
    ImGui::Indent(8); ImGui::SetCursorPosY(ImGui::GetCursorPosY()+8);
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
        const bool selected=v.selected>=0&&v.selected<static_cast<int>(v.instances.size());
        const bool running=selected&&v.instances[v.selected].running;
        ImGui::Spacing();
        ImGui::BeginDisabled(v.busy||!selected);
        {
            style::StyleScope primary;
            primary.Color(ImGuiCol_Button,style::Color(0x69417D));
            if(ImGui::Button(v.busy?"Verbindung wird vorbereitet...":running?"Zu Minecraft wechseln":"Client starten",{-8,32})) a.start=true;
        }
        ImGui::EndDisabled();
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
    ImGui::Unindent(8); ImGui::EndChild();
    ImGui::Dummy({0,5});
    ImGui::TextDisabled("mcov | Windows x64");
    ImGui::SameLine(std::max(0.f,width-140));
    if(ImGui::Button("Schliessen",{140,19})) a.close=true;
    ImGui::End(); return a;
}
} // namespace mcov::launcher

#include "launcher_ui.hpp"
#include "../common/workspace_style.hpp"
#include "../overlay/control_panel.hpp"
#include <algorithm>
#include <utility>

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
    listStyle.Color(ImGuiCol_ChildBg,ImVec4(0,0,0,0));
    listStyle.Var(ImGuiStyleVar_WindowPadding,ImVec2(1,1));
    ImGui::BeginChild("instances",{0,height},ImGuiChildFlags_AlwaysUseWindowPadding);
    if(v.instances.empty()) {
        ImGui::Dummy({0,12});
        ImGui::PushFont(style::MenuFont(true)); ImGui::TextUnformatted("No Minecraft window found"); ImGui::PopFont();
        ImGui::Spacing();
        ImGui::TextWrapped("Open Minecraft to the main menu, then select Refresh. Your game will appear here.");
    }
    ImGui::BeginDisabled(v.busy);
    for(int i=0;i<static_cast<int>(v.instances.size());++i) {
        const auto& p=v.instances[i]; ImGui::PushID(static_cast<int>(p.pid));
        const auto row=ImGui::GetCursorScreenPos();
        const float width=ImGui::GetContentRegionAvail().x;
        auto& draw=*ImGui::GetWindowDrawList();
        const bool selected=v.selected==i;
        draw.AddRectFilled(row,{row.x+width,row.y+56},ImGui::GetColorU32(selected?
            style::Color(style::workspace::AccentRGB,.065f):style::Color(0xFFFFFF,.025f)),7);
        {
            style::StyleScope button;
            button.Color(ImGuiCol_Button,ImVec4(0,0,0,0));
            button.Var(ImGuiStyleVar_FrameRounding,7);
            if(ImGui::Button("##instance",{width,56})) v.selected=i;
        }
        draw.AddRect(row,{row.x+width,row.y+56},ImGui::GetColorU32(selected?
            style::Color(style::workspace::AccentRGB,.25f):style::Color(0xFFFFFF,.055f)),7);
        const auto ink=ImGui::GetColorU32(ImGuiCol_Text);
        const auto muted=ImGui::GetColorU32(ImGuiCol_TextDisabled);
        const auto accent=ImGui::GetColorU32(ImGuiCol_CheckMark);
        draw.PushClipRect({row.x+1,row.y},{row.x+width-12,row.y+56},true);
        draw.AddCircle({row.x+16,row.y+19},4.5f,v.selected==i?accent:muted,16,1.25f);
        if(v.selected==i) draw.AddCircleFilled({row.x+16,row.y+19},2,accent,12);
        draw.AddText(style::MenuFont(true),14,{row.x+32,row.y+12},ink,p.title.c_str());
        const std::string detail="PID "+std::to_string(p.pid)+" | "+p.java+
            (p.running?" | Client running":p.connected?" | Connected":"");
        draw.AddText({row.x+32,row.y+34},muted,detail.c_str());
        draw.PopClipRect();
        if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s\n%s",p.title.c_str(),detail.c_str());
        ImGui::PopID();
    }
    ImGui::EndDisabled(); ImGui::EndChild();
}
void HardwareId(View& v,float width) {
    ImGui::AlignTextToFramePadding(); ImGui::TextDisabled("HWID");
    ImGui::SameLine();
    const float copyWidth=80;
    ImGui::SetNextItemWidth(std::max(1.f,width-ImGui::CalcTextSize("HWID").x-copyWidth-2*ImGui::GetStyle().ItemSpacing.x));
    ImGui::InputTextWithHint("##hardware_id","Unavailable",v.hardwareId.data(),v.hardwareId.size()+1,
        ImGuiInputTextFlags_ReadOnly|ImGuiInputTextFlags_AutoSelectAll);
    if(ImGui::IsItemHovered()) {
        if(v.hardwareId.empty()) ImGui::SetTooltip("Could not read the Windows account ID. Restart Keim to try again.");
        else ImGui::SetTooltip("%s\nWindows account ID used for license verification.",v.hardwareId.c_str());
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(v.hardwareId.empty());
    if(ImGui::Button("Copy HWID",{copyWidth,22})) ImGui::SetClipboardText(v.hardwareId.c_str());
    ImGui::EndDisabled();
}
}

Actions Render(View& v,ImVec2 size) {
    style::WorkspaceStyle scoped; Actions a;
    const bool compact=size.x<620||size.y<460;
    const float sidebar=compact?112.f:166.f;
    v.tab=std::clamp(v.tab,0,1);
    auto& draw=*ImGui::GetBackgroundDrawList();
    const auto ink=ImGui::GetColorU32(ImGuiCol_Text);
    const auto muted=ImGui::GetColorU32(ImGuiCol_TextDisabled);
    const auto accent=ImGui::GetColorU32(ImGuiCol_CheckMark);
    draw.AddRectFilled({12,12},{size.x-12,size.y-12},ImGui::GetColorU32(ImGuiCol_WindowBg),12);
    draw.AddRectFilled({12,12},{12+sidebar,size.y-12},ImGui::GetColorU32(style::Color(style::workspace::SidebarRGB)),12,ImDrawFlags_RoundCornersLeft);
    draw.AddRect({12,12},{size.x-13,size.y-13},ImGui::GetColorU32(ImGuiCol_Border),12);
    draw.AddLine({12+sidebar,13},{12+sidebar,size.y-13},ImGui::GetColorU32(ImGuiCol_Separator));
    style::DrawBrandLogo(draw,{compact?20.f:28.f,compact?21.f:31.f},compact?28.f:30.f);
    draw.AddText(compact?style::MenuFont(true):style::MenuHeadingFont(),compact?14.f:22.f,
        {compact?56.f:64.f,compact?28.f:34.f},ink,"keim");
    if(!compact) {
        draw.AddText({32,72},muted,"Minecraft client");
        draw.AddText(style::MenuFont(),11,{sidebar+32,48},muted,"GET STARTED");
    }
    const char* title=v.tab==1?"Diagnostics":v.licensed?"Start client":"Sign in";
    draw.PushClipRect({sidebar+32,20},{size.x-(compact?80:28),120},true);
    draw.AddText(compact?style::MenuHeadingFont():style::MenuDisplayFont(),compact?22.f:28.f,
        {sidebar+32,compact?25.f:70.f},ink,title);
    draw.PopClipRect();
    if(!compact) {
        const char* description=v.tab==1?"Connection details and recent activity.":
            v.licensed?"Choose your Minecraft session.":"Verify your license to open the client.";
        draw.AddText({sidebar+32,110},muted,description);
    }

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
    ImGui::Begin("##keim_launcher",nullptr,flags);
    {
        style::StyleScope padding; padding.Var(ImGuiStyleVar_WindowPadding,ImVec2(10,14));
        ImGui::BeginChild("launcher_navigation",{sidebar,0},ImGuiChildFlags_AlwaysUseWindowPadding,ImGuiWindowFlags_NoBackground);
        if(!compact) {
            ImGui::SetCursorPosY(50); ImGui::SetCursorPosX(20);
            ImGui::TextDisabled("WORKSPACE"); ImGui::Dummy({0,6});
        }
        const char* tabs[]={"Start","Logs"};
        for(int i=0;i<2;++i) {
            const bool active=v.tab==i; const auto p=ImGui::GetCursorScreenPos();
            style::StyleScope selected;
            selected.Var(ImGuiStyleVar_ButtonTextAlign,ImVec2(0,.5f));
            selected.Var(ImGuiStyleVar_FramePadding,ImVec2(32,4));
            selected.Color(ImGuiCol_Button,active?style::Color(style::workspace::AccentRGB,.09f):ImVec4(0,0,0,0));
            selected.Color(ImGuiCol_Text,active?style::Color(0xD6EBC6):style::Color(0xADB5AF));
            const float height=compact?29.f:36.f;
            if(ImGui::Button(tabs[i],{-1,height})) { v.tab=i; ImGui::SetWindowFocus(); }
            if(active) ImGui::GetWindowDrawList()->AddRectFilled({p.x,p.y+9},{p.x+2,p.y+height-9},accent,1);
            NavigationIcon(*ImGui::GetWindowDrawList(),{p.x+15,p.y+height*.5f},i,active?accent:muted);
        }
        ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY()+16,ImGui::GetWindowHeight()-44));
        ImGui::TextDisabled("Windows x64");
        ImGui::EndChild();
    }
    ImGui::SameLine(0,20);
    ImGui::BeginGroup();
    if(!compact) ImGui::Dummy({0,82});
    const float width=std::max(1.f,ImGui::GetContentRegionAvail().x-16);
    const float footer=v.tab==0?104.f:64.f;
    const ImVec2 contentSize{width,std::max(1.f,ImGui::GetContentRegionAvail().y-footer)};
    {
        style::StyleScope padding; padding.Var(ImGuiStyleVar_WindowPadding,ImVec2(0,14));
        ImGui::BeginChild("launcher_content",contentSize,ImGuiChildFlags_AlwaysUseWindowPadding,ImGuiWindowFlags_NoBackground);
        if(v.tab==0&&!v.licensed) {
            ImGui::PushFont(style::MenuFont(true)); ImGui::TextUnformatted("License key"); ImGui::PopFont();
            ImGui::Dummy({0,4});
            ImGui::BeginDisabled(v.authBusy);
            ImGui::SetNextItemWidth(-1);
            const auto inputFlags=ImGuiInputTextFlags_EnterReturnsTrue|(v.showLicense?0:ImGuiInputTextFlags_Password);
            {
                style::StyleScope input; input.Var(ImGuiStyleVar_FramePadding,ImVec2(12,9));
                if(ImGui::InputTextWithHint("##license_key","Enter license key",v.licenseKey.data(),v.licenseKey.size(),inputFlags)&&v.licenseKey[0]) a.verifyLicense=true;
            }
            ImGui::Checkbox("Show key",&v.showLicense);
            ImGui::EndDisabled();
            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
            {
                style::StyleScope status;
                if(v.authError) status.Color(ImGuiCol_Text,ImGui::ColorConvertU32ToFloat4(style::compact::ErrorText));
                ImGui::TextWrapped("%s",v.authBusy?"Checking your license with KeyAuth...":v.authMessage.c_str());
            }
            ImGui::Spacing(); ImGui::TextWrapped("An internet connection is required. Your key is not saved on this device.");
        } else if(v.tab==0) {
            ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted("Select Minecraft");
            const float refreshWidth=ImGui::CalcTextSize("Refresh").x+14;
            if(ImGui::GetItemRectMax().x+refreshWidth+12<=ImGui::GetCursorScreenPos().x+width) ImGui::SameLine(0,12);
            ImGui::BeginDisabled(v.busy);
            if(ImGui::SmallButton("Refresh")) a.refresh=true;
            ImGui::EndDisabled();
            ImGui::Dummy({0,3});
            InstanceList(v,std::clamp(size.y*.28f,96.f,180.f));
            control_panel::Section(v.busy?"Connection":"Status");
            if(v.busy) {
                ImGui::TextWrapped("%s",v.phase.c_str());
                ImGui::TextDisabled("You can still use this window.");
            } else {
                style::StyleScope status;
                if(v.error) status.Color(ImGuiCol_Text,ImGui::ColorConvertU32ToFloat4(style::compact::ErrorText));
                ImGui::TextWrapped("%s",v.status.c_str());
                if(v.needsJdk&&ImGui::Button("Select JDK folder")) a.chooseJdk=true;
                if(v.error&&ImGui::SmallButton("Show details")) v.tab=1;
            }
            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
            ImGui::TextWrapped("After starting, switch to Minecraft.");
            ImGui::Spacing();
            for(const auto& shortcut:std::array<std::pair<const char*,const char*>,3>{{{"INSERT","Open menu"},{"F6","HUD editor"},{"END","Quit client"}}}) {
                const auto p=ImGui::GetCursorScreenPos();
                auto& contentDraw=*ImGui::GetWindowDrawList();
                contentDraw.AddRectFilled(p,{p.x+58,p.y+22},ImGui::GetColorU32(style::Color(0xFFFFFF,.045f)),4);
                contentDraw.AddText({p.x+8,p.y+4},muted,shortcut.first);
                contentDraw.AddText({p.x+72,p.y+4},muted,shortcut.second);
                ImGui::Dummy({0,22});
            }
        } else {
            control_panel::Section("Connection details");
            ImGui::TextWrapped("%s",v.diagnostic.empty()?"No connection attempt yet. Select your Minecraft instance under Start.":v.diagnostic.c_str());
            if(ImGui::Button("Open log folder")) a.openLogs=true;
            const float copyWidth=ImGui::CalcTextSize("Copy log").x+14;
            if(ImGui::GetItemRectMax().x+copyWidth+9<=ImGui::GetCursorScreenPos().x+width) ImGui::SameLine();
            if(ImGui::Button("Copy log")) ImGui::SetClipboardText((v.diagnostic+"\n"+v.log).c_str());
            control_panel::Section("Log");
            ImGui::TextWrapped("%s",v.log.empty()?"No log entries yet.":v.log.c_str());
        }
        ImGui::EndChild();
    }
    {
        const auto p=ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddLine({p.x,p.y-5},{p.x+width,p.y-5},ImGui::GetColorU32(ImGuiCol_Separator));
    }
    if(v.tab==0) {
        const bool selected=v.selected>=0&&v.selected<static_cast<int>(v.instances.size());
        const bool running=selected&&v.instances[v.selected].running;
        const bool disabled=v.licensed?(v.busy||!selected):(v.authBusy||!v.licenseKey[0]);
        ImGui::BeginDisabled(disabled);
        {
            style::StyleScope primary;
            primary.Color(ImGuiCol_Button,style::Color(disabled?0x3E4940:style::workspace::AccentRGB));
            primary.Color(ImGuiCol_ButtonHovered,style::Color(style::workspace::AccentHoverRGB));
            primary.Color(ImGuiCol_ButtonActive,style::Color(style::workspace::AccentPressedRGB));
            primary.Color(ImGuiCol_Text,style::Color(disabled?style::workspace::TextRGB:0x182312));
            const char* label=!v.licensed?(v.authBusy?"Verifying license...":"Verify license"):
                v.busy?"Preparing connection...":running?"Switch to Minecraft":"Start client";
            if(ImGui::Button(label,{width,40})) { if(v.licensed) a.start=true; else a.verifyLicense=true; }
        }
        ImGui::EndDisabled();
    }
    HardwareId(v,width);
    ImGui::AlignTextToFramePadding(); ImGui::TextDisabled("Keim");
    ImGui::SameLine(0,std::max(9.f,width-ImGui::CalcTextSize("Keim").x-80));
    if(ImGui::Button("Close",{80,22})) a.close=true;
    ImGui::EndGroup(); ImGui::End(); return a;
}
} // namespace mcov::launcher

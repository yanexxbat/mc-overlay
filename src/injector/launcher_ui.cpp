#include "launcher_ui.hpp"
#include "../common/workspace_style.hpp"
#include "../overlay/control_panel.hpp"
#include <algorithm>
#include <array>
#include <utility>
#include <cmath>

namespace mcov::launcher {
namespace {
bool MotionButton(View& v,const char* label,ImVec2 size) {
    const auto id=ImGui::GetID(label);
    auto& m=v.motion;
    auto base=ImGui::GetStyleColorVec4(ImGuiCol_Button);
    const auto over=ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered);
    const auto pressed=ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
    const float hover=m.widgets[id].value,press=m.widgets[id^0x731ac52u].value;
    const auto fill=v.animations?anim::mix(anim::mix(base,over,hover),pressed,press):base;
    style::StyleScope colors;
    colors.Color(ImGuiCol_Button,fill); colors.Color(ImGuiCol_ButtonHovered,v.animations?fill:over);
    colors.Color(ImGuiCol_ButtonActive,v.animations?fill:pressed);
    const bool clicked=ImGui::Button(label,size);
    m.to(id,ImGui::IsItemHovered()?1.f:0.f,v.animations,anim::Hover);
    m.to(id^0x731ac52u,ImGui::IsItemActive()?1.f:0.f,v.animations,anim::Press);
    return clicked;
}
void CheckStroke(ImDrawList& draw,ImVec2 p,float extent,float t,ImU32 color) {
    const ImVec2 a{p.x,p.y+extent*.5f},b{p.x+extent*.35f,p.y+extent*.85f},c{p.x+extent,p.y};
    const float first=anim::unit(t*3),second=anim::unit((t-1.f/3)*1.5f);
    if(first>0) draw.AddLine(a,{a.x+(b.x-a.x)*first,a.y+(b.y-a.y)*first},color,2);
    if(second>0) draw.AddLine(b,{b.x+(c.x-b.x)*second,b.y+(c.y-b.y)*second},color,2);
}
void MotionCheckbox(View& v,const char* label,bool* value) {
    const auto id=ImGui::GetID(label);
    const float initial=*value?1.f:0.f;
    const auto p=ImGui::GetCursorScreenPos();
    { // Keep native hit testing, focus, keyboard operation and accessibility.
        style::StyleScope hide; hide.Color(ImGuiCol_CheckMark,ImVec4(0,0,0,0));
        ImGui::Checkbox(label,value);
    }
    const float t=v.motion.to(id,*value?1.f:0.f,v.animations,anim::Focus,initial);
    const float extent=ImGui::GetFrameHeight();
    CheckStroke(*ImGui::GetWindowDrawList(),{p.x+extent*.25f,p.y+extent*.24f},extent*.5f,t,
        ImGui::GetColorU32(style::Color(anim::Accent)));
}
bool IntroActive(const View& v) { return v.introReady&&!v.motion.introDone&&v.animations; }
void Intro(View& v,ImVec2 size,bool) {
    if(!IntroActive(v)) return;
    const float age=v.motion.introAge;
    const float enter=anim::easeOutCubic(age/anim::IntroReveal);
    const float leave=anim::easeInOutCubic((age-(anim::Intro-anim::IntroExit))/anim::IntroExit);
    const float opacity=enter*(1-leave);
    auto& draw=*ImGui::GetForegroundDrawList();
    draw.AddRectFilled({12,12},{size.x-12,size.y-12},
        ImGui::GetColorU32(style::Color(style::workspace::SidebarRGB,1-leave)),12);
    const float scale=.92f+.08f*enter;
    const float logo=80*scale;
    const ImVec2 center{size.x*.5f,size.y*.5f-20-anim::Slide*leave};
    // Bounded translucent rings provide a quiet glow without blur textures.
    for(int i=5;i>0;--i) draw.AddCircleFilled(center,46.f+i*5,
        ImGui::GetColorU32(style::Color(anim::Accent,.008f*opacity)),48);
    style::DrawBrandLogo(draw,{center.x-logo*.5f,center.y-logo*.5f},logo,
        ImGui::GetColorU32(ImVec4(1,1,1,opacity)));
    auto* font=style::MenuHeadingFont();
    const float fontSize=28*scale;
    const float width=font->CalcTextSizeA(fontSize,1000,0,"keim").x;
    draw.AddText(font,fontSize,{center.x-width*.5f,center.y+53},
        ImGui::GetColorU32(ImVec4(1,1,1,opacity)),"keim");
}
// Match retained suffix/prefix lines when LogTail drops old entries. New lines
// get one birth time; scrolling and duplicate text never restart their fade.
void UpdateLogs(View& v) {
    auto& m=v.motion;
    if(m.logSnapshot==v.log) return;
    m.logSnapshot=v.log;
    std::vector<std::string> lines;
    size_t begin=0;
    while(begin<v.log.size()) {
        auto end=v.log.find('\n',begin);
        if(end==std::string::npos) end=v.log.size();
        lines.push_back(v.log.substr(begin,end-begin)); begin=end+1;
    }
    if(lines==m.lines) return;
    size_t overlap=std::min(lines.size(),m.lines.size());
    while(overlap&&!std::equal(m.lines.end()-overlap,m.lines.end(),lines.begin())) --overlap;
    std::vector<float> births(m.lineBirth.end()-overlap,m.lineBirth.end());
    births.resize(lines.size(),m.clock);
    m.lines=std::move(lines); m.lineBirth=std::move(births);
}
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
            style::Color(anim::Accent,.065f):style::Color(0xFFFFFF,.025f)),7);
        {
            style::StyleScope button;
            button.Color(ImGuiCol_Button,ImVec4(0,0,0,0));
            button.Var(ImGuiStyleVar_FrameRounding,7);
            if(MotionButton(v,"##instance",{width,56})) v.selected=i;
        }
        draw.AddRect(row,{row.x+width,row.y+56},ImGui::GetColorU32(selected?
            style::Color(anim::Accent,.25f):style::Color(0xFFFFFF,.055f)),7);
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
}

Actions Render(View& v,ImVec2 size) {
    style::WorkspaceStyle scoped; Actions a;
    style::StyleScope palette;
    palette.Color(ImGuiCol_CheckMark,style::Color(anim::Accent));
    auto& m=v.motion;
    m.dt=std::max(0.f,ImGui::GetIO().DeltaTime); m.clock+=m.dt;
    m.pageAge+=m.dt;
    if(!v.animations||!anim::IntroEnabled) m.introDone=true;
    bool skip=false;
    if(IntroActive(v)) {
        m.introAge+=m.dt;
        for(int key=ImGuiKey_NamedKey_BEGIN;key<ImGuiKey_NamedKey_END;++key)
            skip|=ImGui::IsKeyPressed(static_cast<ImGuiKey>(key),false);
        for(int button=0;button<5;++button) skip|=ImGui::IsMouseClicked(button);
        skip|=!ImGui::GetIO().InputQueueCharacters.empty();
        if(skip||m.introAge>=anim::Intro) m.introDone=true;
    }
    const bool blocked=IntroActive(v)||skip;
    UpdateLogs(v);
    const bool compact=size.x<620||size.y<560;
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
    const char* title=v.tab==1?"Diagnostics":"Start client";
    draw.PushClipRect({sidebar+32,20},{size.x-(compact?80:28),120},true);
    draw.AddText(compact?style::MenuHeadingFont():style::MenuDisplayFont(),compact?22.f:28.f,
        {sidebar+32,compact?25.f:70.f},ink,title);
    draw.PopClipRect();
    if(!compact) {
        const char* description=v.tab==1?"Connection details and recent activity.":
            "Choose your Minecraft session.";
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
    a.close=hit(closeMin,closeMax)&&!blocked; a.minimize=hit(minMin,minMax)&&!blocked;
    draw.AddLine({size.x-37,29},{size.x-29,37},ink,1.25f); draw.AddLine({size.x-29,29},{size.x-37,37},ink,1.25f);
    draw.AddLine({size.x-64,36},{size.x-54,36},ink,1.25f);

    ImGui::SetNextWindowPos({12,58}); ImGui::SetNextWindowSize({size.x-24,size.y-70});
    style::StyleScope shell; shell.Var(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    constexpr auto flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("##keim_launcher",nullptr,flags);
    ImGui::BeginDisabled(blocked);
    {
        style::StyleScope padding; padding.Var(ImGuiStyleVar_WindowPadding,ImVec2(10,14));
        ImGui::BeginChild("launcher_navigation",{sidebar,0},ImGuiChildFlags_AlwaysUseWindowPadding,ImGuiWindowFlags_NoBackground);
        if(!compact) {
            ImGui::SetCursorPosY(50); ImGui::SetCursorPosX(20);
            ImGui::TextDisabled("WORKSPACE"); ImGui::Dummy({0,6});
        }
        const char* tabs[]={"Start","Logs"};
        const auto nav=ImGui::GetCursorScreenPos();
        const float rowHeight=compact?29.f:36.f;
        const float target=nav.y+v.tab*(rowHeight+ImGui::GetStyle().ItemSpacing.y);
        const float marker=m.to(ImGui::GetID("##nav_marker"),target,v.animations,anim::Transition,target);
        auto& navDraw=*ImGui::GetWindowDrawList();
        navDraw.AddRectFilled({nav.x,marker},{nav.x+ImGui::GetContentRegionAvail().x,marker+rowHeight},
            ImGui::GetColorU32(style::Color(anim::Accent,.09f)),5);
        navDraw.AddRectFilled({nav.x,marker+9},{nav.x+2,marker+rowHeight-9},accent,1);
        for(int i=0;i<2;++i) {
            const bool active=v.tab==i; const auto p=ImGui::GetCursorScreenPos();
            style::StyleScope selected;
            selected.Var(ImGuiStyleVar_ButtonTextAlign,ImVec2(0,.5f));
            selected.Var(ImGuiStyleVar_FramePadding,ImVec2(32,4));
            selected.Color(ImGuiCol_Button,ImVec4(0,0,0,0));
            selected.Color(ImGuiCol_Text,active?style::Color(0xD6EBC6):style::Color(0xADB5AF));
            const float height=compact?29.f:36.f;
            if(MotionButton(v,tabs[i],{-1,height})) { v.tab=i; ImGui::SetWindowFocus(); }

            NavigationIcon(*ImGui::GetWindowDrawList(),{p.x+15,p.y+height*.5f},i,active?accent:muted);
        }
        ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY()+16,ImGui::GetWindowHeight()-70));
        MotionCheckbox(v,"Animations",&v.animations);
        ImGui::TextDisabled("Windows x64");
        ImGui::EndChild();
    }
    ImGui::SameLine(0,20);
    ImGui::BeginGroup();
    const int page=v.tab;
    if(m.page!=page) { if(m.page!=-1) m.pageAge=0; m.page=page; }
    if(!compact) ImGui::Dummy({0,82});
    const float width=std::max(1.f,ImGui::GetContentRegionAvail().x-16);
    const float footer=v.tab==0?76.f:36.f;
    const ImVec2 contentSize{width,std::max(1.f,ImGui::GetContentRegionAvail().y-footer)};
    {
        style::StyleScope padding; padding.Var(ImGuiStyleVar_WindowPadding,ImVec2(0,14));
        const float reveal=v.animations?anim::easeOutCubic(m.pageAge/anim::Transition):1.f;
        padding.Var(ImGuiStyleVar_Alpha,ImGui::GetStyle().Alpha*reveal);
        ImGui::BeginChild("launcher_content",contentSize,ImGuiChildFlags_AlwaysUseWindowPadding,ImGuiWindowFlags_NoBackground);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY()+anim::Slide*(1-reveal));
        if(v.tab==0) {
            ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted("Select Minecraft");
            const float refreshWidth=ImGui::CalcTextSize("Refresh").x+14;
            if(ImGui::GetItemRectMax().x+refreshWidth+12<=ImGui::GetCursorScreenPos().x+width) ImGui::SameLine(0,12);
            ImGui::BeginDisabled(v.busy);
            if(ImGui::SmallButton("Refresh")) a.refresh=true;
            ImGui::EndDisabled();
            ImGui::Dummy({0,3});
            const float listHeight=v.instances.empty()?96.f:std::min(
                float(v.instances.size())*62.f+2,std::clamp(size.y*.25f,96.f,180.f));
            InstanceList(v,listHeight);
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
            if(!v.busy&&!v.error&&!v.needsJdk) {
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
            }
        } else {
            control_panel::Section("Connection details");
            ImGui::TextWrapped("%s",v.diagnostic.empty()?"No connection attempt yet. Select your Minecraft instance under Start.":v.diagnostic.c_str());
            if(ImGui::Button("Open log folder")) a.openLogs=true;
            const float copyWidth=ImGui::CalcTextSize("Copy log").x+14;
            if(ImGui::GetItemRectMax().x+copyWidth+9<=ImGui::GetCursorScreenPos().x+width) ImGui::SameLine();
            if(ImGui::Button("Copy log")) ImGui::SetClipboardText((v.diagnostic+"\n"+v.log).c_str());
            control_panel::Section("Log");
            if(m.lines.empty()) ImGui::TextWrapped("No log entries yet.");
            for(size_t i=0;i<m.lines.size();++i) {
                style::StyleScope line;
                const float fade=v.animations?anim::easeOutCubic((m.clock-m.lineBirth[i])/anim::Transition):1.f;
                line.Var(ImGuiStyleVar_Alpha,ImGui::GetStyle().Alpha*fade);
                ImGui::PushTextWrapPos(0);
                ImGui::TextUnformatted(m.lines[i].empty()?" ":m.lines[i].c_str());
                ImGui::PopTextWrapPos();
            }
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
        const bool disabled=v.busy||!selected;
        ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha,1.f);
        ImGui::BeginDisabled(disabled);
        {
            style::StyleScope primary;
            const float enabled=m.to(ImGui::GetID("##primary_enabled"),disabled?0.f:1.f,v.animations);
            primary.Color(ImGuiCol_Button,anim::mix(style::Color(anim::Disabled),style::Color(anim::Accent),enabled));
            primary.Color(ImGuiCol_ButtonHovered,style::Color(anim::AccentHover));
            primary.Color(ImGuiCol_ButtonActive,style::Color(anim::AccentPressed));
            primary.Color(ImGuiCol_Text,style::Color(disabled?style::workspace::TextRGB:0x182312));
            const char* label=v.busy?"Preparing connection...###primary":running?"Switch to Minecraft###primary":"Start client###primary";
            if(MotionButton(v,label,{width,40})) a.start=true;
            if(v.busy) {
                const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();
                const float span=(hi.x-lo.x)*.22f;
                const float phase=v.animations?float(std::fmod(m.clock*.8f,1.f)):.5f;
                const float x=lo.x-span+(hi.x-lo.x+span)*phase;
                auto& activity=*ImGui::GetWindowDrawList();
                activity.PushClipRect({lo.x+2,hi.y-4},{hi.x-2,hi.y-1},true);
                activity.AddRectFilled({x,hi.y-3},{x+span,hi.y-1},accent,1);
                activity.PopClipRect();
            }
        }
        ImGui::EndDisabled(); ImGui::PopStyleVar();
    }
    ImGui::AlignTextToFramePadding(); ImGui::TextDisabled("Keim");
    ImGui::SameLine(0,std::max(9.f,width-ImGui::CalcTextSize("Keim").x-80));
    if(MotionButton(v,"Close",{80,22})) a.close=true;
    ImGui::EndGroup(); ImGui::EndDisabled(); ImGui::End();
    Intro(v,size,compact);
    return a;
}
} // namespace mcov::launcher

#include "control_panel.hpp"
#include "../common/flat_style.hpp"
#include "../common/liquid_style.hpp"
#include "../common/liquid_glass.hpp"
#include "imgui_internal.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <array>

namespace mcov::control_panel {
namespace {
float gScale=1;
thread_local int gSection=-1;
using MenuStyle=style::LiquidStyle;
void RowLabel(const char* label) {
    ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(label); ImGui::SetNextItemWidth(-1);
}
template<class T> bool Slider(const char* label,T* value,T low,T high,const char* format,ImGuiDataType type) {
    ImGui::PushID(label); RowLabel(label);
    const auto p=ImGui::GetCursorScreenPos(); const float width=ImGui::GetContentRegionAvail().x;
    bool changed;
    {
        style::StyleScope invisible;
        const auto id=ImGui::GetID("##value");
        const auto& ctx=*ImGui::GetCurrentContext();
        const bool editing=ImGui::TempInputIsActive(id)||
            (ImGui::GetIO().KeyCtrl&&ImGui::IsMouseClicked(0)&&ImGui::IsMouseHoveringRect(p,{p.x+width,p.y+ImGui::GetFontSize()+10}))||
            (ctx.NavActivateId==id&&(ctx.NavActivateFlags&ImGuiActivateFlags_PreferInput));
        if(!editing) for(auto c:{ImGuiCol_FrameBg,ImGuiCol_FrameBgHovered,ImGuiCol_FrameBgActive,ImGuiCol_SliderGrab,ImGuiCol_SliderGrabActive})
            invisible.Color(c,ImVec4(0,0,0,0));
        invisible.Var(ImGuiStyleVar_FramePadding,ImVec2(0,5)); invisible.Var(ImGuiStyleVar_FrameBorderSize,0.f);
        // Leeres Anzeigeformat verbirgt nur den Slider-Wert. TempInputScalar nutzt
        // dann das Zahlenformat des Datentyps und behaelt sichtbaren Text/Caret.
        changed=ImGui::SliderScalar("##value",type,value,&low,&high,"",ImGuiSliderFlags_AlwaysClamp|ImGuiSliderFlags_NoRoundToFormat);
    }
    if(!ImGui::TempInputIsActive(ImGui::GetID("##value"))) {
        const float height=ImGui::GetItemRectSize().y; const float fraction=std::clamp(float(*value-low)/float(high-low),0.f,1.f);
        auto& draw=*ImGui::GetWindowDrawList();
        const float rail=p.y+height*.5f,grab=p.x+6+(width-12)*fraction;
        draw.AddRectFilled({p.x+6,rail-2},{p.x+width-6,rail+2},IM_COL32(200,213,255,34),2);
        draw.AddRectFilled({p.x+6,rail-2},{grab,rail+2},ImGui::GetColorU32(ImGuiCol_CheckMark),2);
        if(ImGui::IsItemHovered()||ImGui::IsItemActive()) draw.AddCircleFilled({grab,rail},10,IM_COL32(178,185,255,23),24);
        draw.AddCircleFilled({grab,rail},5.5f,IM_COL32(237,240,255,255),24);
        char text[64]; if(type==ImGuiDataType_S32) std::snprintf(text,sizeof(text),format,int(*value));
        else std::snprintf(text,sizeof(text),format,double(*value));
        const auto extent=ImGui::CalcTextSize(text);
        const ImVec2 tp{p.x+width-extent.x,p.y-ImGui::GetTextLineHeightWithSpacing()};
        draw.AddText(tp,ImGui::GetColorU32(ImGuiCol_CheckMark),text);
        if(ImGui::IsItemFocused()) draw.AddRect(p,{p.x+width,p.y+height},ImGui::GetColorU32(ImGuiCol_NavCursor),6);
    }
    ImGui::PopID(); return changed;
}
}
float Scale() { return gScale; }
SectionScope::SectionScope(int section):previous_(gSection) { gSection=section; }
SectionScope::~SectionScope() { gSection=previous_; }
const char* ThemeName(int) { return "Liquid glass"; }
bool MotionAllowed(const State& state) { return !state.reduceMotion; }
void Section(const char* label) {
    ImGui::Dummy({0,8});
    if(label&&*label) { ImGui::PushFont(style::MenuFont(true)); ImGui::TextUnformatted(label); ImGui::PopFont(); }
    ImGui::Separator(); ImGui::Dummy({0,3});
}
int SubTabs(const char* id,const char* const* labels,int count,int selected,bool selectNext) {
    if(gSection>=0) return std::clamp(gSection,0,count-1);
    selected=std::clamp(selected,0,count-1);
    const int previous=selected;
    style::StyleScope colors;
    colors.Var(ImGuiStyleVar_FramePadding,ImVec2(12,7));
    if(ImGui::BeginTabBar(id,ImGuiTabBarFlags_FittingPolicyScroll)) {
        const int requested=selected;
        for(int i=0;i<count;++i) {
            const auto flags=selectNext&&i==requested ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
            if(ImGui::BeginTabItem(labels[i],nullptr,flags)) { selected=i; ImGui::EndTabItem(); }
        }
        ImGui::EndTabBar();
    }
    if(selected!=previous) ImGui::SetScrollY(0);
    ImGui::Dummy({0,5*Scale()});
    return selected;
}
Actions Render(State& state,const Status& status,void* owner,Content content,Catalog catalog,Activate activate) {
    MenuStyle scoped; Actions actions;
    bool focusDetails=false;
    const auto previousPage=state.renderedPage;
    const auto* viewport=ImGui::GetMainViewport(); const auto work=viewport->WorkSize;
    const ImVec2 size{std::floor(std::min(900.f,std::max(160.f,work.x-24))),std::floor(std::min(680.f,std::max(160.f,work.y-24)))};
    if(!state.positioned) { state.position={(work.x-size.x)*.5f,(work.y-size.y)*.5f}; state.positioned=true; }
    if(state.dragging) {
        const auto m=ImGui::GetIO().MousePos;
        state.position={m.x-viewport->WorkPos.x-state.dragOffset.x,m.y-viewport->WorkPos.y-state.dragOffset.y};
        if(!ImGui::IsMouseDown(0)) state.dragging=false;
    }
    state.position={std::clamp(state.position.x,0.f,std::max(0.f,work.x-size.x)),std::clamp(state.position.y,0.f,std::max(0.f,work.y-size.y))};
    const ImVec2 pos{std::floor(viewport->WorkPos.x+state.position.x),std::floor(viewport->WorkPos.y+state.position.y)};
    ImGui::SetNextWindowPos(pos,ImGuiCond_Always); ImGui::SetNextWindowSize(size,ImGuiCond_Always);
    const auto flags=ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoBackground;
    if(!ImGui::Begin("mcov control panel",nullptr,flags)) { ImGui::End(); return actions; }
    auto& draw=*ImGui::GetWindowDrawList();
    LiquidGlass::Panel(draw,pos,{pos.x+size.x,pos.y+size.y},22);
    ImGui::SetCursorScreenPos({pos.x+5,pos.y+4}); ImGui::InvisibleButton("##drag",{size.x-10,49});
    if(ImGui::IsItemActivated()) { state.dragging=true; const auto m=ImGui::GetIO().MousePos; state.dragOffset={m.x-pos.x,m.y-pos.y}; }
    if(ImGui::IsItemHovered()||state.dragging) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    draw.AddText(style::MenuHeadingFont(),style::MenuHeadingFont()->FontSize,{pos.x+22,pos.y+17},ImGui::GetColorU32(ImGuiCol_Text),"mcov");
    draw.AddText({pos.x+99,pos.y+26},ImGui::GetColorU32(ImGuiCol_TextDisabled),"Liquid glass");
    draw.AddText({pos.x+size.x-121,pos.y+26},ImGui::GetColorU32(ImGuiCol_TextDisabled),"INSERT  /  menu");
    const float sidebar=size.x>=620?146.f:98.f;
    ImGui::SetCursorScreenPos({pos.x+16,pos.y+74});
    ImGui::BeginChild("navigation",{sidebar,size.y-121},ImGuiChildFlags_None,ImGuiWindowFlags_NoBackground);
    constexpr const char* labels[]={"Combat","Visuals","Blocks","HUD","Stats","Settings","Misc"};
    const float tabHeight=std::clamp((size.y-121-36)/7,24.f,40.f);
    for(int i=0;i<IM_ARRAYSIZE(labels);++i) {
        style::StyleScope selected;
        selected.Var(ImGuiStyleVar_ItemSpacing,ImVec2(0,6));
        selected.Var(ImGuiStyleVar_FrameRounding,10.f); selected.Var(ImGuiStyleVar_FramePadding,ImVec2(4,7));
        selected.Color(ImGuiCol_Button,state.page==static_cast<Page>(i)?style::Color(0x818CF8,.36f):style::Color(0xC0CFFF,.04f));
        if(ImGui::Button(labels[i],{-1,tabHeight})) state.page=static_cast<Page>(i);
    }
    ImGui::EndChild(); state.renderedPage=state.page; state.pageAlpha=1; state.animateNextPage=false;
    ImGui::SetCursorScreenPos({pos.x+sidebar+32,pos.y+74});
    { style::StyleScope padding; padding.Var(ImGuiStyleVar_WindowPadding,ImVec2(13,13));
      if(state.page!=previousPage) ImGui::SetNextWindowScroll({-1,0});
      ImGui::BeginChild("modules",{size.x-sidebar-48,size.y-121},ImGuiChildFlags_AlwaysUseWindowPadding);
      ImGui::PushFont(style::MenuHeadingFont()); ImGui::TextUnformatted(labels[int(state.page)]); ImGui::PopFont();
      ImGui::TextWrapped("Left click: toggle  |  Right click: settings");
      ImGui::Dummy({0,8});
      if(catalog) {
          std::array<ModuleView,64> modules{};
          const int count=std::clamp(catalog(owner,state.page,modules),0,int(modules.size()));
          const int columns=std::clamp(int((ImGui::GetContentRegionAvail().x+10)/210),1,3);
          if(ImGui::BeginTable("module_grid",columns,ImGuiTableFlags_SizingStretchSame)) {
              for(int i=0;i<count;++i) {
                  ImGui::TableNextColumn(); ImGui::PushID(i); auto& m=modules[i];
                  const auto tile=ImGui::GetCursorScreenPos();
                  const float width=ImGui::GetContentRegionAvail().x;
                  const bool pressed=ImGui::InvisibleButton("##module",{width,96},ImGuiButtonFlags_MouseButtonLeft|ImGuiButtonFlags_MouseButtonRight|ImGuiButtonFlags_EnableNav);
                  const bool left=pressed&&!ImGui::IsMouseReleased(1)&&!ImGui::IsMouseDown(1);
                  const bool right=ImGui::IsItemClicked(1);
                  if(left) {
                      if(m.mode==TileMode::Settings) {
                          focusDetails=true;
                          state.settingsOpen=true; state.settingsPage=state.page; state.settingsSection=i;
                          std::snprintf(state.settingsLabel,sizeof(state.settingsLabel),"%s",m.name);
                      } else if(activate) { activate(owner,state.page,i); if(m.mode==TileMode::Toggle) m.enabled=!m.enabled; }
                  }
                  if(right) { focusDetails=true; state.settingsOpen=true; state.settingsPage=state.page; state.settingsSection=i;
                      std::snprintf(state.settingsLabel,sizeof(state.settingsLabel),"%s",m.name); }
                  auto* td=ImGui::GetWindowDrawList(); const bool hover=ImGui::IsItemHovered();
                  td->AddRectFilled(tile,{tile.x+width,tile.y+96},ImGui::GetColorU32(m.enabled?style::Color(0x818CF8,hover?.32f:.22f):style::Color(0xC0CFFF,hover?.14f:.065f)),12);
                  td->AddRect(tile,{tile.x+width,tile.y+96},ImGui::GetColorU32(m.enabled?style::Color(0xB2B9FF,.38f):style::Color(0xC0CFFF,.10f)),12);
                  td->PushClipRect({tile.x+12,tile.y+10},{tile.x+width-12,tile.y+86},true);
                  td->AddText(style::MenuFont(true),14,{tile.x+14,tile.y+16},ImGui::GetColorU32(ImGuiCol_Text),m.name);
                  const char* hint=m.mode==TileMode::Toggle?(m.enabled?"Enabled":"Disabled"):m.mode==TileMode::Action?"Run action":"Open settings";
                  td->AddCircleFilled({tile.x+18,tile.y+48},3,ImGui::GetColorU32(m.enabled?style::Color(0x98DEC3):style::Color(0xC5CCE2,.65f)),16);
                  td->AddText({tile.x+28,tile.y+41},ImGui::GetColorU32(ImGuiCol_TextDisabled),hint);
                  td->AddText({tile.x+14,tile.y+69},ImGui::GetColorU32(ImGuiCol_TextDisabled),m.binding[0]?m.binding:"Right click to configure");
                  td->PopClipRect();
                  if(ImGui::IsItemFocused()) td->AddRect(tile,{tile.x+width,tile.y+96},ImGui::GetColorU32(ImGuiCol_NavCursor),12,0,2);
                  if(hover) ImGui::SetTooltip("%s\n%s",m.name,m.mode==TileMode::Toggle?"Left click toggles. Right click opens settings.":"Click to open or run. Right click opens settings.");
                  ImGui::PopID();
              }
              ImGui::EndTable();
          }
      } else content(owner,state.page);
      ImGui::EndChild(); }
    ImGui::SetCursorScreenPos({pos.x+22,pos.y+size.y-33});
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(status.connected?style::Color(0x98DEC3):style::Color(0xD5B1AA),"%s",status.connected?"Connected":"Disconnected");
    ImGui::SameLine(); ImGui::TextDisabled("%s",status.version);
    ImGui::SameLine(std::max(8.f,size.x-219));
    if(ImGui::Button("Center",{60,27})) state.positioned=false;
    ImGui::SameLine(0,5); if(ImGui::Button("Reset",{60,27})) actions.reset=true;
    ImGui::SameLine(0,5); if(ImGui::Button("Close",{60,27})) actions.close=true;
    ImGui::End();
    if(actions.close) state.settingsOpen=false;
    if(state.settingsOpen) {
        const ImVec2 detailSize{std::min(480.f,std::max(160.f,work.x-24)),std::min(610.f,std::max(160.f,work.y-24))};
        const bool changed=!state.settingsWasOpen||state.settingsPage!=state.renderedSettingsPage||state.settingsSection!=state.renderedSettingsSection;
        auto& placement=state.settingsPlacement;
        if(!placement.positioned) { placement.position={state.position.x+size.x+14,state.position.y+30}; placement.positioned=true; }
        if(placement.dragging) {
            const auto mouse=ImGui::GetIO().MousePos;
            placement.position={mouse.x-viewport->WorkPos.x-placement.offset.x,mouse.y-viewport->WorkPos.y-placement.offset.y};
            if(!ImGui::IsMouseDown(0)) placement.dragging=false;
        }
        const float marginX=std::min(12.f,std::max(0.f,(work.x-detailSize.x)*.5f));
        const float marginY=std::min(12.f,std::max(0.f,(work.y-detailSize.y)*.5f));
        placement.position={std::clamp(placement.position.x,marginX,std::max(marginX,work.x-detailSize.x-marginX)),std::clamp(placement.position.y,marginY,std::max(marginY,work.y-detailSize.y-marginY))};
        const ImVec2 dp{viewport->WorkPos.x+placement.position.x,viewport->WorkPos.y+placement.position.y};
        ImGui::SetNextWindowPos(dp); ImGui::SetNextWindowSize(detailSize);
        if(changed||focusDetails) ImGui::SetNextWindowFocus();
        if(ImGui::Begin("mcov module settings",nullptr,flags)) {
            auto& dd=*ImGui::GetWindowDrawList(); LiquidGlass::Panel(dd,dp,{dp.x+detailSize.x,dp.y+detailSize.y},22);
            ImGui::SetCursorScreenPos({dp.x+12,dp.y+8}); ImGui::InvisibleButton("##detail_drag",{detailSize.x-60,49});
            if(ImGui::IsItemActivated()) { placement.dragging=true; const auto mouse=ImGui::GetIO().MousePos; placement.offset={mouse.x-dp.x,mouse.y-dp.y}; }
            if(ImGui::IsItemHovered()||placement.dragging) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            dd.AddText(style::MenuHeadingFont(),style::MenuHeadingFont()->FontSize,{dp.x+21,dp.y+17},ImGui::GetColorU32(ImGuiCol_Text),state.settingsLabel);
            ImGui::SetCursorScreenPos({dp.x+detailSize.x-43,dp.y+17});
            if(ImGui::Button("X",{27,27})) state.settingsOpen=false;
            ImGui::SetCursorScreenPos({dp.x+16,dp.y+65});
            if(changed) ImGui::SetNextWindowScroll({-1,0});
            ImGui::BeginChild("content",{detailSize.x-32,detailSize.y-81},ImGuiChildFlags_AlwaysUseWindowPadding);
            SectionScope selected(state.settingsSection); content(owner,state.settingsPage);
            ImGui::EndChild();
        }
        ImGui::End();
        state.renderedSettingsPage=state.settingsPage; state.renderedSettingsSection=state.settingsSection;
    }
    state.settingsWasOpen=state.settingsOpen;
    return actions;
}
bool BeginRows(const char* id) {
    if(!ImGui::BeginTable(id,1,ImGuiTableFlags_SizingStretchSame|ImGuiTableFlags_NoSavedSettings)) return false;
    ImGui::TableSetupColumn("Setting"); return true;
}
bool CheckRow(const char* label,bool* value) { ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); return ImGui::Checkbox(label,value); }
bool ComboRow(const char* label,int* value,const char* const* items,int count) {
    ImGui::PushID(label); RowLabel(label); const bool changed=ImGui::Combo("##value",value,items,count); ImGui::PopID(); return changed;
}
bool SliderRow(const char* label,float* value,float min,float max,const char* format) { return Slider(label,value,min,max,format,ImGuiDataType_Float); }
bool SliderDoubleRow(const char* label,double* value,double min,double max,const char* format) { return Slider(label,value,min,max,format,ImGuiDataType_Double); }
bool SliderIntRow(const char* label,int* value,int min,int max,const char* format) { return Slider(label,value,min,max,format,ImGuiDataType_S32); }
bool ColorRow(const char* label,float* value) {
    ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::PushID(label);
    ImGui::TextUnformatted(label); ImGui::SameLine(std::max(0.f,ImGui::GetContentRegionAvail().x-25));
    const bool changed=ImGui::ColorEdit4("##value",value,ImGuiColorEditFlags_NoInputs|ImGuiColorEditFlags_NoLabel);
    ImGui::PopID(); return changed;
}
void ValueRow(const char* label,const char* value) { RowLabel(label); ImGui::TextWrapped("%s",value); }
} // namespace mcov::control_panel

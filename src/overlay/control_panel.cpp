#include "control_panel.hpp"
#include "../common/flat_style.hpp"
#include "../common/classic_style.hpp"
#include "imgui_internal.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace mcov::control_panel {
namespace {
float gScale=1;
using MenuStyle=style::ClassicStyle;
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
        for(auto c:{ImGuiCol_FrameBg,ImGuiCol_FrameBgHovered,ImGuiCol_FrameBgActive,ImGuiCol_SliderGrab,ImGuiCol_SliderGrabActive,ImGuiCol_Text})
            invisible.Color(c,ImVec4(0,0,0,0));
        invisible.Var(ImGuiStyleVar_FramePadding,ImVec2(0,0)); invisible.Var(ImGuiStyleVar_FrameBorderSize,0.f);
        changed=ImGui::SliderScalar("##value",type,value,&low,&high,format,ImGuiSliderFlags_AlwaysClamp|ImGuiSliderFlags_NoRoundToFormat);
    }
    if(!ImGui::TempInputIsActive(ImGui::GetID("##value"))) {
        const float height=ImGui::GetFontSize(); const float fraction=std::clamp(float(*value-low)/float(high-low),0.f,1.f);
        auto& draw=*ImGui::GetWindowDrawList();
        draw.AddRectFilled(p,{p.x+width,p.y+height},ImGui::GetColorU32(ImGuiCol_FrameBg));
        draw.AddRectFilled({p.x+1,p.y+1},{p.x+1+(width-2)*fraction,p.y+height-1},ImGui::GetColorU32(ImGuiCol_CheckMark));
        draw.AddRect(p,{p.x+width,p.y+height},IM_COL32(0,0,0,255));
        char text[64]; if(type==ImGuiDataType_S32) std::snprintf(text,sizeof(text),format,int(*value));
        else std::snprintf(text,sizeof(text),format,double(*value));
        const auto extent=ImGui::CalcTextSize(text);
        const ImVec2 tp{p.x+(width-extent.x)*.5f,p.y};
        draw.AddText({tp.x+1,tp.y+1},IM_COL32(0,0,0,255),text); draw.AddText(tp,ImGui::GetColorU32(ImGuiCol_Text),text);
        if(ImGui::IsItemFocused()) draw.AddRect(p,{p.x+width,p.y+height},ImGui::GetColorU32(ImGuiCol_NavCursor));
    }
    ImGui::PopID(); return changed;
}
}
float Scale() { return gScale; }
const char* ThemeName(int) { return "Classic violet"; }
bool MotionAllowed(const State& state) { return !state.reduceMotion; }
void Section(const char* label) {
    ImGui::Spacing();
    const auto p=ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddLine(p,{p.x+ImGui::GetContentRegionAvail().x,p.y},ImGui::GetColorU32(ImGuiCol_CheckMark),1);
    ImGui::Dummy({0,2}); if(label&&*label) ImGui::TextUnformatted(label);
}
int SubTabs(const char* id,const char* const* labels,int count,int selected,bool selectNext) {
    selected=std::clamp(selected,0,count-1);
    const int previous=selected;
    style::StyleScope colors;
    colors.Color(ImGuiCol_TabSelected,ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));
    colors.Color(ImGuiCol_TabHovered,ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
    colors.Color(ImGuiCol_TabSelectedOverline,ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));
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
Actions Render(State& state,const Status& status,void* owner,Content content) {
    MenuStyle scoped; Actions actions;
    const auto previousPage=state.renderedPage;
    const auto* viewport=ImGui::GetMainViewport(); const auto work=viewport->WorkSize;
    const ImVec2 size{std::floor(std::min(600.f,std::max(160.f,work.x-24))),std::floor(std::min(650.f,std::max(160.f,work.y-24)))};
    if(!state.positioned) { state.position={(work.x-size.x)*.5f,(work.y-size.y)*.5f}; state.positioned=true; }
    if(state.dragging) {
        const auto m=ImGui::GetIO().MousePos;
        state.position={m.x-viewport->WorkPos.x-state.dragOffset.x,m.y-viewport->WorkPos.y-state.dragOffset.y};
        if(!ImGui::IsMouseDown(0)) state.dragging=false;
    }
    state.position={std::clamp(state.position.x,0.f,std::max(0.f,work.x-size.x)),std::clamp(state.position.y,0.f,std::max(0.f,work.y-size.y))};
    const ImVec2 pos{std::floor(viewport->WorkPos.x+state.position.x),std::floor(viewport->WorkPos.y+state.position.y)};
    ImGui::SetNextWindowPos(pos,ImGuiCond_Always); ImGui::SetNextWindowSize(size,ImGuiCond_Always);
    const auto flags=ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoScrollbar;
    if(!ImGui::Begin("mcov control panel",nullptr,flags)) { ImGui::End(); return actions; }
    auto& draw=*ImGui::GetWindowDrawList();
    draw.AddLine({pos.x+1,pos.y+1},{pos.x+size.x-1,pos.y+1},ImGui::GetColorU32(ImGuiCol_CheckMark),2);
    ImGui::SetCursorScreenPos({pos.x+5,pos.y+4}); ImGui::InvisibleButton("##drag",{size.x-10,20});
    if(ImGui::IsItemActivated()) { state.dragging=true; const auto m=ImGui::GetIO().MousePos; state.dragOffset={m.x-pos.x,m.y-pos.y}; }
    if(ImGui::IsItemHovered()||state.dragging) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    draw.AddText({pos.x+7,pos.y+5},ImGui::GetColorU32(ImGuiCol_Text),"mcov");
    ImGui::SetCursorScreenPos({pos.x+8,pos.y+27});
    ImGui::BeginChild("navigation",{size.x-16,31},ImGuiChildFlags_None,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoBackground);
    constexpr const char* labels[]={"Combat","Visuals","Blocks","HUD","Stats","Settings"};
    const float tabWidth=(size.x-16)/6;
    for(int i=0;i<6;++i) {
        if(i) ImGui::SameLine(0,0);
        style::StyleScope selected;
        if(state.page==static_cast<Page>(i)) selected.Color(ImGuiCol_Button,ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
        if(ImGui::Button(labels[i],{tabWidth,30})) state.page=static_cast<Page>(i);
        if(state.page==static_cast<Page>(i)) { const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax(); ImGui::GetWindowDrawList()->AddLine(a,{b.x,a.y},ImGui::GetColorU32(ImGuiCol_CheckMark),2); }
    }
    ImGui::EndChild(); state.renderedPage=state.page; state.pageAlpha=1; state.animateNextPage=false;
    ImGui::SetCursorScreenPos({pos.x+8,pos.y+64});
    { style::StyleScope padding; padding.Var(ImGuiStyleVar_WindowPadding,ImVec2(5,5));
      if(state.page!=previousPage) ImGui::SetNextWindowScroll({-1,0});
      ImGui::BeginChild("content",{size.x-16,size.y-96},ImGuiChildFlags_Borders|ImGuiChildFlags_AlwaysUseWindowPadding);
      content(owner,state.page); ImGui::EndChild(); }
    ImGui::SetCursorScreenPos({pos.x+8,pos.y+size.y-25});
    ImGui::TextDisabled("%s | %s",status.connected?"Connected":"Disconnected",status.version);
    ImGui::SameLine(std::max(8.f,size.x-181));
    if(ImGui::Button("Center",{55,18})) state.positioned=false;
    ImGui::SameLine(); if(ImGui::Button("Reset",{50,18})) actions.reset=true;
    ImGui::SameLine(); if(ImGui::Button("Close",{50,18})) actions.close=true;
    ImGui::End(); return actions;
}
bool BeginRows(const char* id) {
    if(!ImGui::BeginTable(id,1,ImGuiTableFlags_BordersOuter|ImGuiTableFlags_SizingStretchSame|ImGuiTableFlags_NoSavedSettings)) return false;
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

#include "control_panel.hpp"
#include "../common/compact_style.hpp"
#include "imgui_internal.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace mcov::control_panel {
namespace {
float gScale=1;
thread_local int gSection=-1;
constexpr const char* kPages[]={"Combat","Visuals","Blocks","HUD","Stats","Settings","Misc"};

bool Matches(const char* text,const char* query) {
    if(!*query) return true;
    for(;*text;++text) {
        const char* a=text; const char* b=query;
        while(*a&&*b&&std::tolower(static_cast<unsigned char>(*a))==std::tolower(static_cast<unsigned char>(*b))) { ++a; ++b; }
        if(!*b) return true;
    }
    return false;
}
void Icon(ImDrawList& draw,ImVec2 p,int kind,ImU32 color) {
    constexpr float w=1.25f;
    if(kind==0) { draw.AddCircle(p,5.5f,color,16,w); draw.AddLine({p.x-8,p.y},{p.x+8,p.y},color,w); draw.AddLine({p.x,p.y-8},{p.x,p.y+8},color,w); }
    else if(kind==1) { draw.AddCircle(p,6,color,16,w); draw.AddCircleFilled(p,2,color,12); }
    else if(kind==2) { draw.AddRect({p.x-5,p.y-5},{p.x+5,p.y+5},color,2,0,w); draw.AddLine({p.x-5,p.y},{p.x+5,p.y},color,w); }
    else if(kind==3) { draw.AddRect({p.x-6,p.y-4},{p.x+6,p.y+4},color,2,0,w); draw.AddLine({p.x,p.y+4},{p.x,p.y+7},color,w); }
    else if(kind==4) { for(int i=0;i<3;++i) draw.AddLine({p.x-5+i*5,p.y+5},{p.x-5+i*5,p.y+1-i*3},color,2); }
    else if(kind==5) { draw.AddCircle(p,6,color,12,w); draw.AddCircle(p,2,color,12,w); }
    else { for(int y:{-3,3}) for(int x:{-3,3}) draw.AddRect({p.x+x-2,p.y+y-2},{p.x+x+2,p.y+y+2},color,1,0,w); }
}
void RowLabel(const char* label) {
    ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding(); ImGui::TextWrapped("%s",label);
    ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1);
}
template<class T> bool Slider(const char* label,T* value,T low,T high,const char* format,ImGuiDataType type) {
    ImGui::PushID(label); RowLabel(label);
    const auto p=ImGui::GetCursorScreenPos(); const float width=ImGui::GetContentRegionAvail().x;
    const auto id=ImGui::GetID("##value"); const auto& ctx=*ImGui::GetCurrentContext();
    const bool editing=ImGui::TempInputIsActive(id)||
        (ImGui::GetIO().KeyCtrl&&ImGui::IsMouseClicked(0)&&ImGui::IsMouseHoveringRect(p,{p.x+width,p.y+ImGui::GetFrameHeight()}))||
        (ctx.NavActivateId==id&&(ctx.NavActivateFlags&ImGuiActivateFlags_PreferInput));
    bool changed;
    {
        style::StyleScope hidden;
        if(!editing) for(auto c:{ImGuiCol_FrameBg,ImGuiCol_FrameBgHovered,ImGuiCol_FrameBgActive,ImGuiCol_SliderGrab,ImGuiCol_SliderGrabActive})
            hidden.Color(c,ImVec4(0,0,0,0));
        changed=ImGui::SliderScalar("##value",type,value,&low,&high,editing?format:"",
            ImGuiSliderFlags_AlwaysClamp|ImGuiSliderFlags_NoRoundToFormat);
    }
    if(!ImGui::TempInputIsActive(id)) {
        auto& draw=*ImGui::GetWindowDrawList(); const auto max=ImGui::GetItemRectMax();
        const float fraction=std::clamp(float((*value-low)/(double(high)-double(low))),0.f,1.f);
        draw.AddRectFilled(p,max,ImGui::GetColorU32(ImGui::IsItemHovered()?ImGuiCol_FrameBgHovered:ImGuiCol_FrameBg),4);
        const float fill=std::max(4.f,(max.x-p.x)*fraction);
        if(fraction>0) draw.AddRectFilled(p,{p.x+fill,max.y},ImGui::GetColorU32(ImGui::IsItemActive()?ImGuiCol_SliderGrabActive:ImGuiCol_SliderGrab),4);
        char text[64]; if(type==ImGuiDataType_S32) std::snprintf(text,sizeof(text),format,int(*value));
        else std::snprintf(text,sizeof(text),format,double(*value));
        const auto extent=ImGui::CalcTextSize(text);
        // A dark value lozenge keeps small text readable over every fill level.
        const ImVec2 tp{p.x+(max.x-p.x-extent.x)*.5f,p.y+(max.y-p.y-extent.y)*.5f};
        draw.AddRectFilled({tp.x-4,tp.y},{tp.x+extent.x+4,tp.y+extent.y},ImGui::GetColorU32(style::Color(0x25252C,.72f)),3);
        draw.AddText(tp,ImGui::GetColorU32(ImGuiCol_Text),text);
        if(ImGui::IsItemFocused()) draw.AddRect(p,max,ImGui::GetColorU32(ImGuiCol_NavCursor),4,0,1);
    }
    ImGui::PopID(); return changed;
}
} // namespace
float Scale() { return gScale; }
SectionScope::SectionScope(int section):previous_(gSection) { gSection=section; }
SectionScope::~SectionScope() { gSection=previous_; }
const char* ThemeName(int) { return "Translucent dark"; }
bool MotionAllowed(const State& state) { return !state.reduceMotion; }
void Section(const char* label) {
    ImGui::Dummy({0,8});
    if(label&&*label) { ImGui::PushFont(style::MenuFont(true)); ImGui::TextUnformatted(label); ImGui::PopFont(); }
    ImGui::Separator(); ImGui::Dummy({0,2});
}
int SubTabs(const char* id,const char* const* labels,int count,int selected,bool selectNext) {
    if(gSection>=0) return std::clamp(gSection,0,count-1);
    selected=std::clamp(selected,0,count-1);
    if(ImGui::BeginTabBar(id,ImGuiTabBarFlags_FittingPolicyScroll)) {
        for(int i=0;i<count;++i) if(ImGui::BeginTabItem(labels[i],nullptr,selectNext&&i==selected?ImGuiTabItemFlags_SetSelected:0)) {
            selected=i; ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    return selected;
}
Actions RenderMenu(State& state,const Status& status,void* owner,Content content,Catalog catalog,Activate activate) {
    style::CompactStyle scoped; Actions actions;
    const auto* viewport=ImGui::GetMainViewport(); const auto work=viewport->WorkSize;
    if(work.x<180||work.y<180) return actions;
    const ImVec2 size{std::floor(std::min(740.f,work.x-24)),std::floor(std::min(520.f,work.y-24))};
    if(!state.positioned) {
        // Reserve space for the independent settings pane on desktop viewports.
        const float reserve=std::min(496.f,std::max(0.f,work.x-size.x-24));
        state.position={(work.x-size.x-reserve)*.5f,(work.y-size.y)*.5f}; state.positioned=true;
    }
    if(state.dragging) {
        const auto m=ImGui::GetIO().MousePos;
        state.position={m.x-viewport->WorkPos.x-state.dragOffset.x,m.y-viewport->WorkPos.y-state.dragOffset.y};
        if(!ImGui::IsMouseDown(0)) state.dragging=false;
    }
    state.position={std::clamp(state.position.x,0.f,std::max(0.f,work.x-size.x)),std::clamp(state.position.y,0.f,std::max(0.f,work.y-size.y))};
    const ImVec2 pos{std::floor(viewport->WorkPos.x+state.position.x),std::floor(viewport->WorkPos.y+state.position.y)};
    ImGui::SetNextWindowPos(pos); ImGui::SetNextWindowSize(size);
    constexpr auto flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|
        ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoScrollWithMouse;
    bool openSettings=false;
    if(!ImGui::Begin("mcov control panel",nullptr,flags)) { ImGui::End(); return actions; }
    auto& draw=*ImGui::GetWindowDrawList();
    const float sidebar=size.x>=620?150.f:112.f;
    draw.AddRectFilled(pos,{pos.x+size.x,pos.y+size.y},ImGui::GetColorU32(ImGuiCol_WindowBg),12);
    draw.AddRectFilled(pos,{pos.x+sidebar,pos.y+size.y},ImGui::GetColorU32(style::Color(0x15151B,.22f)),12,ImDrawFlags_RoundCornersLeft);
    draw.AddLine({pos.x+sidebar,pos.y+12},{pos.x+sidebar,pos.y+size.y-12},ImGui::GetColorU32(ImGuiCol_Separator));
    ImGui::SetCursorScreenPos({pos.x+8,pos.y+5}); ImGui::InvisibleButton("##drag",{std::max(1.f,size.x-58),32});
    if(ImGui::IsItemActivated()) { state.dragging=true; const auto m=ImGui::GetIO().MousePos; state.dragOffset={m.x-pos.x,m.y-pos.y}; }
    if(ImGui::IsItemHovered()||state.dragging) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    draw.AddCircle({pos.x+25,pos.y+23},6,ImGui::GetColorU32(ImGuiCol_CheckMark),20,1.5f);
    draw.AddCircleFilled({pos.x+25,pos.y+23},2.3f,ImGui::GetColorU32(ImGuiCol_CheckMark),16);
    draw.AddText(style::MenuFont(true),14,{pos.x+40,pos.y+16},ImGui::GetColorU32(ImGuiCol_Text),"mcov");
    draw.AddText(style::MenuFont(true),14,{pos.x+sidebar+22,pos.y+16},ImGui::GetColorU32(ImGuiCol_Text),kPages[int(state.page)]);
    ImGui::SetCursorScreenPos({pos.x+size.x-34,pos.y+12});
    if(ImGui::SmallButton("X##close")) actions.close=true;
    ImGui::SetCursorScreenPos({pos.x+14,pos.y+48}); ImGui::SetNextItemWidth(sidebar-28);
    const bool filterChanged=ImGui::InputTextWithHint("##module_search","Search",state.search,sizeof(state.search));
    ImGui::SetCursorScreenPos({pos.x+10,pos.y+89});
    if(ImGui::BeginChild("navigation",{sidebar-20,std::max(1.f,size.y-142)},ImGuiChildFlags_None,ImGuiWindowFlags_NoBackground)) {
        for(int i=0;i<IM_ARRAYSIZE(kPages);++i) {
            const auto p=ImGui::GetCursorScreenPos(); const bool active=state.page==static_cast<Page>(i);
            style::StyleScope selected;
            selected.Var(ImGuiStyleVar_ButtonTextAlign,ImVec2(0,.5f)); selected.Var(ImGuiStyleVar_FramePadding,ImVec2(32,4));
            selected.Color(ImGuiCol_Button,active?style::Color(0xFFFFFF,.065f):ImVec4(0,0,0,0));
            selected.Color(ImGuiCol_Text,active?style::Color(0xF5D0C4):style::Color(0xC2C2CC));
            if(ImGui::Button(kPages[i],{-1,29})) state.page=static_cast<Page>(i);
            Icon(*ImGui::GetWindowDrawList(),{p.x+15,p.y+14.5f},i,ImGui::GetColorU32(active?ImGuiCol_CheckMark:ImGuiCol_TextDisabled));
        }
    }
    ImGui::EndChild();
    const bool pageChanged=state.page!=state.renderedPage;
    if(pageChanged) { state.search[0]='\0'; state.settingsOpen=false; ImGui::SetWindowFocus(); }
    std::array<ModuleView,64> modules{};
    const int count=catalog?std::clamp(catalog(owner,state.page,modules),0,int(modules.size())):0;
    int& selected=state.selectedSections[int(state.page)]; selected=std::clamp(selected,0,std::max(0,count-1));
    int matching=0;
    for(int i=0;i<count;++i) if(Matches(modules[i].name,state.search)) ++matching;
    ImGui::SetCursorScreenPos({pos.x+sidebar+17,pos.y+48});
    if(pageChanged||filterChanged) ImGui::SetNextWindowScroll({-1,0});
    if(ImGui::BeginChild("module_grid",{std::max(1.f,size.x-sidebar-34),std::max(1.f,size.y-106)},ImGuiChildFlags_None,ImGuiWindowFlags_NoBackground)) {
        if(catalog&&!matching) {
            ImGui::TextDisabled("No modules match your search.");
            if(ImGui::Button("Clear search")) state.search[0]='\0';
        } else if(catalog) {
            const int columns=std::clamp(int(ImGui::GetContentRegionAvail().x/174.f),1,3);
            style::StyleScope grid;
            grid.Var(ImGuiStyleVar_CellPadding,ImVec2(5,5));
            if(ImGui::BeginTable("module_widgets",columns,ImGuiTableFlags_SizingStretchSame|ImGuiTableFlags_NoSavedSettings)) {
                for(int i=0;i<count;++i) {
                    const auto& module=modules[i]; if(!Matches(module.name,state.search)) continue;
                    ImGui::TableNextColumn(); ImGui::PushID(i);
                    const auto p=ImGui::GetCursorScreenPos(); const float width=ImGui::GetContentRegionAvail().x;
                    const bool active=module.mode==TileMode::Toggle&&module.enabled;
                    bool clicked=false,details=false;
                    {
                        style::StyleScope widget;
                        widget.Var(ImGuiStyleVar_FrameRounding,8);
                        widget.Color(ImGuiCol_Button,active?style::Color(0xEF8B70,.10f):style::Color(0xFFFFFF,.045f));
                        widget.Color(ImGuiCol_ButtonHovered,active?style::Color(0xEF8B70,.16f):style::Color(0xFFFFFF,.085f));
                        widget.Color(ImGuiCol_ButtonActive,style::Color(0xEF8B70,.22f));
                        clicked=ImGui::Button("##module_widget",{width,66});
                        details=ImGui::IsItemClicked(ImGuiMouseButton_Right)||
                            (ImGui::IsItemFocused()&&(ImGui::IsKeyPressed(ImGuiKey_Menu)||
                                (ImGui::GetIO().KeyShift&&ImGui::IsKeyPressed(ImGuiKey_F10))));
                    }
                    const bool hovered=ImGui::IsItemHovered();
                    auto& tile=*ImGui::GetWindowDrawList();
                    tile.PushClipRect({p.x+12,p.y+10},{p.x+width-12,p.y+31},true);
                    tile.AddText(style::MenuFont(true),14,{p.x+12,p.y+12},ImGui::GetColorU32(ImGuiCol_Text),module.name);
                    tile.PopClipRect();
                    const char* label=module.mode==TileMode::Toggle?(active?"Enabled":"Disabled"):
                        module.mode==TileMode::Action?"Run action":"Open settings";
                    tile.AddText({p.x+12,p.y+39},ImGui::GetColorU32(active?ImGuiCol_CheckMark:ImGuiCol_TextDisabled),label);
                    if(module.mode==TileMode::Toggle) {
                        const ImVec2 a{p.x+width-38,p.y+39},b{p.x+width-12,p.y+53};
                        tile.AddRectFilled(a,b,ImGui::GetColorU32(active?ImGuiCol_CheckMark:ImGuiCol_FrameBgActive),7);
                        tile.AddCircleFilled({active?b.x-7:a.x+7,a.y+7},4,ImGui::GetColorU32(active?style::Color(0x25252C):style::Color(0xAEAEB8)),12);
                    }
                    if(hovered) {
                        ImGui::BeginTooltip(); ImGui::TextUnformatted(module.name);
                        if(module.binding[0]) ImGui::TextDisabled("%s",module.binding);
                        ImGui::TextDisabled("%s",module.mode==TileMode::Toggle?"Click to toggle. Right-click for settings.":
                            module.mode==TileMode::Action?"Click to run. Right-click for settings.":"Click to open settings.");
                        ImGui::EndTooltip();
                    }
                    if(clicked) {
                        if(module.mode==TileMode::Settings) details=true;
                        else if(activate) activate(owner,state.page,i);
                    }
                    if(details) {
                        selected=i; state.settingsPage=state.page; state.settingsSection=i;
                        state.settingsOpen=true; openSettings=true;
                        std::snprintf(state.settingsLabel,sizeof(state.settingsLabel),"%s",module.name);
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        } else if(content) { SectionScope section(-1); content(owner,state.page); }
    }
    ImGui::EndChild();
    ImGui::SetCursorScreenPos({pos.x+18,pos.y+size.y-34});
    draw.AddCircleFilled({pos.x+22,pos.y+size.y-23},2.5f,ImGui::GetColorU32(status.connected?style::Color(0xA5C7AD):style::Color(0xEF8B70)),12);
    ImGui::SetCursorScreenPos({pos.x+32,pos.y+size.y-30}); ImGui::TextDisabled("%s",status.connected?"Connected":"Offline");
    ImGui::SetCursorScreenPos({pos.x+sidebar+22,pos.y+size.y-48}); ImGui::TextDisabled("Right-click a module for settings");
    ImGui::SetCursorScreenPos({pos.x+sidebar+22,pos.y+size.y-30}); ImGui::TextDisabled("%s",status.version);
    ImGui::SameLine(std::max(sidebar+80,size.x-206));
    if(ImGui::SmallButton("Center")) { state.positioned=false; state.settingsPlacement.positioned=false; }
    ImGui::SameLine(0,9); if(ImGui::SmallButton("Reset")) actions.reset=true;
    ImGui::SameLine(0,9); if(ImGui::SmallButton("Close")) actions.close=true;
    ImGui::End();
    if(actions.close) state.settingsOpen=false;
    state.renderedPage=state.page; state.pageAlpha=1; state.animateNextPage=false;

    if(state.settingsOpen&&catalog&&content) {
        std::array<ModuleView,64> details{};
        const int detailCount=std::clamp(catalog(owner,state.settingsPage,details),0,int(details.size()));
        if(state.settingsSection<0||state.settingsSection>=detailCount) state.settingsOpen=false;
        else {
            const auto& module=details[state.settingsSection];
            auto& placement=state.settingsPlacement;
            const ImVec2 detailSize{std::floor(std::min(480.f,work.x-24)),std::floor(std::min(580.f,work.y-24))};
            if(!placement.positioned) {
                float x=state.position.x+size.x+16;
                if(x+detailSize.x>work.x-12) x=state.position.x>=detailSize.x+16?
                    state.position.x-detailSize.x-16:work.x-detailSize.x-12;
                placement.position={x,state.position.y}; placement.positioned=true;
            }
            if(placement.dragging) {
                const auto m=ImGui::GetIO().MousePos;
                placement.position={m.x-viewport->WorkPos.x-placement.offset.x,m.y-viewport->WorkPos.y-placement.offset.y};
                if(!ImGui::IsMouseDown(0)) placement.dragging=false;
            }
            placement.position={std::clamp(placement.position.x,0.f,std::max(0.f,work.x-detailSize.x)),
                std::clamp(placement.position.y,0.f,std::max(0.f,work.y-detailSize.y))};
            const ImVec2 detailPos{std::floor(viewport->WorkPos.x+placement.position.x),std::floor(viewport->WorkPos.y+placement.position.y)};
            ImGui::SetNextWindowPos(detailPos); ImGui::SetNextWindowSize(detailSize);
            if(openSettings) ImGui::SetNextWindowFocus();
            constexpr auto detailFlags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoScrollWithMouse;
            if(ImGui::Begin("mcov module settings",nullptr,detailFlags)) {
                ImGui::SetCursorScreenPos({detailPos.x+8,detailPos.y+5});
                ImGui::InvisibleButton("##drag",{std::max(1.f,detailSize.x-154),32});
                if(ImGui::IsItemActivated()) { placement.dragging=true; const auto m=ImGui::GetIO().MousePos; placement.offset={m.x-detailPos.x,m.y-detailPos.y}; }
                if(ImGui::IsItemHovered()||placement.dragging) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
                auto& settingsDraw=*ImGui::GetWindowDrawList();
                settingsDraw.PushClipRect({detailPos.x+16,detailPos.y+12},{detailPos.x+detailSize.x-154,detailPos.y+34},true);
                settingsDraw.AddText(style::MenuFont(true),14,{detailPos.x+16,detailPos.y+16},ImGui::GetColorU32(ImGuiCol_Text),module.name);
                settingsDraw.PopClipRect();
                ImGui::SetCursorScreenPos({detailPos.x+detailSize.x-141,detailPos.y+12});
                if(module.mode==TileMode::Toggle&&activate) {
                    bool enabled=module.enabled;
                    if(ImGui::Checkbox("Enabled##module_enabled",&enabled)) activate(owner,state.settingsPage,state.settingsSection);
                } else if(module.mode==TileMode::Action&&activate&&ImGui::SmallButton("Run action"))
                    activate(owner,state.settingsPage,state.settingsSection);
                ImGui::SetCursorScreenPos({detailPos.x+detailSize.x-34,detailPos.y+12});
                if(ImGui::SmallButton("X##details_close")) state.settingsOpen=false;
                settingsDraw.AddLine({detailPos.x+16,detailPos.y+42},{detailPos.x+detailSize.x-16,detailPos.y+42},ImGui::GetColorU32(ImGuiCol_Separator));
                const bool changed=state.settingsPage!=state.renderedSettingsPage||state.settingsSection!=state.renderedSettingsSection;
                ImGui::SetCursorScreenPos({detailPos.x+16,detailPos.y+52});
                if(changed) ImGui::SetNextWindowScroll({-1,0});
                if(ImGui::BeginChild("content",{std::max(1.f,detailSize.x-32),std::max(1.f,detailSize.y-68)},ImGuiChildFlags_None,ImGuiWindowFlags_NoBackground)) {
                    SectionScope section(state.settingsSection); content(owner,state.settingsPage);
                }
                ImGui::EndChild();
            }
            ImGui::End();
            state.renderedSettingsPage=state.settingsPage; state.renderedSettingsSection=state.settingsSection;
        }
    }
    if(!state.settingsOpen) state.settingsPlacement.dragging=false;
    state.settingsWasOpen=state.settingsOpen;
    return actions;
}
Actions Render(State& state,const Status& status,void* owner,Content content,Catalog catalog,Activate activate) {
    return RenderMenu(state,status,owner,content,catalog,activate);
}
bool BeginRows(const char* id) {
    if(!ImGui::BeginTable(id,2,ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_NoSavedSettings)) return false;
    ImGui::TableSetupColumn("Setting",ImGuiTableColumnFlags_WidthStretch,.58f);
    ImGui::TableSetupColumn("Value",ImGuiTableColumnFlags_WidthStretch,.42f);
    return true;
}
bool CheckRow(const char* label,bool* value) {
    ImGui::PushID(label); ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
    bool changed=ImGui::Selectable(label,false,ImGuiSelectableFlags_None,{0,ImGui::GetFrameHeight()});
    if(changed) *value=!*value;
    ImGui::TableSetColumnIndex(1);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX()+std::max(0.f,ImGui::GetContentRegionAvail().x-ImGui::GetFrameHeight()));
    changed=ImGui::Checkbox("##value",value)||changed;
    ImGui::PopID(); return changed;
}
bool ComboRow(const char* label,int* value,const char* const* items,int count) {
    ImGui::PushID(label); RowLabel(label); const bool changed=ImGui::Combo("##value",value,items,count); ImGui::PopID(); return changed;
}
bool SliderRow(const char* label,float* value,float min,float max,const char* format) { return Slider(label,value,min,max,format,ImGuiDataType_Float); }
bool SliderDoubleRow(const char* label,double* value,double min,double max,const char* format) { return Slider(label,value,min,max,format,ImGuiDataType_Double); }
bool SliderIntRow(const char* label,int* value,int min,int max,const char* format) { return Slider(label,value,min,max,format,ImGuiDataType_S32); }
bool ColorRow(const char* label,float* value) {
    ImGui::PushID(label); RowLabel(label);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX()+std::max(0.f,ImGui::GetContentRegionAvail().x-ImGui::GetFrameHeight()));
    const bool changed=ImGui::ColorEdit4("##value",value,ImGuiColorEditFlags_NoInputs|ImGuiColorEditFlags_NoLabel);
    ImGui::PopID(); return changed;
}
void ValueRow(const char* label,const char* value) { RowLabel(label); ImGui::TextWrapped("%s",value); }
} // namespace mcov::control_panel

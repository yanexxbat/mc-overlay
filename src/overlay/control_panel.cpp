#include "control_panel.hpp"
#include "../common/workspace_style.hpp"
#include "imgui_internal.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace mcov::control_panel {
namespace {
float gScale=1;
thread_local int gSection=-1;
constexpr const char* kPages[]={"Combat","Visuals","Blocks","HUD","Stats","Settings","Misc"};
constexpr const char* kDescriptions[]={
    "Aim, timing and item controls.", "Fine-tune what you see.",
    "Explore blocks and your surroundings.", "Your information, at a glance.",
    "Player and client information.", "Make Keim feel like yours.",
    "Small tools for everyday play."};
constexpr unsigned kMenuAccent=style::workspace::AccentRGB;

// Scoped adapter for legacy row callbacks. No animation state is global, and
// nested panels restore the previous owner when returning from their callback.
thread_local motion::State* currentMotion=nullptr;
struct MotionScope {
    motion::State* previous=currentMotion;
    explicit MotionScope(motion::State& state) { currentMotion=&state; }
    ~MotionScope() { currentMotion=previous; }
};
float Animate(ImGuiID id,float target,float duration=motion::Transition,float initial=0) {
    return currentMotion?currentMotion->To(id,target,duration,initial):target;
}
bool MotionButton(const char* label,ImVec2 size={}) {
    const auto id=ImGui::GetID(label);
    const auto base=ImGui::GetStyleColorVec4(ImGuiCol_Button);
    const auto over=ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered);
    const auto down=ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
    const bool enabled=currentMotion&&currentMotion->enabled;
    const float hover=enabled?currentMotion->widgets[id].value:0;
    const float press=enabled?currentMotion->widgets[id^0x731ac52u].value:0;
    const auto fill=motion::Mix(motion::Mix(base,over,hover),down,press);
    style::StyleScope colors;
    colors.Color(ImGuiCol_Button,fill);
    colors.Color(ImGuiCol_ButtonHovered,enabled?fill:over);
    colors.Color(ImGuiCol_ButtonActive,enabled?fill:down);
    const bool clicked=ImGui::Button(label,size);
    Animate(id,ImGui::IsItemHovered()?1.f:0.f,motion::Hover);
    Animate(id^0x731ac52u,ImGui::IsItemActive()?1.f:0.f,motion::Press);
    return clicked;
}
bool MotionSmallButton(const char* label) {
    style::StyleScope padding;
    padding.Var(ImGuiStyleVar_FramePadding,ImVec2(ImGui::GetStyle().FramePadding.x,0));
    return MotionButton(label);
}
bool MotionCheckbox(const char* label,bool* value) {
    const auto id=ImGui::GetID(label);
    const float initial=*value?1.f:0.f;
    const auto p=ImGui::GetCursorScreenPos();
    bool changed;
    {
        style::StyleScope hide; hide.Color(ImGuiCol_CheckMark,ImVec4(0,0,0,0));
        changed=ImGui::Checkbox(label,value);
    }
    const float t=Animate(id,*value?1.f:0.f,motion::Focus,initial);
    const float extent=ImGui::GetFrameHeight();
    const ImVec2 a{p.x+extent*.25f,p.y+extent*.5f},b{p.x+extent*.43f,p.y+extent*.7f},c{p.x+extent*.76f,p.y+extent*.28f};
    const float first=std::clamp(t*3,0.f,1.f),second=std::clamp((t-1.f/3)*1.5f,0.f,1.f);
    auto& draw=*ImGui::GetWindowDrawList(); const auto ink=ImGui::GetColorU32(ImGuiCol_CheckMark);
    if(first>0) draw.AddLine(a,{a.x+(b.x-a.x)*first,a.y+(b.y-a.y)*first},ink,2);
    if(second>0) draw.AddLine(b,{b.x+(c.x-b.x)*second,b.y+(c.y-b.y)*second},ink,2);
    return changed;
}bool FilterTab(const char* label,int count,bool selected,float width) {
    ImGui::PushID(label);
    const auto p=ImGui::GetCursorScreenPos();
    style::StyleScope tab;
    tab.Color(ImGuiCol_Button,selected?style::Color(0xFFFFFF,.07f):ImVec4(0,0,0,0));
    const bool clicked=MotionButton("##filter",{width,28});
    char text[48]; std::snprintf(text,sizeof(text),"%s  %d",label,count);
    auto& draw=*ImGui::GetWindowDrawList();
    draw.AddText({p.x+10,p.y+7},ImGui::GetColorU32(selected?ImGuiCol_Text:ImGuiCol_TextDisabled),text);
    if(selected) draw.AddRectFilled({p.x+10,p.y+27},{p.x+width-10,p.y+29},ImGui::GetColorU32(ImGuiCol_CheckMark),1);
    ImGui::PopID(); return clicked;
}

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
template<class T> bool Slider(const char* label,T* value,T low,T high,const char* format,ImGuiDataType type,bool zeroIsOff=false) {
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
        if(fraction>0) {
            draw.AddRectFilled(p,{p.x+fill,max.y},ImGui::GetColorU32(style::Color(kMenuAccent,.13f)),4);
            draw.AddRectFilled({p.x,max.y-2},{p.x+fill,max.y},ImGui::GetColorU32(ImGuiCol_SliderGrab),1);
        }
        char text[64]; if(zeroIsOff&&*value==0) std::snprintf(text,sizeof(text),"Off");
        else if(type==ImGuiDataType_S32) std::snprintf(text,sizeof(text),format,int(*value));
        else std::snprintf(text,sizeof(text),format,double(*value));
        const auto extent=ImGui::CalcTextSize(text);
        const ImVec2 tp{p.x+(max.x-p.x-extent.x)*.5f,p.y+(max.y-p.y-extent.y)*.5f};
        draw.AddText(tp,ImGui::GetColorU32(ImGuiCol_Text),text);
        if(ImGui::IsItemFocused()) draw.AddRect(p,max,ImGui::GetColorU32(ImGuiCol_NavCursor),4,0,1);
    }
    ImGui::PopID(); return changed;
}
} // namespace
float Scale() { return gScale; }
SectionScope::SectionScope(int section):previous_(gSection) { gSection=section; }
SectionScope::~SectionScope() { gSection=previous_; }
const char* ThemeName(int) { return "Graphite / Sage"; }
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
    style::WorkspaceStyle scoped; Actions actions;
    auto& motionState=state.motion;
    motionState.dt=std::max(0.f,ImGui::GetIO().DeltaTime); motionState.enabled=MotionAllowed(state);
    motionState.pageAge+=motionState.dt; motionState.detailAge+=motionState.dt;
    MotionScope motionOwner(motionState);
    const float visibility=motionState.window.Step(motionState.visible?1.f:0.f,motionState.dt,motionState.enabled,motion::Transition,1.f);
    style::StyleScope entrance;
    entrance.Var(ImGuiStyleVar_Alpha,ImGui::GetStyle().Alpha*visibility);
    const auto* viewport=ImGui::GetMainViewport(); const auto work=viewport->WorkSize;
    if(work.x<180||work.y<180) return actions;
    const float preferredWidth=work.x>=1440?860.f:740.f;
    const ImVec2 size{std::floor(std::min(preferredWidth,work.x-24)),std::floor(std::min(610.f,work.y-24))};
    const bool compact=size.x<620||size.y<460;
    const float reserve=std::min(496.f,std::max(0.f,work.x-size.x-24));
    const ImVec2 homePosition{(work.x-size.x-reserve)*.5f,(work.y-size.y)*.5f};
    if(!state.positioned) {
        // Reserve space for the independent settings pane on desktop viewports.
        state.position=homePosition; state.positioned=true;
    }
    if(state.dragging) {
        const auto m=ImGui::GetIO().MousePos;
        state.position={m.x-viewport->WorkPos.x-state.dragOffset.x,m.y-viewport->WorkPos.y-state.dragOffset.y};
        if(!ImGui::IsMouseDown(0)) state.dragging=false;
    }
    state.position={std::clamp(state.position.x,0.f,std::max(0.f,work.x-size.x)),std::clamp(state.position.y,0.f,std::max(0.f,work.y-size.y))};
    const ImVec2 pos{std::floor(viewport->WorkPos.x+state.position.x),std::floor(viewport->WorkPos.y+state.position.y)};
    constexpr auto flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|
        ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoScrollWithMouse;
    const auto inputFlags=motionState.visible?ImGuiWindowFlags_None:ImGuiWindowFlags_NoInputs;
    bool openSettings=false;
    // On compact viewports, avoid compositing two sets of UI text through each
    // other. Closing details returns to the grid on the next frame.
    const bool compactDetails=state.settingsOpen&&state.page==state.renderedPage&&work.x<1260;
    if(!compactDetails) {
    ImGui::SetNextWindowPos(pos); ImGui::SetNextWindowSize(size);
    if(!ImGui::Begin("Keim control panel",nullptr,flags|inputFlags)) { ImGui::End(); return actions; }
    auto& draw=*ImGui::GetWindowDrawList();
    const float sidebar=compact?112.f:166.f;
    draw.AddRectFilled(pos,{pos.x+size.x,pos.y+size.y},ImGui::GetColorU32(ImGuiCol_WindowBg),12);
    draw.AddRectFilled(pos,{pos.x+sidebar,pos.y+size.y},ImGui::GetColorU32(style::Color(style::workspace::SidebarRGB)),12,ImDrawFlags_RoundCornersLeft);
    draw.AddRect(pos,{pos.x+size.x-1,pos.y+size.y-1},ImGui::GetColorU32(ImGuiCol_Border),12);
    draw.AddLine({pos.x+sidebar,pos.y+1},{pos.x+sidebar,pos.y+size.y-1},ImGui::GetColorU32(ImGuiCol_Separator));
    ImGui::SetCursorScreenPos({pos.x+8,pos.y+5}); ImGui::InvisibleButton("##drag",{std::max(1.f,size.x-58),32});
    if(ImGui::IsItemActivated()) { state.dragging=true; const auto m=ImGui::GetIO().MousePos; state.dragOffset={m.x-pos.x,m.y-pos.y}; }
    if(ImGui::IsItemHovered()||state.dragging) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    style::DrawBrandLogo(draw,{pos.x+16,pos.y+(compact?10:27)},compact?25.f:30.f,ImGui::GetColorU32(ImVec4(1,1,1,1)));
    draw.AddText(compact?style::MenuFont(true):style::MenuHeadingFont(),compact?14.f:22.f,
        {pos.x+51,pos.y+(compact?16:30)},ImGui::GetColorU32(ImGuiCol_Text),"keim");
    if(!compact) draw.AddText({pos.x+20,pos.y+65},ImGui::GetColorU32(ImGuiCol_TextDisabled),"Minecraft client");
    ImGui::SetCursorScreenPos({pos.x+size.x-34,pos.y+12});
    if(MotionSmallButton("X##close")) actions.close=true;
    const bool focusSearch=!state.settingsOpen&&ImGui::GetIO().KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_F,false);
    ImGui::SetCursorScreenPos(compact?ImVec2(pos.x+14,pos.y+48):ImVec2(pos.x+size.x-232,pos.y+46));
    ImGui::SetNextItemWidth(compact?sidebar-28:210);
    if(focusSearch) ImGui::SetKeyboardFocusHere();
    bool filterChanged;
    {
        style::StyleScope search;
        search.Var(ImGuiStyleVar_FramePadding,compact?ImVec2(26,3):ImVec2(30,8));
        filterChanged=ImGui::InputTextWithHint("##module_search",compact?"Search":"Search modules...",state.search,sizeof(state.search));
        const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();
        const float focused=Animate(ImGui::GetID("##search_focus"),ImGui::IsItemActive()?1.f:0.f,motion::Focus);
        draw.AddRect(lo,hi,ImGui::GetColorU32(style::Color(kMenuAccent,focused)),5,0,1);
        draw.AddRect({lo.x-1,lo.y-1},{hi.x+1,hi.y+1},ImGui::GetColorU32(style::Color(kMenuAccent,focused*.12f)),6,0,3);
        const ImVec2 lens{lo.x+12,(lo.y+hi.y)*.5f-1};
        const auto ink=ImGui::GetColorU32(ImGuiCol_TextDisabled);
        draw.AddCircle(lens,4,ink,16,1.4f);
        draw.AddLine({lens.x+3,lens.y+3},{lens.x+7,lens.y+7},ink,1.4f);
        if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("Search this category  /  Ctrl+F");
    }
    draw.AddText(style::MenuFont(),11,{pos.x+20,pos.y+(compact?85:104)},ImGui::GetColorU32(ImGuiCol_TextDisabled),"WORKSPACE");
    ImGui::SetCursorScreenPos({pos.x+10,pos.y+(compact?106:126)});
    if(ImGui::BeginChild("navigation",{sidebar-20,std::max(1.f,size.y-(compact?181:201))},ImGuiChildFlags_None,ImGuiWindowFlags_NoBackground)) {
        const auto nav=ImGui::GetCursorScreenPos();
        const float rowHeight=compact?29.f:36.f;
        const float marker=Animate(ImGui::GetID("##marker"),float(state.page),motion::Transition,float(state.page));
        const float markerY=nav.y+marker*(rowHeight+ImGui::GetStyle().ItemSpacing.y);
        auto& navDraw=*ImGui::GetWindowDrawList();
        navDraw.AddRectFilled({nav.x,markerY},{nav.x+ImGui::GetContentRegionAvail().x,markerY+rowHeight},ImGui::GetColorU32(style::Color(kMenuAccent,.09f)),5);
        navDraw.AddRectFilled({nav.x,markerY+9},{nav.x+2,markerY+rowHeight-9},ImGui::GetColorU32(ImGuiCol_CheckMark),1);
        for(int i=0;i<IM_ARRAYSIZE(kPages);++i) {
            const auto p=ImGui::GetCursorScreenPos(); const bool active=state.page==static_cast<Page>(i);
            style::StyleScope selected;
            selected.Var(ImGuiStyleVar_ButtonTextAlign,ImVec2(0,.5f)); selected.Var(ImGuiStyleVar_FramePadding,ImVec2(32,4));
            selected.Color(ImGuiCol_Button,ImVec4(0,0,0,0));
            selected.Color(ImGuiCol_Text,active?style::Color(0xD6EBC6):style::Color(0xADB5AF));
            const float height=compact?29.f:36.f;
            if(MotionButton(kPages[i],{-1,height})) state.page=static_cast<Page>(i);

            Icon(*ImGui::GetWindowDrawList(),{p.x+15,p.y+height*.5f},i,ImGui::GetColorU32(active?ImGuiCol_CheckMark:ImGuiCol_TextDisabled));
        }
    }
    ImGui::EndChild();
    const bool pageChanged=state.page!=state.renderedPage;
    if(pageChanged) {
        // Release the text editor as well as its buffer when a category changes.
        const auto searchId=ImGui::GetID("##module_search");
        if(ImGui::GetActiveID()==searchId) ImGui::ClearActiveID();
        if(auto* editor=ImGui::GetInputTextState(searchId)) editor->ReloadUserBufAndMoveToEnd();
        auto& deactivated=ImGui::GetCurrentContext()->InputTextDeactivatedState;
        if(deactivated.ID==searchId) deactivated.ClearFreeMemory();
        state.search[0]='\0'; state.enabledOnly=false; state.settingsOpen=false; ImGui::SetWindowFocus();
    }
    std::array<ModuleView,64> modules{};
    const int count=catalog?std::clamp(catalog(owner,state.page,modules),0,int(modules.size())):0;
    int& selected=state.selectedSections[int(state.page)]; selected=std::clamp(selected,0,std::max(0,count-1));
    int enabled=0;
    for(int i=0;i<count;++i) {
        if(modules[i].mode==TileMode::Toggle&&modules[i].enabled) ++enabled;
    }
    const float contentX=pos.x+sidebar+22;
    if(!compact) draw.AddText(style::MenuFont(),11,{contentX,pos.y+21},ImGui::GetColorU32(ImGuiCol_TextDisabled),"YOUR WORKSPACE");
    draw.PushClipRect({contentX,pos.y+15},{pos.x+size.x-(compact?44:242),pos.y+76},true);
    draw.AddText(compact?style::MenuHeadingFont():style::MenuDisplayFont(),compact?22.f:28.f,
        {contentX,pos.y+(compact?21:42)},ImGui::GetColorU32(ImGuiCol_Text),kPages[int(state.page)]);
    draw.PopClipRect();
    draw.PushClipRect({contentX,pos.y+45},{pos.x+size.x-22,pos.y+100},true);
    draw.AddText({contentX,pos.y+(compact?52:82)},ImGui::GetColorU32(ImGuiCol_TextDisabled),kDescriptions[int(state.page)]);
    draw.PopClipRect();
    const float toolbarY=compact?82.f:116.f;
    ImGui::SetCursorScreenPos({contentX,pos.y+toolbarY});
    if(catalog) {
        if(FilterTab("All",count,!state.enabledOnly,82)) { state.enabledOnly=false; filterChanged=true; }
        ImGui::SameLine(0,6);
        if(FilterTab("Enabled",enabled,state.enabledOnly,110)) { state.enabledOnly=true; filterChanged=true; }
    }
    auto visible=[&](const ModuleView& module) {
        return Matches(module.name,state.search)&&(!state.enabledOnly||(module.mode==TileMode::Toggle&&module.enabled));
    };
    int matching=0; for(int i=0;i<count;++i) if(visible(modules[i])) ++matching;
    if(!compact) {
        char results[32]; std::snprintf(results,sizeof(results),"%d shown",matching);
        draw.AddText({pos.x+size.x-ImGui::CalcTextSize(results).x-22,pos.y+toolbarY+8},ImGui::GetColorU32(ImGuiCol_TextDisabled),results);
    }
    if(pageChanged) motionState.pageAge=motionState.dt;
    const float reveal=motionState.enabled?motion::Cubic(motionState.pageAge/motion::Transition):1.f;
    const float gridY=toolbarY+38;
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,ImGui::GetStyle().Alpha*reveal);
    ImGui::SetCursorScreenPos({pos.x+sidebar+17,pos.y+gridY});
    if(pageChanged||filterChanged) ImGui::SetNextWindowScroll({-1,0});
    if(ImGui::BeginChild("module_grid",{std::max(1.f,size.x-sidebar-34),std::max(1.f,size.y-gridY-58)},ImGuiChildFlags_None,ImGuiWindowFlags_NoBackground)) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY()+motion::Slide*(1-reveal));

        if(catalog&&!matching) {
            ImGui::TextWrapped("%s",state.enabledOnly?"No enabled modules match this view.":"No modules match your search.");
            if(MotionButton(state.enabledOnly?"Show all modules":"Clear search")) { state.search[0]='\0'; state.enabledOnly=false; }
        } else if(catalog) {
            const int columns=std::clamp(int(ImGui::GetContentRegionAvail().x/220.f),1,2);
            style::StyleScope grid;
            grid.Var(ImGuiStyleVar_CellPadding,ImVec2(5,5));
            if(ImGui::BeginTable("module_widgets",columns,ImGuiTableFlags_SizingStretchSame|ImGuiTableFlags_NoSavedSettings)) {
                for(int i=0;i<count;++i) {
                    const auto& module=modules[i]; if(!visible(module)) continue;
                    ImGui::TableNextColumn(); ImGui::PushID(i);
                    const auto p=ImGui::GetCursorScreenPos(); const float width=ImGui::GetContentRegionAvail().x;
                    const bool active=module.mode==TileMode::Toggle&&module.enabled;
                    const float on=Animate(ImGui::GetID("##toggle_state")^(0x9e3779b9u*unsigned(int(state.page)+1)),active?1.f:0.f,motion::Focus,active?1.f:0.f);
                    bool clicked=false,details=false;
                    auto& tile=*ImGui::GetWindowDrawList();
                    tile.AddRectFilled(p,{p.x+width,p.y+66},
                        ImGui::GetColorU32(motion::Mix(style::Color(0xFFFFFF,.025f),style::Color(kMenuAccent,.065f),on)),7);
                    {
                        style::StyleScope widget;
                        widget.Var(ImGuiStyleVar_FrameRounding,7);
                        widget.Color(ImGuiCol_Button,ImVec4(0,0,0,0));
                        widget.Color(ImGuiCol_ButtonHovered,style::Color(0xFFFFFF,.035f));
                        widget.Color(ImGuiCol_ButtonActive,style::Color(kMenuAccent,.16f));
                        // A separate, keyboard-accessible settings target avoids accidental toggles.
                        clicked=MotionButton("##module_widget",{width-34,66});
                        details=ImGui::IsItemClicked(ImGuiMouseButton_Right)||
                            (ImGui::IsItemFocused()&&(ImGui::IsKeyPressed(ImGuiKey_Menu)||
                                (ImGui::GetIO().KeyShift&&ImGui::IsKeyPressed(ImGuiKey_F10))));
                    }
                    const bool hovered=ImGui::IsItemHovered();
                    const bool pressed=ImGui::IsItemActive();
                    tile.AddRect(p,{p.x+width,p.y+66},ImGui::GetColorU32(motion::Mix(style::Color(0xFFFFFF,.055f),style::Color(kMenuAccent,.19f),on)),7);
                    tile.PushClipRect({p.x+12,p.y+10},{p.x+width-40,p.y+31},true);
                    tile.AddText(style::MenuFont(true),14,{p.x+12,p.y+12},ImGui::GetColorU32(ImGuiCol_Text),module.name);
                    tile.PopClipRect();
                    const char* label=module.mode==TileMode::Toggle?(active?"Enabled":"Disabled"):
                        module.mode==TileMode::Action?"Run action":"Open settings";
                    const ImU32 labelInk=active?ImGui::GetColorU32(style::Color(kMenuAccent)):
                        ImGui::GetColorU32(pressed?ImGuiCol_Text:ImGuiCol_TextDisabled);
                    tile.AddText({p.x+12,p.y+39},labelInk,label);
                    const char* binding=std::strncmp(module.binding,"Key: ",5)==0?module.binding+5:module.binding;
                    if(binding[0]&&std::strcmp(binding,"None")!=0) {
                        const float badgeWidth=ImGui::CalcTextSize(binding).x+12;
                        const float badgeX=p.x+width-82-badgeWidth;
                        if(badgeX>p.x+12+ImGui::CalcTextSize(label).x+12) {
                            tile.AddRectFilled({badgeX,p.y+36},{badgeX+badgeWidth,p.y+56},ImGui::GetColorU32(style::Color(0xFFFFFF,.04f)),4);
                            tile.AddText({badgeX+6,p.y+39},ImGui::GetColorU32(ImGuiCol_TextDisabled),binding);
                        }
                    }
                    if(module.mode==TileMode::Toggle) {
                        const ImVec2 a{p.x+width-72,p.y+39},b{p.x+width-46,p.y+53};
                        tile.AddRectFilled(a,b,ImGui::GetColorU32(motion::Mix(ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive),ImGui::GetStyleColorVec4(ImGuiCol_CheckMark),on)),7);
                        tile.AddCircleFilled({a.x+7+(b.x-a.x-14)*on,a.y+7},4,ImGui::GetColorU32(motion::Mix(style::Color(0xAEAEB8),style::Color(0x25252C),on)),12);
                    }
                    if(hovered&&ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                        ImGui::BeginTooltip(); ImGui::TextUnformatted(module.name);
                        if(module.binding[0]) ImGui::TextDisabled("%s",module.binding);
                        ImGui::TextDisabled("%s",module.mode==TileMode::Toggle?"Click to toggle. Right-click for settings.":
                            module.mode==TileMode::Action?"Click to run. Right-click for settings.":"Click to open settings.");
                        ImGui::EndTooltip();
                    }
                    ImGui::SetCursorScreenPos({p.x+width-32,p.y+2});
                    {
                        style::StyleScope settingsButton;
                        settingsButton.Color(ImGuiCol_Button,ImVec4(0,0,0,0));
                        if(MotionButton("##settings",{30,62})) details=true;
                        const ImVec2 center{p.x+width-17,p.y+33};
                        auto ink=ImGui::GetColorU32(ImGui::IsItemHovered()?ImGuiCol_Text:ImGuiCol_TextDisabled);
                        for(int line=0;line<3;++line) {
                            const float y=center.y-5+line*5;
                            tile.AddLine({center.x-5,y},{center.x+5,y},ink,1);
                            tile.AddCircleFilled({center.x+(line==1?2.f:-2.f),y},1.7f,ink,8);
                        }
                        if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("Open %s settings",module.name);
                    }
                    ImGui::SetCursorScreenPos({p.x,p.y+66});
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
    ImGui::EndChild(); ImGui::PopStyleVar();
    draw.AddLine({pos.x+14,pos.y+size.y-57},{pos.x+sidebar-14,pos.y+size.y-57},ImGui::GetColorU32(ImGuiCol_Separator));
    draw.AddText(style::MenuFont(),11,{pos.x+18,pos.y+size.y-46},ImGui::GetColorU32(ImGuiCol_TextDisabled),"MINECRAFT JAVA");
    draw.AddCircleFilled({pos.x+22,pos.y+size.y-23},2.5f,ImGui::GetColorU32(status.connected?ImGui::ColorConvertU32ToFloat4(style::compact::Positive):ImGui::ColorConvertU32ToFloat4(style::compact::Warning)),12);
    ImGui::SetCursorScreenPos({pos.x+32,pos.y+size.y-30}); ImGui::TextDisabled("%s",status.connected?"Connected":"Offline");
    draw.AddLine({contentX,pos.y+size.y-47},{pos.x+size.x-22,pos.y+size.y-47},ImGui::GetColorU32(ImGuiCol_Separator));
    ImGui::SetCursorScreenPos({pos.x+sidebar+22,pos.y+size.y-30});
    ImGui::TextDisabled("%s",status.memory[0]?status.memory:compact?status.version:"Right-click to configure");
    ImGui::SameLine(std::max(sidebar+80,size.x-170));
    if(MotionSmallButton("Center UI")) { state.position=homePosition; state.dragging=false; state.settingsPlacement.positioned=false; state.settingsPlacement.dragging=false; }
    ImGui::SameLine(0,9); if(MotionSmallButton("Close")) actions.close=true;
    ImGui::End();
    if(actions.close) state.settingsOpen=false;
    state.renderedPage=state.page; state.pageAlpha=1; state.animateNextPage=false;
    }

    if(state.settingsOpen&&catalog&&content) {
        std::array<ModuleView,64> details{};
        const int detailCount=std::clamp(catalog(owner,state.settingsPage,details),0,int(details.size()));
        if(state.settingsSection<0||state.settingsSection>=detailCount) state.settingsOpen=false;
        else {
            const auto& module=details[state.settingsSection];
            if(!state.settingsWasOpen||state.settingsPage!=state.renderedSettingsPage||state.settingsSection!=state.renderedSettingsSection) motionState.detailAge=motionState.dt;
            const float detailReveal=motionState.enabled?motion::Cubic(motionState.detailAge/motion::Transition):1.f;
            style::StyleScope detailMotion;
            // Keep the pane opaque where it overlaps the menu; fade its contents.
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
            ImGui::SetNextWindowScroll({0,0}); // Only the content child scrolls; keep the header fixed.
            if(openSettings) ImGui::SetNextWindowFocus();
            constexpr auto detailFlags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoScrollWithMouse;
            if(ImGui::Begin("Keim module settings",nullptr,detailFlags|inputFlags)) {
                ImGui::SetCursorScreenPos({detailPos.x+8,detailPos.y+5});
                ImGui::InvisibleButton("##drag",{std::max(1.f,detailSize.x-154),32});
                if(ImGui::IsItemActivated()) { placement.dragging=true; const auto m=ImGui::GetIO().MousePos; placement.offset={m.x-detailPos.x,m.y-detailPos.y}; }
                if(ImGui::IsItemHovered()||placement.dragging) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
                auto& settingsDraw=*ImGui::GetWindowDrawList();
                settingsDraw.AddRect(detailPos,{detailPos.x+detailSize.x-1,detailPos.y+detailSize.y-1},ImGui::GetColorU32(ImGuiCol_Border),12);
                settingsDraw.PushClipRect({detailPos.x+16,detailPos.y+12},{detailPos.x+detailSize.x-154,detailPos.y+34},true);
                settingsDraw.AddText(style::MenuFont(true),14,{detailPos.x+16,detailPos.y+16},ImGui::GetColorU32(ImGuiCol_Text),module.name);
                settingsDraw.PopClipRect();
                ImGui::SetCursorScreenPos({detailPos.x+detailSize.x-141,detailPos.y+12});
                if(module.mode==TileMode::Toggle&&activate) {
                    bool enabled=module.enabled;
                    if(MotionCheckbox("Enabled##module_enabled",&enabled)) activate(owner,state.settingsPage,state.settingsSection);
                } else if(module.mode==TileMode::Action&&activate&&MotionSmallButton("Run action"))
                    activate(owner,state.settingsPage,state.settingsSection);
                ImGui::SetCursorScreenPos({detailPos.x+detailSize.x-34,detailPos.y+12});
                if(MotionSmallButton("X##details_close")) state.settingsOpen=false;
                settingsDraw.AddLine({detailPos.x+16,detailPos.y+42},{detailPos.x+detailSize.x-16,detailPos.y+42},ImGui::GetColorU32(ImGuiCol_Separator));
                const bool changed=state.settingsPage!=state.renderedSettingsPage||state.settingsSection!=state.renderedSettingsSection;
                ImGui::SetCursorScreenPos({detailPos.x+24,detailPos.y+52});
                if(changed) ImGui::SetNextWindowScroll({-1,0});
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha,ImGui::GetStyle().Alpha*detailReveal);
                if(ImGui::BeginChild("content",{std::max(1.f,detailSize.x-48),std::max(1.f,detailSize.y-68)},ImGuiChildFlags_None,ImGuiWindowFlags_NoBackground)) {
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY()+motion::Slide*(1-detailReveal));
                    SectionScope section(state.settingsSection); content(owner,state.settingsPage);
                }
                ImGui::EndChild(); ImGui::PopStyleVar();
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
    changed=MotionCheckbox("##value",value)||changed;
    ImGui::PopID(); return changed;
}
bool ComboRow(const char* label,int* value,const char* const* items,int count) {
    ImGui::PushID(label); RowLabel(label); const bool changed=ImGui::Combo("##value",value,items,count); ImGui::PopID(); return changed;
}
bool SliderRow(const char* label,float* value,float min,float max,const char* format,bool zeroIsOff) { return Slider(label,value,min,max,format,ImGuiDataType_Float,zeroIsOff); }
bool SliderDoubleRow(const char* label,double* value,double min,double max,const char* format) { return Slider(label,value,min,max,format,ImGuiDataType_Double); }
bool SliderIntRow(const char* label,int* value,int min,int max,const char* format,bool zeroIsOff) { return Slider(label,value,min,max,format,ImGuiDataType_S32,zeroIsOff); }
bool ColorRow(const char* label,float* value) {
    ImGui::PushID(label); RowLabel(label);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX()+std::max(0.f,ImGui::GetContentRegionAvail().x-ImGui::GetFrameHeight()));
    const bool changed=ImGui::ColorEdit4("##value",value,ImGuiColorEditFlags_NoInputs|ImGuiColorEditFlags_NoLabel);
    ImGui::PopID(); return changed;
}
void ValueRow(const char* label,const char* value) { RowLabel(label); ImGui::TextWrapped("%s",value); }
} // namespace mcov::control_panel

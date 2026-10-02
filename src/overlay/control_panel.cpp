#include "control_panel.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <windows.h>
#include "menu_motion.hpp"
#include "imgui_internal.h"
#include "../common/flat_style.hpp"

namespace mcov::control_panel {
namespace {
constexpr const char* kThemes[] = {"Reference amber", "Graphite", "Light", "Crimson"};
float gScale = 1.25f;
bool gMotion = true;
float ControlHeight() { return std::max(20*gScale,ImGui::GetFontSize()+8*gScale); }
float RowHeight() { return std::max(28*gScale,ControlHeight()+8*gScale); }
bool PointerHover() {
    return ImGui::GetIO().MouseSource == ImGuiMouseSource_Mouse && ImGui::IsItemHovered();
}
bool WindowsMotionAllowed() {
    static double checked=-1;
    static bool enabled=true;
    const double now=ImGui::GetTime();
    if (checked<0 || now<checked || now-checked>=1) {
        BOOL value=TRUE;
        if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&value,0)) enabled=value!=FALSE;
        checked=now;
    }
    return enabled;
}
float Animate(const char* token,float target,float seconds=motion::kHoverSeconds,bool allow=true) {
    const ImGuiID id=ImGui::GetID(token);
    const auto key=[&](const char* suffix) { return ImHashStr(suffix,0,id); };
    auto* storage=ImGui::GetStateStorage();
    const int frame=ImGui::GetFrameCount(), previousFrame=storage->GetInt(key("frame"),-1);
    float value=storage->GetFloat(id,target);
    float from=storage->GetFloat(key("from"),value);
    float elapsed=storage->GetFloat(key("time"),seconds);
    const float previousTarget=storage->GetFloat(key("target"),target);
    if (!gMotion || !allow || previousFrame<0 || frame-previousFrame>1) {
        value=from=target; elapsed=seconds;
    } else {
        if (previousTarget!=target) { from=value; elapsed=0; }
        if (previousFrame!=frame) elapsed=std::min(seconds,elapsed+ImGui::GetIO().DeltaTime);
        value=from+(target-from)*motion::EaseOut(elapsed/seconds);
    }
    storage->SetFloat(id,value); storage->SetFloat(key("from"),from);
    storage->SetFloat(key("time"),elapsed); storage->SetFloat(key("target"),target);
    storage->SetInt(key("frame"),frame);
    return value;
}
void SelectPage(State& state,Page page) {
    if (state.page==page) return;
    state.page=page;
    state.animateNextPage=ImGui::IsMouseReleased(ImGuiMouseButton_Left) && gMotion;
}
ImVec4 Color(unsigned int rgb) {
    return ImVec4(float((rgb >> 16) & 255)/255.f, float((rgb >> 8) & 255)/255.f, float(rgb & 255)/255.f, 1);
}
ImVec4 Mix(ImVec4 a, ImVec4 b, float t) {
    return ImVec4(a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t, a.z+(b.z-a.z)*t, a.w+(b.w-a.w)*t);
}
void ApplyTheme(ImGuiStyle& s, int index) {
    const bool light=index==2, graphite=index==1;
    const ImVec4 bg=Color(light ? 0xE9E7E1 : graphite ? 0x222427 : 0x22201E);
    const ImVec4 panel=Color(light ? 0xF3F1EB : graphite ? 0x1D2024 : 0x1C1C19);
    const ImVec4 frame=Color(light ? 0xE3DFD6 : graphite ? 0x272C31 : 0x242420);
    const ImVec4 text=Color(light ? 0x34322C : 0xD6D5CC);
    const ImVec4 muted=Color(light ? 0x746F65 : 0xA5A299);
    const ImVec4 accent=Color(index==3 ? 0xE1A09A : graphite ? 0xBDD3DF : light ? 0xAD813A : 0xF0D596);
    const ImVec4 border=Color(light ? 0xC6C0B4 : graphite ? 0x41454B : 0x44413A);
    s.WindowPadding=ImVec2(0,0);
    s.WindowRounding=10*gScale; s.ChildRounding=3*gScale;
    s.FrameRounding=3*gScale; s.PopupRounding=4*gScale;
    s.FramePadding=ImVec2(5*gScale,4*gScale);
    s.ItemSpacing=ImVec2(8*gScale,0);
    s.ItemInnerSpacing=ImVec2(4*gScale,3*gScale);
    s.CellPadding=ImVec2(5*gScale,4*gScale);
    s.ScrollbarSize=5*gScale; s.ScrollbarRounding=2*gScale;
    s.GrabMinSize=4*gScale; s.GrabRounding=0;
    s.WindowBorderSize=0; s.ChildBorderSize=1; s.FrameBorderSize=1;
    auto* c=s.Colors;
    for (int i=0;i<ImGuiCol_COUNT;++i) c[i]=panel;
    c[ImGuiCol_Text]=text; c[ImGuiCol_TextDisabled]=muted;
    c[ImGuiCol_WindowBg]=bg; c[ImGuiCol_ChildBg]=ImVec4(0,0,0,0);
    c[ImGuiCol_PopupBg]=panel; c[ImGuiCol_Border]=border;
    c[ImGuiCol_BorderShadow]=ImVec4(0,0,0,0);
    c[ImGuiCol_FrameBg]=frame; c[ImGuiCol_FrameBgHovered]=Mix(frame,accent,.10f);
    c[ImGuiCol_FrameBgActive]=Mix(frame,accent,.18f);
    c[ImGuiCol_Button]=frame; c[ImGuiCol_ButtonHovered]=Mix(frame,accent,.12f);
    c[ImGuiCol_ButtonActive]=Mix(frame,accent,.25f);
    c[ImGuiCol_CheckMark]=c[ImGuiCol_SliderGrab]=accent;
    c[ImGuiCol_SliderGrabActive]=text;
    c[ImGuiCol_Header]=Mix(frame,accent,.15f); c[ImGuiCol_HeaderHovered]=Mix(frame,accent,.22f);
    c[ImGuiCol_HeaderActive]=Mix(frame,accent,.3f);
    c[ImGuiCol_Separator]=border;
    c[ImGuiCol_TableBorderLight]=Mix(panel,border,.13f);
    c[ImGuiCol_TableBorderStrong]=border;
    c[ImGuiCol_TableRowBg]=c[ImGuiCol_TableRowBgAlt]=panel;
    c[ImGuiCol_ScrollbarBg]=bg; c[ImGuiCol_ScrollbarGrab]=border;
    c[ImGuiCol_ScrollbarGrabHovered]=muted; c[ImGuiCol_ScrollbarGrabActive]=accent;
    c[ImGuiCol_TextSelectedBg]=Mix(panel,accent,.25f);
    c[ImGuiCol_PlotLines]=c[ImGuiCol_PlotHistogram]=accent;
    c[ImGuiCol_TextLink]=c[ImGuiCol_NavCursor]=c[ImGuiCol_DragDropTarget]=accent;
    c[ImGuiCol_NavWindowingHighlight]=accent;
    c[ImGuiCol_NavWindowingDimBg]=c[ImGuiCol_ModalWindowDimBg]=ImVec4(0,0,0,.55f);
}
struct MenuStyle {
    ImGuiStyle saved=ImGui::GetStyle();
    float previous=gScale;
    bool previousMotion=gMotion;
    MenuStyle(int theme,float scale,bool animate) {
        gScale=scale; gMotion=animate; ApplyTheme(ImGui::GetStyle(),theme); ImGui::PushFont(style::MenuFont());
    }
    ~MenuStyle() { ImGui::PopFont(); ImGui::GetStyle()=saved; gScale=previous; gMotion=previousMotion; }
};
void FocusOutline(ImVec2 p,ImVec2 size) {
    if (ImGui::IsItemFocused()) ImGui::GetWindowDrawList()->AddRect(p,ImVec2(p.x+size.x,p.y+size.y),
        ImGui::GetColorU32(ImGuiCol_NavCursor),2*gScale,0,gScale);
}
void Icon(ImDrawList& draw,const char* glyph,ImVec2 center,float size,ImU32 color) {
    ImFont* font=style::MenuIconFont();
    const ImVec2 extent=font->CalcTextSizeA(size,FLT_MAX,0,glyph);
    draw.AddText(font,size,ImVec2(center.x-extent.x*.5f,center.y-extent.y*.5f),color,glyph);
}
// Font Awesome Solid glyphs, from the project's existing licensed font asset.
constexpr const char* kSkull="\xef\x95\x8c";
constexpr const char* kEye="\xef\x81\xae";
constexpr const char* kCube="\xef\x86\xb2";
constexpr const char* kLayers="\xef\x97\xbd";
constexpr const char* kUser="\xef\x80\x87";
constexpr const char* kSun="\xef\x86\x85";
constexpr const char* kGear="\xef\x80\x93";
void NavButton(State& state,const char* glyph,const char* label,Page page,float width) {
    const ImVec2 p=ImGui::GetCursorScreenPos(), extent(width,57*gScale);
    ImGui::PushID(int(page));
    const bool clicked=ImGui::InvisibleButton("##nav",extent,ImGuiButtonFlags_EnableNav);
    if (clicked) SelectPage(state,page);
    const bool selected=state.page==page;
    const float hover=Animate("hover",PointerHover() ? 1.f : 0.f);
    const float active=Animate("selected",selected ? 1.f : 0.f,motion::kPageSeconds,
                               !clicked || ImGui::IsMouseReleased(ImGuiMouseButton_Left));
    const float press=Animate("press",ImGui::IsItemActive() ? 1.f : 0.f,motion::kFeedbackSeconds);
    const float shrink=gMotion ? 1-.03f*press : 1.f;
    const ImVec2 a(p.x+extent.x*(1-shrink)*.5f,p.y+extent.y*(1-shrink)*.5f);
    const ImVec2 b(p.x+extent.x*(1+shrink)*.5f,p.y+extent.y*(1+shrink)*.5f);
    auto& draw=*ImGui::GetWindowDrawList();
    const auto accent=ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
    const auto selection=state.theme==0 ? Color(0x876B57) : Mix(ImGui::GetStyleColorVec4(ImGuiCol_WindowBg),accent,.22f);
    const auto fill=Mix(Mix(ImGui::GetStyleColorVec4(ImGuiCol_WindowBg),selection,active),accent,hover*.06f);
    draw.AddRectFilled(a,b,ImGui::GetColorU32(fill),4*gScale);
    draw.AddRect(a,b,ImGui::GetColorU32(Mix(ImGui::GetStyleColorVec4(ImGuiCol_Border),accent,active*.35f)),4*gScale,0,1.f);
    const ImU32 ink=ImGui::GetColorU32(Mix(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled),accent,active));
    Icon(draw,glyph,ImVec2(p.x+width*.5f,p.y+(state.navCollapsed ? 28.5f : 20.f)*gScale),
         std::min(32.f,width*.46f)*shrink,ink);
    if (!state.navCollapsed) {
        ImFont* font=style::MenuFont(true);
        const float fs=std::min(12.f,width/5.2f)*shrink;
        const ImVec2 ts=font->CalcTextSizeA(fs,FLT_MAX,0,label);
        draw.AddText(font,fs,ImVec2(p.x+(width-ts.x)*.5f,p.y+extent.y-15*gScale),
            ImGui::GetColorU32(Mix(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled),ImGui::GetStyleColorVec4(ImGuiCol_Text),active)),label);
    }
    FocusOutline(p,extent);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        const char* hint=page==Page::Combat ? "Aim lock and triggerbot" : page==Page::Blocks ? "Block ESP and loot filters" :
                         page==Page::Visuals ? "Entity rendering" : "HUD and equipment";
        ImGui::SetTooltip("%s",hint);
    }
    ImGui::PopID(); ImGui::Dummy(ImVec2(0,7*gScale));
}
bool ToolButton(const char* id,const char* glyph,const char* tooltip,bool selected) {
    const ImVec2 p=ImGui::GetCursorScreenPos(), extent(34*gScale,34*gScale);
    const bool pressed=ImGui::InvisibleButton(id,extent,ImGuiButtonFlags_EnableNav);
    ImGui::PushID(id);
    const float hover=Animate("hover",PointerHover() ? 1.f : 0.f);
    const float active=Animate("selected",selected ? 1.f : 0.f);
    const float press=Animate("press",ImGui::IsItemActive() ? 1.f : 0.f,motion::kFeedbackSeconds);
    const float shrink=gMotion ? 1-.03f*press : 1.f;
    const ImVec2 a(p.x+extent.x*(1-shrink)*.5f,p.y+extent.y*(1-shrink)*.5f);
    const ImVec2 b(p.x+extent.x*(1+shrink)*.5f,p.y+extent.y*(1+shrink)*.5f);
    auto& draw=*ImGui::GetWindowDrawList();
    const auto accent=ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
    const auto fill=Mix(ImGui::GetStyleColorVec4(ImGuiCol_WindowBg),accent,.12f+.10f*active+.06f*hover);
    draw.AddRectFilled(a,b,ImGui::GetColorU32(fill),3*gScale);
    Icon(draw,glyph,ImVec2(p.x+extent.x*.5f,p.y+extent.y*.5f),18*gScale*shrink,ImGui::GetColorU32(ImGuiCol_CheckMark));
    FocusOutline(p,extent);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) ImGui::SetTooltip("%s",tooltip);
    ImGui::PopID(); ImGui::Dummy(ImVec2(0,5*gScale));
    return pressed;
}
void Orbits(ImDrawList& draw,ImVec2 p,ImVec2 extent) {
    const ImU32 ink=IM_COL32(156,151,133,13);
    draw.PushClipRect(p,ImVec2(p.x+extent.x,p.y+extent.y),true);
    const ImVec2 left(p.x-12*gScale,p.y+extent.y+4*gScale);
    for (float radius:{77.f,138.f,194.f}) {
        draw.PathArcTo(left,radius*gScale,-1.69f,.02f,64); draw.PathStroke(ink,0,gScale);
    }
    const ImVec2 right(p.x+extent.x+34*gScale,p.y-12*gScale);
    for (float radius:{84.f,139.f,197.f}) {
        draw.PathArcTo(right,radius*gScale,.72f,3.16f,64); draw.PathStroke(ink,0,gScale);
    }
    draw.PopClipRect();
}
void RowLabel(const char* label,bool bright=true) {
    ImGui::TableNextRow(0,RowHeight());
    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,ImGui::GetColorU32(ImGuiCol_TableRowBg));
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text,ImGui::GetStyleColorVec4(bright ? ImGuiCol_Text : ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s",label); ImGui::PopStyleColor();
    ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-1);
}
template<class T> bool Slider(const char* label,T* value,T low,T high,const char* format,ImGuiDataType type) {
    ImGui::PushID(label); RowLabel(label,false);
    const ImVec2 p=ImGui::GetCursorScreenPos();
    const float width=std::max(16.f,ImGui::GetContentRegionAvail().x), h=ControlHeight();
    const float number=std::min(width*.48f,48*gScale);
    ImGui::SetCursorScreenPos(ImVec2(p.x+number,p.y));
    const float trackWidth=std::max(4.f,width-number);
    bool changed;
    {
        // Use ImGui's slider behavior for mouse, keyboard, disabled state and
        // Ctrl-click numeric entry; only its presentation is replaced.
        style::StyleScope invisible;
        if (!ImGui::TempInputIsActive(ImGui::GetID("##value")))
            for (auto color:{ImGuiCol_FrameBg,ImGuiCol_FrameBgHovered,ImGuiCol_FrameBgActive,
                            ImGuiCol_SliderGrab,ImGuiCol_SliderGrabActive,ImGuiCol_Text})
                invisible.Color(color,ImVec4(0,0,0,0));
        invisible.Var(ImGuiStyleVar_FrameBorderSize,0.f);
        invisible.Var(ImGuiStyleVar_FramePadding,ImVec2(0,(h-ImGui::GetFontSize())*.5f));
        ImGui::SetNextItemWidth(trackWidth);
        changed=ImGui::SliderScalar("##value",type,value,&low,&high,format,ImGuiSliderFlags_NoRoundToFormat);
    }
    if (ImGui::TempInputIsActive(ImGui::GetID("##value"))) {
        *value=std::clamp(*value,low,high);
        ImGui::PopID(); return changed;
    }
    const float hover=Animate("hover",PointerHover() || ImGui::IsItemActive() ? 1.f : 0.f);
    const bool input=ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0);
    if (input) ImGui::OpenPopup("numeric entry");
    if (ImGui::BeginPopup("numeric entry")) {
        ImGui::TextUnformatted(label); ImGui::SetNextItemWidth(125*gScale);
        if (ImGui::InputScalar("##number",type,value,nullptr,nullptr,format)) {
            *value=std::clamp(*value,low,high); changed=true;
        }
        ImGui::EndPopup();
    }
    auto& draw=*ImGui::GetWindowDrawList();
    const float cy=p.y+h*.5f;
    const float fraction=std::clamp(float(*value-low)/float(high-low),0.f,1.f);
    const float start=p.x+number+2*gScale, end=p.x+width-2*gScale, knob=start+(end-start)*fraction;
    draw.AddLine(ImVec2(start,cy),ImVec2(end,cy),ImGui::GetColorU32(ImGuiCol_FrameBgActive),3*gScale);
    draw.AddLine(ImVec2(start,cy),ImVec2(knob,cy),ImGui::GetColorU32(ImGuiCol_CheckMark),3*gScale);
    draw.AddCircleFilled(ImVec2(knob,cy),(3.f+(gMotion ? .6f*hover : 0.f))*gScale,ImGui::GetColorU32(ImGuiCol_Text),16);
    char text[64]; if (type==ImGuiDataType_S32) std::snprintf(text,sizeof(text),format,int(*value));
    else std::snprintf(text,sizeof(text),format,double(*value));
    const auto ts=ImGui::CalcTextSize(text);
    draw.AddText(ImVec2(p.x+std::max(0.f,number-ts.x-5*gScale),cy-ts.y*.5f),ImGui::GetColorU32(ImGuiCol_Text),text);
    FocusOutline(ImVec2(p.x+number,p.y),ImVec2(trackWidth,h));
    // Restore row extent even when the numeric popup is open.
    ImGui::SetCursorScreenPos(ImVec2(p.x,p.y+h));
    ImGui::PopID(); return changed;
}
} // namespace

float Scale() { return gScale; }
const char* ThemeName(int index) { return kThemes[std::clamp(index,0,3)]; }
void Section(const char* label) {
    ImGui::Dummy(ImVec2(0,7*gScale));
    if (!label || !*label) return;
    ImGui::PushStyleColor(ImGuiCol_Text,ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX()+5*gScale);
    ImGui::TextUnformatted(label); ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0,3*gScale));
}
Actions Render(State& state,const Status& status,void* owner,Content content) {
    state.theme=std::clamp(state.theme,0,3);
    const auto* viewport=ImGui::GetMainViewport(); const auto work=viewport->WorkSize;
    const bool narrow=work.x<640;
    const float scale=narrow ? 1.f : std::min({1.25f,work.x/840.f,work.y/490.f});
    MenuStyle theme(state.theme,std::max(.5f,scale),!state.reduceMotion && WindowsMotionAllowed());
    Actions actions;
    const float navWidth=(state.navCollapsed ? 48.f : 80.f)*gScale, gap=18*gScale;
    const float width=std::floor(std::min(work.x,narrow ? std::max(180.f,work.x-24.f) : (590*gScale+gap+navWidth)));
    const float height=std::floor(std::min(work.y,narrow ? std::max(160.f,work.y-24.f) : 382*gScale));
    if (!state.positioned) {
        state.position=ImVec2((work.x-width)*.5f,(work.y-height)*.5f); state.positioned=true;
    }
    if (state.dragging) {
        const auto mouse=ImGui::GetIO().MousePos;
        state.position=ImVec2(mouse.x-viewport->WorkPos.x-state.dragOffset.x,
                              mouse.y-viewport->WorkPos.y-state.dragOffset.y);
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) state.dragging=false;
    }
    // Retain placement across reopening and viewport changes; keep both panels reachable.
    state.position.x=std::clamp(state.position.x,0.f,std::max(0.f,work.x-width));
    state.position.y=std::clamp(state.position.y,0.f,std::max(0.f,work.y-height));
    const ImVec2 pos(std::floor(viewport->WorkPos.x+state.position.x),std::floor(viewport->WorkPos.y+state.position.y));
    ImGui::SetNextWindowPos(pos,ImGuiCond_Always); ImGui::SetNextWindowSize(ImVec2(width,height),ImGuiCond_Always);
    constexpr auto flags=ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove;
    if (!ImGui::Begin("mcov control panel",nullptr,flags)) { ImGui::End(); return actions; }
    ImGui::SetWindowFontScale(1.f);
    const float mainWidth=width-navWidth-gap;
    const ImVec2 mainEnd(pos.x+mainWidth,pos.y+height), navPos(mainEnd.x+gap,pos.y);
    auto& draw=*ImGui::GetWindowDrawList();
    const auto bg=ImGui::GetColorU32(ImGuiCol_WindowBg);
    // Separate floating panels, measured from the 752 x 427 reference.
    style::DrawPanelShadow(*ImGui::GetBackgroundDrawList(),pos,mainEnd,10*gScale);
    style::DrawPanelShadow(*ImGui::GetBackgroundDrawList(),navPos,ImVec2(pos.x+width,pos.y+height),10*gScale);
    draw.AddRectFilled(pos,mainEnd,bg,10*gScale);
    draw.AddRect(pos,mainEnd,ImGui::GetColorU32(ImGuiCol_Border,.38f),10*gScale,0,gScale);
    draw.AddRectFilled(navPos,ImVec2(pos.x+width,pos.y+height),bg,10*gScale);
    // Dedicated, empty top strip: a drag cannot toggle a module or move a slider.
    ImGui::SetCursorScreenPos(pos);
    ImGui::InvisibleButton("##drag",ImVec2(mainWidth,18*gScale));
    if (ImGui::IsItemActivated() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        state.dragging=true;
        const auto mouse=ImGui::GetIO().MousePos;
        state.dragOffset=ImVec2(mouse.x-pos.x,mouse.y-pos.y);
    }
    if (ImGui::IsItemHovered() || state.dragging) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) ImGui::SetTooltip("Drag to move the menu");
    const float grip=Animate("drag feedback",PointerHover() || state.dragging ? 1.f : 0.f);
    draw.AddRectFilled(ImVec2(pos.x+mainWidth*.5f-16*gScale,pos.y+7*gScale),
                       ImVec2(pos.x+mainWidth*.5f+16*gScale,pos.y+9*gScale),
                       ImGui::GetColorU32(Mix(ImGui::GetStyleColorVec4(ImGuiCol_Border),
                                              ImGui::GetStyleColorVec4(ImGuiCol_CheckMark),grip*.6f)),2*gScale);
    const ImVec2 inner(pos.x+14*gScale,pos.y+14*gScale), innerEnd(mainEnd.x-15*gScale,mainEnd.y-14*gScale);
    draw.AddRect(inner,innerEnd,ImGui::GetColorU32(ImGuiCol_Border),4*gScale,0,gScale);
    const float toolX=mainEnd.x-56*gScale;
    draw.AddLine(ImVec2(toolX-8*gScale,inner.y),ImVec2(toolX-8*gScale,innerEnd.y),ImGui::GetColorU32(ImGuiCol_Border,.7f),gScale);
    Orbits(draw,pos,ImVec2(mainWidth,height));
    ImGui::SetCursorScreenPos(ImVec2(inner.x+8*gScale,inner.y+8*gScale));
    if (state.renderedPage!=state.page) {
        state.pageStartAlpha=state.animateNextPage && gMotion ? std::min(state.pageAlpha,.7f) : 1.f;
        state.pageElapsed=state.animateNextPage && gMotion ? 0.f : motion::kPageSeconds;
        state.renderedPage=state.page; state.animateNextPage=false;
    }
    state.pageElapsed=std::min(motion::kPageSeconds,state.pageElapsed+ImGui::GetIO().DeltaTime);
    state.pageAlpha=gMotion ? state.pageStartAlpha+(1-state.pageStartAlpha)*motion::EaseOut(state.pageElapsed/motion::kPageSeconds) : 1.f;
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,ImGui::GetStyle().Alpha*state.pageAlpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    ImGui::BeginChild("content",ImVec2(std::max(10.f,toolX-24*gScale-inner.x),height-44*gScale),ImGuiChildFlags_None);
    ImGui::SetWindowFontScale(1.f);
    if (content) content(owner,state.page);
    ImGui::EndChild(); ImGui::PopStyleVar(2);
    ImGui::SetCursorScreenPos(ImVec2(toolX,pos.y+21*gScale));
    ImGui::BeginChild("tools",ImVec2(35*gScale,120*gScale),ImGuiChildFlags_None,ImGuiWindowFlags_NoScrollbar);
    ImGui::SetWindowFontScale(1.f);
    if (ToolButton("##stats",kUser,"Player and client statistics",state.page==Page::Stats)) SelectPage(state,Page::Stats);
    if (ToolButton("##theme",kSun,"Appearance and menu actions",false)) ImGui::OpenPopup("appearance");
    if (ImGui::BeginPopup("appearance")) {
        for (int i=0;i<IM_ARRAYSIZE(kThemes);++i) if (ImGui::MenuItem(kThemes[i],nullptr,state.theme==i)) state.theme=i;
        ImGui::Separator();
        if (ImGui::MenuItem("Compact navigation",nullptr,state.navCollapsed)) state.navCollapsed=!state.navCollapsed;
        if (ImGui::MenuItem("Reduced motion",nullptr,state.reduceMotion)) state.reduceMotion=!state.reduceMotion;
        if (ImGui::MenuItem("Center menu")) state.positioned=false;
        if (ImGui::MenuItem("Reset current page",nullptr,false,state.page!=Page::Stats)) actions.reset=true;
        if (ImGui::MenuItem("Close menu","INSERT")) actions.close=true;
        ImGui::Separator();
        ImGui::TextDisabled("%s | %s",status.connected ? "Connected" : "Disconnected",status.version);
        ImGui::TextDisabled("%s",status.agent);
        ImGui::EndPopup();
    }
    if (ToolButton("##settings",kGear,"Overlay and agent settings",state.page==Page::Settings)) SelectPage(state,Page::Settings);
    ImGui::EndChild();
    ImGui::SetCursorScreenPos(ImVec2(navPos.x+10*gScale,navPos.y+10*gScale));
    ImGui::BeginChild("navigation",ImVec2(navWidth-20*gScale,height-20*gScale),ImGuiChildFlags_None,ImGuiWindowFlags_NoScrollbar);
    ImGui::SetWindowFontScale(1.f);
    const float buttonWidth=navWidth-20*gScale;
    NavButton(state,kSkull,"AIMBOT",Page::Combat,buttonWidth);
    NavButton(state,kEye,"VISUALS",Page::Visuals,buttonWidth);
    NavButton(state,kCube,"LOOT",Page::Blocks,buttonWidth);
    NavButton(state,kLayers,"HUD",Page::Hud,buttonWidth);
    ImGui::EndChild(); ImGui::End(); return actions;
}
bool BeginRows(const char* id) {
    if (!ImGui::BeginTable(id,2,ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings)) return false;
    ImGui::TableSetupColumn("Setting",ImGuiTableColumnFlags_WidthStretch,1.4f);
    ImGui::TableSetupColumn("Value",ImGuiTableColumnFlags_WidthStretch,1.f);
    return true;
}
bool CheckRow(const char* label,bool* value) {
    ImGui::PushID(label);
    ImGui::TableNextRow(0,RowHeight());
    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,ImGui::GetColorU32(ImGuiCol_TableRowBg));
    ImGui::TableNextColumn();
    const ImVec2 p=ImGui::GetCursorScreenPos();
    const float width=std::max(1.f,ImGui::GetContentRegionAvail().x);
    const float h=std::max(ControlHeight(),ImGui::CalcTextSize(label,nullptr,false,width).y+8*gScale);
    const bool labelPressed=ImGui::InvisibleButton("##label",ImVec2(width,h),ImGuiButtonFlags_EnableNav);
    if (labelPressed) *value=!*value;
    auto& draw=*ImGui::GetWindowDrawList();
    const bool labelHover=PointerHover();
    const bool labelActive=ImGui::IsItemActive();
    const float labelSelection=Animate("label selection",*value ? 1.f : 0.f,motion::kFeedbackSeconds,
                                      !labelPressed || ImGui::IsMouseReleased(ImGuiMouseButton_Left));
    draw.AddText(ImGui::GetFont(),ImGui::GetFontSize(),ImVec2(p.x,p.y+(ControlHeight()-ImGui::GetFontSize())*.5f),
        ImGui::GetColorU32(Mix(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled),ImGui::GetStyleColorVec4(ImGuiCol_Text),labelSelection)),label,nullptr,width);
    FocusOutline(p,ImVec2(width,h));
    ImGui::TableNextColumn();
    const auto cell=ImGui::GetCursorScreenPos();
    const float side=16*gScale;
    const ImVec2 box(cell.x+std::max(0.f,ImGui::GetContentRegionAvail().x-side),cell.y+(ControlHeight()-side)*.5f);
    ImGui::SetCursorScreenPos(ImVec2(box.x-4*gScale,cell.y));
    const bool pressed=ImGui::InvisibleButton("##value",ImVec2(24*gScale,ControlHeight()),ImGuiButtonFlags_EnableNav);
    if (pressed) *value=!*value;
    const float hover=Animate("hover",PointerHover() || labelHover ? 1.f : 0.f);
    const float selection=Animate("selection",*value ? 1.f : 0.f,motion::kFeedbackSeconds,
        !(pressed || labelPressed) || ImGui::IsMouseReleased(ImGuiMouseButton_Left));
    const float press=Animate("press",ImGui::IsItemActive() || labelActive ? 1.f : 0.f,motion::kFeedbackSeconds);
    const auto accent=ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
    const auto off=Mix(ImGui::GetStyleColorVec4(ImGuiCol_FrameBg),accent,.12f+.12f*hover);
    const float inset=gMotion ? side*.02f*press : 0.f;
    draw.AddRectFilled(ImVec2(box.x+inset,box.y+inset),ImVec2(box.x+side-inset,box.y+side-inset),
                       ImGui::GetColorU32(Mix(off,accent,selection)),2*gScale);
    draw.AddRect(box,ImVec2(box.x+side,box.y+side),ImGui::GetColorU32(Mix(ImGui::GetStyleColorVec4(ImGuiCol_Border),accent,selection*.7f)),2*gScale);
    if (selection>.001f) {
        const auto ink=ImGui::GetColorU32(ImGuiCol_WindowBg,selection);
        draw.AddLine(ImVec2(box.x+4*gScale,box.y+8*gScale),ImVec2(box.x+7*gScale,box.y+11*gScale),ink,1.8f*gScale);
        draw.AddLine(ImVec2(box.x+7*gScale,box.y+11*gScale),ImVec2(box.x+12*gScale,box.y+5*gScale),ink,1.8f*gScale);
    }
    FocusOutline(ImVec2(box.x-4*gScale,cell.y),ImVec2(24*gScale,ControlHeight()));
    ImGui::PopID(); return pressed || labelPressed;
}
bool ComboRow(const char* label,int* value,const char* const* items,int count) {
    ImGui::PushID(label); RowLabel(label,false);
    ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
    const bool changed=ImGui::Combo("##value",value,items,count);
    ImGui::PopStyleColor(2); ImGui::PopID(); return changed;
}
bool SliderRow(const char* label,float* value,float min,float max,const char* format) {
    return Slider(label,value,min,max,format,ImGuiDataType_Float);
}
bool SliderIntRow(const char* label,int* value,int min,int max,const char* format) {
    return Slider(label,value,min,max,format,ImGuiDataType_S32);
}
bool ColorRow(const char* label,float* value) {
    ImGui::PushID(label); RowLabel(label,false);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX()+std::max(0.f,ImGui::GetContentRegionAvail().x-18*gScale));
    const bool changed=ImGui::ColorEdit4("##value",value,ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
    ImGui::PopID(); return changed;
}
void ValueRow(const char* label,const char* value) { RowLabel(label,false); ImGui::AlignTextToFramePadding(); ImGui::TextWrapped("%s",value); }
} // namespace mcov::control_panel

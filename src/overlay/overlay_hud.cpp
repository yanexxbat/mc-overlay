#include "overlay.hpp"
#include "../common/compact_style.hpp"
#include <windows.h>
#include <cstdio>
#include <cctype>
#include <cfloat>
#include <string_view>

namespace mcov {
namespace {
constexpr float kModuleFontSize=18.f;
constexpr float kModuleRowHeight=25.f;
constexpr float kModuleInset=8.f;
constexpr float kModuleRail=2.f;
constexpr ImU32 kHudText=IM_COL32(246,247,249,255);
constexpr ImU32 kHudDetail=IM_COL32(169,177,185,255);
struct ModuleRow {
    std::string label;
    std::string detail;
    float labelWidth=0,detailWidth=0,width=0;
};
// Keep menu labels descriptive; the in-game list uses compact client-style names.
std::string HudModuleName(const char* name) {
    std::string label;
    bool upper=true;
    for(const unsigned char c:std::string_view(name)) {
        if(c==' ') { upper=true; continue; }
        label+=upper?static_cast<char>(std::toupper(c)):static_cast<char>(c);
        upper=false;
    }
    return label;
}
void HudText(ImDrawList& draw,ImFont* font,float size,ImVec2 pos,ImU32 color,const char* text) {
    draw.AddText(font,size,{pos.x+1,pos.y+1},IM_COL32(0,0,0,180),text);
    draw.AddText(font,size,pos,color,text);
}
void DrawModuleList(ImDrawList& draw,ImFont* font,std::span<const ModuleRow> rows,ImVec2 size) {
    const float right=size.x-kModuleRail;
    for(size_t n=0;n<rows.size();++n) {
        const auto& row=rows[n];
        const float y=float(n)*kModuleRowHeight;
        const float left=right-row.width-2*kModuleInset;
        ImDrawFlags corners=n==0?ImDrawFlags_RoundCornersTopLeft:ImDrawFlags_RoundCornersNone;
        if(n+1==rows.size()||rows[n+1].width<row.width) {
            corners&=~ImDrawFlags_RoundCornersNone;
            corners|=ImDrawFlags_RoundCornersBottomLeft;
        }
        draw.AddRectFilled({left,y},{right,y+kModuleRowHeight},IM_COL32(16,21,26,158),4,corners);
        const ImVec2 pos{left+kModuleInset,y+(kModuleRowHeight-kModuleFontSize)*.5f};
        HudText(draw,font,kModuleFontSize,pos,kHudText,row.label.c_str());
        if(!row.detail.empty()) HudText(draw,font,kModuleFontSize-2,
            {right-kModuleInset-row.detailWidth,pos.y+1},kHudDetail,row.detail.c_str());
    }
    draw.AddRectFilled({right,0},size,IM_COL32(245,247,249,245),1);
}
} // namespace
bool Overlay::HudEnabled(int i) const {
    if(i>=int(HudModule::Radar)&&i<=int(HudModule::Cps)) return hud_.extras[i-int(HudModule::Radar)];
    if(i==int(HudModule::Notifications)) return rockstarConfig_.Enabled(rockstar::Feature::Notifications);
    if(i==int(HudModule::Nearby)) return rockstarConfig_.Enabled(rockstar::Feature::DetectPlayer);
    if(i==int(HudModule::RegionMap)) return hud_.regionMap.enabled;
    switch(static_cast<HudModule>(i)) {
        case HudModule::Equipment:return hud_.equipment;
        case HudModule::Effects:return hud_.effects;
        case HudModule::Coordinates:return hud_.coordinates;
        case HudModule::Performance:return hud_.performance;
        case HudModule::Compass:return hud_.compass;
        case HudModule::Reach:return hud_.reachMarker;
        case HudModule::Watermark:return settings_.showWatermark;
        case HudModule::Modules:return hud_.modules;
        case HudModule::Keybinds:return hud_.keybinds;
        default:return false;
    }
}
void Overlay::SetHudEnabled(int i,bool value) {
    if(i>=int(HudModule::Radar)&&i<=int(HudModule::Cps)) hud_.extras[i-int(HudModule::Radar)]=value;
    if(i==int(HudModule::Notifications)) rockstarConfig_[rockstar::Feature::Notifications].enabled=value;
    if(i==int(HudModule::Nearby)) rockstarConfig_[rockstar::Feature::DetectPlayer].enabled=value;
    if(i==int(HudModule::RegionMap)) hud_.regionMap.enabled=value;
    switch(static_cast<HudModule>(i)) {
        case HudModule::Equipment:hud_.equipment=value;break;
        case HudModule::Effects:hud_.effects=value;break;
        case HudModule::Coordinates:hud_.coordinates=value;break;
        case HudModule::Performance:hud_.performance=value;break;
        case HudModule::Compass:hud_.compass=value;break;
        case HudModule::Reach:hud_.reachMarker=value;break;
        case HudModule::Watermark:settings_.showWatermark=value;break;
        case HudModule::Modules:hud_.modules=value;break;
        case HudModule::Keybinds:hud_.keybinds=value;break;
        default:break;
    }
}
void Overlay::SetHudEditing(bool editing) {
    if(hudEditing_==editing) return;
    hudEditing_=editing;
    if(editing) {
        SetMenuVisible(false); ApplyExtendedStyles(false); ForceForeground(hwnd_,settings_.focusMode);
    } else {
        preferencesDirty_=true; SavePreferences();
        ApplyExtendedStyles(!menuVisible_&&settings_.clickThroughWhenIdle);
        if(!menuVisible_) ForceForeground(target_.window,settings_.focusMode==FocusMode::None?FocusMode::None:FocusMode::Simple);
    }
}
void Overlay::DrawHud(const WorldSnapshot& s) {
    if(!hud_.visible&&!hudEditing_) return;
    ImGui::PushFont(style::MenuFont());
    DrawAntiVanishHud();
    auto& draw=*ImGui::GetBackgroundDrawList(); const ImVec2 viewport{float(windowW_),float(windowH_)};
    int equipment=0; for(const auto& item:s.equipment) equipment+=item.present!=0;
    const int effects=std::min<int>(s.effectCount,kMaxEffects);
    ImFont* listFont=ImGui::GetIO().FontDefault;
    if(!listFont) listFont=ImGui::GetFont();
    std::array<ModuleRow,keys::kCount> moduleRows;
    int active=0,assigned=0;
    for(int i=3;i<keys::kCount;++i) if(i!=keys::Index(keys::Action::Hud)) {
        const auto action=static_cast<keys::Action>(i);
        if(action!=keys::Action::BaseFinder&&!keys::System(action)&&ModuleEnabled(action)) {
            auto& row=moduleRows[active++]; row.label=HudModuleName(keys::kNames[i]);
            char detail[32]{};
            switch(action) {
                case keys::Action::KillAura:std::snprintf(detail,sizeof(detail),"%.1f m",combatMenu_.Store().Read().values.attack.range);break;
                case keys::Action::AimAssist:std::snprintf(detail,sizeof(detail),"%.0f deg",combat_.aimFov);break;
                case keys::Action::ChestEsp:std::snprintf(detail,sizeof(detail),"%.0f m",rockstarConfig_[rockstar::Feature::ChestEsp].range);break;
                default:break;
            }
            row.detail=detail;
            row.labelWidth=listFont->CalcTextSizeA(kModuleFontSize,FLT_MAX,0,row.label.c_str()).x;
            row.detailWidth=listFont->CalcTextSizeA(kModuleFontSize-2,FLT_MAX,0,row.detail.c_str()).x;
            row.width=row.labelWidth+(row.detail.empty()?0:5+row.detailWidth);
        }
        assigned+=bindings_[i].key!=0;
    }
    // Stable ordering avoids jitter for equally wide names.
    std::stable_sort(moduleRows.begin(),moduleRows.begin()+active,[](const auto& a,const auto& b) { return a.width>b.width; });
    if(active==0&&hudEditing_) {
        auto& row=moduleRows[0]; row.label="ActiveModules";
        row.width=row.labelWidth=listFont->CalcTextSizeA(kModuleFontSize,FLT_MAX,0,row.label.c_str()).x;
    }
    char watermark[80];
    const char* username=rockstar::Pack::DisplayName(s.username,rockstarConfig_.Enabled(rockstar::Feature::NameProtect),true);
    std::snprintf(watermark,sizeof(watermark),s.username[0]?"keim / %.32s":"keim",username);
    const ImVec2 watermarkSize{listFont->CalcTextSizeA(20.f,FLT_MAX,0,watermark).x+2,27};
    float stackY=44;
    const float lineHeight=ImGui::GetTextLineHeight();
    for(int i=0;i<kHudModules;++i) {
        hudRects_[i]={}; const auto kind=static_cast<HudModule>(i);
        if(hudEditing_&&(viewport.x<640||viewport.y<480)&&i!=hudSelected_) continue;
        if(!HudEnabled(i)&&!hudEditing_) continue;
        const bool empty=(kind==HudModule::Equipment&&!equipment)||(kind==HudModule::Effects&&!effects)||
                         (kind==HudModule::Reach&&!s.hasTarget)||(kind==HudModule::Modules&&!active)||(kind==HudModule::Keybinds&&!assigned);
        if(empty&&!hudEditing_) continue;
        ImVec2 size{232,50};
        switch(kind) {
            case HudModule::Equipment:size.y=equipment?24+equipment*(lineHeight+9):50;break;
            case HudModule::Effects:size.y=effects?28+effects*(lineHeight+4):50;break;
            case HudModule::Coordinates:size.y=28+4*(lineHeight+3);break;
            case HudModule::Performance:size.y=132;break;
            case HudModule::Compass:size={320,26};break;
            case HudModule::Reach:size.y=54;break;
            case HudModule::Watermark:size=watermarkSize;break;
            case HudModule::Modules:size={moduleRows[0].width+2*kModuleInset+kModuleRail,std::max(1,active)*kModuleRowHeight};break;
            case HudModule::Keybinds:size.y=28+std::max(1,assigned)*(lineHeight+4);break;
            case HudModule::Radar:size={196,196};break;
            case HudModule::Keystrokes:size={130,138};break;
            case HudModule::Speedometer:size.y=68;break;
            case HudModule::Session:size.y=102;break;
            case HudModule::ArmorStrip:size.y=78;break;
            case HudModule::Indicators:size.y=84;break;
            case HudModule::Notifications:size.y=145;break;
            case HudModule::Nearby:size.y=176;break;
            case HudModule::RegionMap:size=regionmap::kSize;break;
            default:break;
        }
        ImVec2 start{16,stackY};
        if(i<=3) stackY+=size.y+12;
        else if(kind==HudModule::Compass) start={(viewport.x-size.x)*.5f,12};
        else if(kind==HudModule::Reach) start={(viewport.x-size.x)*.5f,(viewport.y-size.y)*.5f};
        else if(kind==HudModule::Watermark) start={viewport.x-size.x-16,12};
        else if(kind==HudModule::Modules) start={viewport.x-size.x-16,44};
        else if(kind==HudModule::Keybinds) start={viewport.x-size.x-16,viewport.y-size.y-16};
        else if(i>=int(HudModule::Radar)) start={264+(i-int(HudModule::Radar))%3*250.f,viewport.y-size.y-20-(i-int(HudModule::Radar))/3*180.f};
        auto& placement=hud_.layout[i]; const bool wasPlaced=placement.placed;
        if(!wasPlaced&&i>=int(HudModule::Radar)) {
            // Nur neue Startpositionen suchen; gespeicherte und bewusst ueberlappende Layouts bleiben erhalten.
            bool found=false;
            const float left=viewport.x>=1000?264.f:16.f;
            for(int column=0;column<4&&!found;++column) for(float y=viewport.y-size.y-16;y>=44;y-=24) {
                const ImVec2 candidate{left+column*250,y}; if(candidate.x+size.x>viewport.x-16) break;
                bool overlap=false;
                for(int previous=0;previous<i;++previous) { const auto& r=hudRects_[previous];
                    if(r.max.x>r.min.x&&r.max.y>r.min.y&&candidate.x<r.max.x+8&&candidate.x+size.x+8>r.min.x&&candidate.y<r.max.y+8&&candidate.y+size.y+8>r.min.y) { overlap=true; break; }
                }
                if(!overlap) { start=candidate; found=true; break; }
            }
        }
        const auto oldPosition=placement.position;
        const auto rect=kind==HudModule::Modules?PlaceHudContent(placement,viewport,size,start,moduleHudAnchor_):
            kind==HudModule::Watermark?PlaceHudContent(placement,viewport,size,start,watermarkHudAnchor_):
            PlaceHud(placement,viewport,size,start);
        hudRects_[i]=rect;
        if(placement.position.x!=oldPosition.x||placement.position.y!=oldPosition.y) {
            preferencesDirty_=true; preferenceSaveAt_=GetTickCount64()+500;
        }
        if(!wasPlaced) { preferencesDirty_=true; preferenceSaveAt_=GetTickCount64()+500; }
        // Bestehende Snapshot-Widgets zeichnen lokal; nur ihre eigenen Vertices
        // und Clip-Commands werden versetzt/skaliert. Keine JNI-Abfragen im HUD.
        const int vertexStart=draw.VtxBuffer.Size;
        draw.PushClipRect({0,0},size,false); const int commandStart=draw.CmdBuffer.Size-1;
        if(empty&&kind!=HudModule::Modules) {
            DrawHudPanel(draw,{0,0},size); style::compact::Heading(draw,{0,0},size.x,kHudNames[i]);
            draw.AddText({10,28},style::compact::Muted,"No data / editor preview");
        } else switch(kind) {
            case HudModule::Equipment:DrawEquipmentPanel(s,0,0);break;
            case HudModule::Effects:DrawEffectsPanel(s,0,0);break;
            case HudModule::Coordinates:DrawCoordinatesPanel(s,0,0);break;
            case HudModule::Performance:DrawPerformancePanel(s,0,0);break;
            case HudModule::Compass:DrawCompassStrip(s,size.x);break;
            case HudModule::Reach:DrawReachMarker(s);break;
            case HudModule::Watermark: {
                HudText(draw,listFont,20.f,{0,2},kHudText,watermark); break;
            }
            case HudModule::Modules:
                DrawModuleList(draw,listFont,{moduleRows.data(),size_t(std::max(1,active))},size); break;
            case HudModule::Keybinds: {
                DrawHudPanel(draw,{0,0},size); style::compact::Heading(draw,{0,0},size.x,kHudNames[i]); float y=28;
                for(int a=3;a<keys::kCount;++a) {
                    if(a==keys::Index(keys::Action::Hud)) continue;
                    const bool on=ModuleEnabled(static_cast<keys::Action>(a));
                    if(!bindings_[a].key) continue;
                    const auto key=BindName(bindings_[a]); const auto extent=ImGui::CalcTextSize(key.c_str());
                    const float keyX=std::max(10.f,size.x-10-extent.x);
                    draw.PushClipRect({10,y},{std::max(10.f,keyX-8),y+lineHeight},true);
                    draw.AddText({10,y},on?style::compact::Accent:style::compact::Muted,keys::kNames[a]);
                    draw.PopClipRect();
                    draw.AddText({keyX,y},style::compact::Text,key.c_str());
                    y+=lineHeight+4;
                } break;
            }
            case HudModule::RegionMap:regionmap::Draw(draw,s,hud_.regionMap,agentConnected_&&haveSnapshot_&&heartbeatClock_.Alive(GetTickCount64()));break;
            default:rockstarPack_.DrawHud(draw,kind,s,rockstarConfig_,combatMenu_.Relations().Read(),size,double(GetTickCount64())/1000); break;
        }
        const int commandEnd=draw.CmdBuffer.Size;
        const float scale=(rect.max.x-rect.min.x)/size.x;
        for(int v=vertexStart;v<draw.VtxBuffer.Size;++v) { auto& p=draw.VtxBuffer[v].pos; p={rect.min.x+p.x*scale,rect.min.y+p.y*scale}; }
        for(int c=commandStart;c<commandEnd;++c) {
            auto& clip=draw.CmdBuffer[c].ClipRect;
            clip={rect.min.x+std::clamp(clip.x,0.f,size.x)*scale,rect.min.y+std::clamp(clip.y,0.f,size.y)*scale,
                  rect.min.x+std::clamp(clip.z,0.f,size.x)*scale,rect.min.y+std::clamp(clip.w,0.f,size.y)*scale};
        }
        draw.PopClipRect();
    }
    ImGui::PopFont();
}
void Overlay::DrawHudEditor() {
    style::CompactStyle themed;
    const auto size=ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({0,0}); ImGui::SetNextWindowSize(size);
    style::StyleScope scoped;
    scoped.Var(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    ImGui::Begin("HUD editor",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);
    auto& draw=*ImGui::GetWindowDrawList();
    hudSelected_=std::clamp(hudSelected_,0,kHudModules-1);
    const int selected=hudSelected_;
    // Das ausgewählte Element bekommt bei überlappenden Vorschauen die oberste
    // Hit-Fläche. Die Toolbar erreicht auch Elemente unter einem anderen Modul.
    for(int pass=0;pass<=kHudModules;++pass) {
        const int i=pass==kHudModules?selected:pass;
        if(pass<kHudModules&&i==selected) continue;
        auto rect=hudRects_[i]; const ImVec2 extent{rect.max.x-rect.min.x,rect.max.y-rect.min.y};
        if(extent.x<=0||extent.y<=0) continue;
        ImGui::SetCursorScreenPos(rect.min); ImGui::PushID(i);
        ImGui::InvisibleButton("##move",extent);
        if(ImGui::IsItemActivated()) hudSelected_=i;
        if(ImGui::IsItemActive()&&ImGui::IsMouseDragging(0,0)) {
            MoveHud(hud_.layout[i],rect,size,ImGui::GetIO().MouseDelta); preferencesDirty_=true;
        }
        if(ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll); ImGui::SetTooltip("%s: drag to move",kHudNames[i]);
        }
        draw.AddRect(rect.min,rect.max,hudSelected_==i?style::compact::Accent:IM_COL32(174,174,184,100),8*hud_.layout[i].scale);
        if(hudSelected_==i&&!HudEnabled(i)) {
            const float width=ImGui::CalcTextSize("Disabled").x+12;
            const ImVec2 badge{std::max(rect.min.x,rect.max.x-width),std::max(0.f,rect.min.y-22)};
            draw.AddRectFilled(badge,{badge.x+width,badge.y+20},style::compact::Surface,4);
            draw.AddText({badge.x+6,badge.y+3},style::compact::AccentActive,"Disabled");
        }
        ImGui::PopID();
    }
    const bool compact=size.x<640||size.y<480;
    const float width=std::min(540.f,size.x-24),height=compact?110.f:90.f;
    ImGui::SetCursorScreenPos({(size.x-width)*.5f,size.y-height-12});
    {
    style::StyleScope toolbar;
    toolbar.Color(ImGuiCol_ChildBg,style::compact::Surface);
    toolbar.Var(ImGuiStyleVar_WindowPadding,ImVec2(14,10));
    ImGui::BeginChild("editor_toolbar",{width,height},ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::TextUnformatted("HUD editor"); ImGui::SameLine(0,12);
    ImGui::TextDisabled("Esc / %s finishes",BindName(bindings_[keys::Index(keys::Action::HudEditor)]).c_str());
    if(compact) ImGui::TextDisabled("Compact editor: selected element only");
    const float available=ImGui::GetContentRegionAvail().x;
    const float enabledWidth=ImGui::GetFrameHeight()+ImGui::GetStyle().ItemInnerSpacing.x+ImGui::CalcTextSize("Enabled").x;
    ImGui::SetNextItemWidth(std::max(80.f,available-enabledWidth-60-2*ImGui::GetStyle().ItemSpacing.x));
    ImGui::Combo("##module",&hudSelected_,kHudNames.data(),kHudModules); ImGui::SameLine();
    bool enabled=HudEnabled(hudSelected_);
    if(ImGui::Checkbox("Enabled",&enabled)) { SetHudEnabled(hudSelected_,enabled); preferencesDirty_=true; }
    ImGui::SameLine(); if(ImGui::Button("Done",{60,0})) SetHudEditing(false);
    ImGui::SetNextItemWidth(std::max(80.f,available-104-ImGui::GetStyle().ItemSpacing.x-ImGui::CalcTextSize("Scale").x-ImGui::GetStyle().ItemInnerSpacing.x));
    if(ImGui::SliderFloat("Scale",&hud_.layout[hudSelected_].scale,.65f,1.6f,"%.2f x")) preferencesDirty_=true;
    ImGui::SameLine(); if(ImGui::Button("Reset layout",{104,0})) { hud_.layout={}; preferencesDirty_=true; }
    ImGui::EndChild();
    }
    ImGui::End();
}
} // namespace mcov

#include "overlay.hpp"
#include "../common/compact_style.hpp"
#include <windows.h>
#include <cstdio>

namespace mcov {
bool Overlay::HudEnabled(int i) const {
    if(i>=int(HudModule::Radar)&&i<=int(HudModule::Cps)) return hud_.extras[i-int(HudModule::Radar)];
    if(i==int(HudModule::Notifications)) return rockstarConfig_.Enabled(rockstar::Feature::Notifications);
    if(i==int(HudModule::Nearby)) return rockstarConfig_.Enabled(rockstar::Feature::DetectPlayer);
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
    auto& draw=*ImGui::GetBackgroundDrawList(); const ImVec2 viewport{float(windowW_),float(windowH_)};
    int equipment=0; for(const auto& item:s.equipment) equipment+=item.present!=0;
    const int effects=std::min<int>(s.effectCount,kMaxEffects);
    int active=0,assigned=0;
    for(int i=3;i<keys::kCount;++i) if(i!=keys::Index(keys::Action::Hud)) {
        active+=ModuleEnabled(static_cast<keys::Action>(i)); assigned+=bindings_[i].key!=0;
    }
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
            case HudModule::Watermark:size={260,28};break;
            case HudModule::Modules:size.y=28+std::max(1,active)*(lineHeight+4);break;
            case HudModule::Keybinds:size.y=28+std::max(1,assigned)*(lineHeight+4);break;
            case HudModule::Radar:size={196,196};break;
            case HudModule::Keystrokes:size={130,138};break;
            case HudModule::Speedometer:size.y=68;break;
            case HudModule::Session:size.y=102;break;
            case HudModule::ArmorStrip:size.y=78;break;
            case HudModule::Indicators:size.y=84;break;
            case HudModule::Notifications:size.y=145;break;
            case HudModule::Nearby:size.y=176;break;
            default:break;
        }
        ImVec2 start{16,stackY};
        if(i<=3) stackY+=size.y+12;
        else if(kind==HudModule::Compass) start={(viewport.x-size.x)*.5f,12};
        else if(kind==HudModule::Reach) start={(viewport.x-size.x)*.5f,(viewport.y-size.y)*.5f};
        else if(kind==HudModule::Watermark) start={16,8};
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
        const auto rect=PlaceHud(placement,viewport,size,start); hudRects_[i]=rect;
        if(!wasPlaced) { preferencesDirty_=true; preferenceSaveAt_=GetTickCount64()+500; }
        // Bestehende Snapshot-Widgets zeichnen lokal; nur ihre eigenen Vertices
        // und Clip-Commands werden versetzt/skaliert. Keine JNI-Abfragen im HUD.
        const int vertexStart=draw.VtxBuffer.Size;
        draw.PushClipRect({0,0},size,false); const int commandStart=draw.CmdBuffer.Size-1;
        if(empty) {
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
                DrawHudPanel(draw,{0,0},size); char text[100];
                std::snprintf(text,sizeof(text),"mcov | %d FPS | %s",s.fps,s.gameVersion[0]?s.gameVersion:"waiting");
                draw.AddCircleFilled({12,14},2.5f,style::compact::Accent,12);
                draw.AddText({22,7},style::compact::Text,text); break;
            }
            case HudModule::Modules:case HudModule::Keybinds: {
                DrawHudPanel(draw,{0,0},size); style::compact::Heading(draw,{0,0},size.x,kHudNames[i]); float y=28;
                for(int a=3;a<keys::kCount;++a) {
                    if(a==keys::Index(keys::Action::Hud)) continue;
                    const bool on=ModuleEnabled(static_cast<keys::Action>(a));
                    if(kind==HudModule::Modules?!on:!bindings_[a].key) continue;
                    if(kind==HudModule::Keybinds) {
                        const auto key=BindName(bindings_[a]); const auto extent=ImGui::CalcTextSize(key.c_str());
                        const float keyX=std::max(10.f,size.x-10-extent.x);
                        draw.PushClipRect({10,y},{std::max(10.f,keyX-8),y+lineHeight},true);
                        draw.AddText({10,y},on?style::compact::Accent:style::compact::Muted,keys::kNames[a]);
                        draw.PopClipRect();
                        draw.AddText({keyX,y},style::compact::Text,key.c_str());
                    } else draw.AddText({10,y},style::compact::Accent,keys::kNames[a]);
                    y+=lineHeight+4;
                } break;
            }
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
    const auto size=ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({0,0}); ImGui::SetNextWindowSize(size);
    style::StyleScope scoped;
    scoped.Var(ImGuiStyleVar_WindowPadding,ImVec2(5,5)); scoped.Var(ImGuiStyleVar_ChildRounding,0.f);
    scoped.Var(ImGuiStyleVar_FrameRounding,0.f); scoped.Var(ImGuiStyleVar_FramePadding,ImVec2(4,2));
    scoped.Var(ImGuiStyleVar_PopupRounding,0.f); scoped.Var(ImGuiStyleVar_FrameBorderSize,1.f);
    scoped.Var(ImGuiStyleVar_ItemSpacing,ImVec2(6,4));
    scoped.Color(ImGuiCol_ChildBg,IM_COL32(36,36,36,255)); scoped.Color(ImGuiCol_Border,IM_COL32(8,8,8,255));
    scoped.Color(ImGuiCol_FrameBg,IM_COL32(25,25,25,255)); scoped.Color(ImGuiCol_Button,IM_COL32(25,25,25,255));
    scoped.Color(ImGuiCol_PopupBg,IM_COL32(36,36,36,255)); scoped.Color(ImGuiCol_Header,IM_COL32(46,39,51,255));
    scoped.Color(ImGuiCol_HeaderHovered,IM_COL32(64,50,71,255));
    scoped.Color(ImGuiCol_ButtonHovered,IM_COL32(57,48,63,255)); scoped.Color(ImGuiCol_ButtonActive,IM_COL32(73,52,81,255));
    scoped.Color(ImGuiCol_CheckMark,IM_COL32(174,105,209,255)); scoped.Color(ImGuiCol_SliderGrab,IM_COL32(174,105,209,255));
    scoped.Color(ImGuiCol_SliderGrabActive,IM_COL32(205,160,225,255));
    ImGui::PushFont(style::ClassicMenuFont());
    ImGui::Begin("HUD editor",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);
    auto& draw=*ImGui::GetWindowDrawList();
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
        draw.AddRect(rect.min,rect.max,hudSelected_==i?IM_COL32(190,119,225,255):IM_COL32(140,134,146,180));
        if(!HudEnabled(i)) draw.AddText({rect.min.x+8,rect.min.y+5},IM_COL32(232,130,130,255),"Disabled");
        ImGui::PopID();
    }
    const float width=std::min(430.f,size.x-16);
    ImGui::SetCursorScreenPos({(size.x-width)*.5f,size.y-91});
    ImGui::BeginChild("editor_toolbar",{width,83},ImGuiChildFlags_Borders|ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::Text("HUD editor | Esc / %s finishes",BindName(bindings_[keys::Index(keys::Action::HudEditor)]).c_str());
    if(size.x<640||size.y<480) ImGui::TextDisabled("Compact editor: selected element only");
    ImGui::SetNextItemWidth(width*.42f);
    ImGui::Combo("##module",&hudSelected_,kHudNames.data(),kHudModules); ImGui::SameLine();
    bool enabled=HudEnabled(hudSelected_);
    if(ImGui::Checkbox("Enabled",&enabled)) { SetHudEnabled(hudSelected_,enabled); preferencesDirty_=true; }
    ImGui::SameLine(); if(ImGui::Button("Done")) SetHudEditing(false);
    ImGui::SetNextItemWidth(width*.55f);
    if(ImGui::SliderFloat("Scale",&hud_.layout[hudSelected_].scale,.65f,1.6f,"%.2f x")) preferencesDirty_=true;
    ImGui::SameLine(); if(ImGui::Button("Reset layout")) { hud_.layout={}; preferencesDirty_=true; }
    ImGui::EndChild(); ImGui::End(); ImGui::PopFont();
}
} // namespace mcov

#pragma once
#include "imgui.h"
#include "../common/compact_theme.hpp"
#include "../common/hud_style.hpp"
#include "region_map.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <span>

namespace mcov {
enum class HudModule { Equipment, Effects, Coordinates, Performance, Compass, Reach, Watermark, Modules, Keybinds,
    Radar, Keystrokes, Speedometer, Session, ArmorStrip, Indicators, Clock, Cps, Notifications, Nearby, RegionMap, TargetInfo, Count };
inline constexpr int kHudModules=static_cast<int>(HudModule::Count);
inline constexpr std::array<const char*,kHudModules> kHudNames={"Equipment","Status effects","Coordinates","Performance",
    "Compass","Reach indicator","Watermark","Active modules","Keybinds","Radar","Keystrokes","Speedometer",
    "Session statistics","Armor strip","Combat indicators","Clock","Click rate","Notifications","Nearby players","Region map","Target Info"};
struct HudPlacement {
    ImVec2 position{}; // Normalisiert im verfügbaren Weg, damit Auflösungswechsel die Anker erhalten.
    float scale=1;
    bool placed=false;
};
struct HudRect { ImVec2 min{},max{}; };
inline HudRect PlaceHud(HudPlacement& p,ImVec2 viewport,ImVec2 size,ImVec2 defaultPosition) {
    p.scale=std::clamp(std::isfinite(p.scale)?p.scale:1.f,.65f,1.6f);
    // Auf kleinen Viewports passt die gesamte Fläche hinein, ohne gespeicherte Skalierung zu ändern.
    const float fit=std::min({p.scale,viewport.x/std::max(1.f,size.x),viewport.y/std::max(1.f,size.y)});
    size={size.x*fit,size.y*fit}; const ImVec2 travel{std::max(0.f,viewport.x-size.x),std::max(0.f,viewport.y-size.y)};
    if(!p.placed) { p.position={travel.x>0?defaultPosition.x/travel.x:0,travel.y>0?defaultPosition.y/travel.y:0}; p.placed=true; }
    p.position={std::clamp(std::isfinite(p.position.x)?p.position.x:0.f,0.f,1.f),
                std::clamp(std::isfinite(p.position.y)?p.position.y:0.f,0.f,1.f)};
    const ImVec2 min{travel.x*p.position.x,travel.y*p.position.y}; return {min,{min.x+size.x,min.y+size.y}};
}
inline void MoveHud(HudPlacement& p,HudRect rect,ImVec2 viewport,ImVec2 delta) {
    const ImVec2 travel{std::max(0.f,viewport.x-(rect.max.x-rect.min.x)),std::max(0.f,viewport.y-(rect.max.y-rect.min.y))};
    p.position={travel.x>0?std::clamp((rect.min.x+delta.x)/travel.x,0.f,1.f):0,
                travel.y>0?std::clamp((rect.min.y+delta.y)/travel.y,0.f,1.f):0};
}
inline constexpr float kHudGrid=16.f;
struct HudSnap {
    ImVec2 position{};
    float guideX=-1,guideY=-1;
};
inline HudSnap SnapHud(ImVec2 desired,ImVec2 extent,ImVec2 viewport,std::span<const HudRect> peers,int selected,bool snapping) {
    const ImVec2 travel{std::max(0.f,viewport.x-extent.x),std::max(0.f,viewport.y-extent.y)};
    desired={std::clamp(desired.x,0.f,travel.x),std::clamp(desired.y,0.f,travel.y)};
    HudSnap result{desired};
    if(!snapping) return result;
    result.position={std::clamp(std::round(desired.x/kHudGrid)*kHudGrid,0.f,travel.x),
                     std::clamp(std::round(desired.y/kHudGrid)*kHudGrid,0.f,travel.y)};
    // Prefer nearby matching edges/centres over the grid, including scaled widgets.
    float bestX=6.f,bestY=6.f;
    for(size_t i=0;i<peers.size();++i) {
        const auto& peer=peers[i];
        if(int(i)==selected||peer.max.x<=peer.min.x||peer.max.y<=peer.min.y) continue;
        for(float fraction:{0.f,.5f,1.f}) {
            const float guideX=peer.min.x+(peer.max.x-peer.min.x)*fraction;
            const float guideY=peer.min.y+(peer.max.y-peer.min.y)*fraction;
            const float x=guideX-extent.x*fraction,y=guideY-extent.y*fraction;
            if(x>=0&&x<=travel.x&&std::abs(x-desired.x)<bestX) {
                bestX=std::abs(x-desired.x); result.position.x=x; result.guideX=guideX;
            }
            if(y>=0&&y<=travel.y&&std::abs(y-desired.y)<bestY) {
                bestY=std::abs(y-desired.y); result.position.y=y; result.guideY=guideY;
            }
        }
    }
    return result;
}
// Runtime-only measurements: changing text/content must not move a widget's anchor.
// Explicit editor moves, scale changes and viewport changes still use PlaceHud.
struct HudContentAnchor {
    HudPlacement placement{};
    HudRect rect{};
    ImVec2 viewport{};
};
inline HudRect PlaceHudContent(HudPlacement& p,ImVec2 viewport,ImVec2 size,ImVec2 start,HudContentAnchor& previous) {
    const bool contentOnly=p.placed&&previous.placement.placed&&p.scale==previous.placement.scale&&
        p.position.x==previous.placement.position.x&&p.position.y==previous.placement.position.y&&
        viewport.x==previous.viewport.x&&viewport.y==previous.viewport.y;
    auto rect=PlaceHud(p,viewport,size,start);
    const bool resized=std::abs((rect.max.x-rect.min.x)-(previous.rect.max.x-previous.rect.min.x))>.01f||
        std::abs((rect.max.y-rect.min.y)-(previous.rect.max.y-previous.rect.min.y))>.01f;
    if(contentOnly&&resized) {
        const float x=p.position.x>=.5f?previous.rect.max.x-rect.max.x:previous.rect.min.x-rect.min.x;
        MoveHud(p,rect,viewport,{x,previous.rect.min.y-rect.min.y});
        rect=PlaceHud(p,viewport,size,start);
    }
    previous={p,rect,viewport};
    return rect;
}
struct HudSettings {
    bool visible=true,equipment=true,effects=true,coordinates=true,compass=true,performance=false,reachMarker=true;
    bool modules=true,keybinds=true;
    std::array<bool,10> extras{};
    regionmap::Config regionMap{};
    float durabilityWarn=.2f;
    std::array<HudPlacement,kHudModules> layout{};
};
inline void DrawHudPanel(ImDrawList& draw,ImVec2 min,ImVec2 max) {
    style::hud::Panel(draw,min,max);
}
} // namespace mcov

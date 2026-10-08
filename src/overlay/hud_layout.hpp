#pragma once
#include "imgui.h"
#include "../common/compact_theme.hpp"
#include <array>
#include <algorithm>
#include <cmath>

namespace mcov {
enum class HudModule { Equipment, Effects, Coordinates, Performance, Compass, Reach, Watermark, Modules, Keybinds,
    Radar, Keystrokes, Speedometer, Session, ArmorStrip, Indicators, Clock, Cps, Notifications, Nearby, Count };
inline constexpr int kHudModules=static_cast<int>(HudModule::Count);
inline constexpr std::array<const char*,kHudModules> kHudNames={"Equipment","Status effects","Coordinates","Performance",
    "Compass","Reach indicator","Watermark","Active modules","Keybinds","Radar","Keystrokes","Speedometer",
    "Session statistics","Armor strip","Combat indicators","Clock","Click rate","Notifications","Nearby players"};
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
struct HudSettings {
    bool visible=true,equipment=true,effects=true,coordinates=true,compass=true,performance=false,reachMarker=true;
    bool modules=true,keybinds=true;
    std::array<bool,10> extras{};
    float durabilityWarn=.2f;
    std::array<HudPlacement,kHudModules> layout{};
};
inline void DrawHudPanel(ImDrawList& draw,ImVec2 min,ImVec2 max) {
    style::compact::Panel(draw,min,max);
}
} // namespace mcov

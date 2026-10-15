#include "debug_hud.hpp"
#include "imgui.h"
#include "../common/compact_theme.hpp"
#include <cstdio>

namespace mcov::combat {
namespace {
constexpr float kMargin=16, kPanelWidth=320, kLineHeight=18, kPlotHeight=68, kPadding=12;
constexpr ImU32 kText=style::compact::Text, kMuted=style::compact::Muted,
    kAccent=style::compact::Accent, kTarget=style::compact::AccentActive;
}
void CombatHud::Draw(ImDrawList& draw,const DebugSnapshot& s,const HudConfig& c,const ImVec2& viewport) {
    if(!c.enabled || viewport.x<64 || viewport.y<64 || !std::isfinite(viewport.x) || !std::isfinite(viewport.y)) return;
    draw.PushClipRect(ImVec2(0,0),viewport,true);
    struct ClipScope { ImDrawList& draw; ~ClipScope() { draw.PopClipRect(); } } clip{draw};
    const ImVec2 center(viewport.x*0.5f,viewport.y*0.5f);
    if(c.fov && s.enabled && s.fovRadiusPx>0 && std::isfinite(s.fovRadiusPx)) {
        draw.AddCircle(center,static_cast<float>(std::min(s.fovRadiusPx,static_cast<double>(std::hypot(viewport.x,viewport.y)))),kAccent,96,1);
    }
    if(!c.target && !c.packetStats && !c.trace) return;
    const float width=std::min(kPanelWidth,viewport.x-2*kMargin);
    const float height=2*kPadding+(c.target ? 3*kLineHeight : 0)+(c.packetStats ? 2*kLineHeight : 0)+(c.trace ? kPlotHeight+kLineHeight : 0);
    const ImVec2 start(kMargin,kMargin),end(kMargin+width,kMargin+height);
    style::compact::Panel(draw,start,end);
    draw.PushClipRect(ImVec2(start.x+kPadding,start.y),ImVec2(end.x-kPadding,end.y),true);
    ClipScope bodyClip{draw};
    float y=start.y+kPadding;
    char text[256];
    auto line=[&](const char* value,ImU32 color=kText) { draw.AddText(ImVec2(start.x+kPadding,y),color,value); y+=kLineHeight; };
    if(c.target) {
        if(s.target) {
            std::snprintf(text,sizeof(text),"%.*s  %.2fm  %.1f deg",static_cast<int>(s.target->name.size()),s.target->name.data(),s.target->range,s.target->angle);
            line(text);
            std::snprintf(text,sizeof(text),"Error %.2f deg  Cooldown %.0f%%",s.error,s.cooldown*100); line(text,kMuted);
        } else { line("Combat Assist"); line("No valid target",kMuted); }
        line(s.silentUnavailable ? "Silent adapter unavailable" : s.cooldownUnavailable ? "Cooldown unknown" : s.resyncing ? "Resyncing" : s.reacting ? "Reaction delay" : s.enabled ? "Active" : "Disabled",kMuted);
    }
    if(c.packetStats) {
        std::snprintf(text,sizeof(text),"Sent %llu  Buffered %zu",static_cast<unsigned long long>(s.packets.sent),s.packets.buffered); line(text,kMuted);
        std::snprintf(text,sizeof(text),"Discarded %llu  Failed %llu",static_cast<unsigned long long>(s.packets.discarded),static_cast<unsigned long long>(s.packets.failed)); line(text,kMuted);
    }
    if(c.trace) {
        line("Rotation: target / actual",kMuted);
        const std::size_t count=std::min(s.traceCount,kTraceCapacity);
        if(count>=2) {
            double extent=1;
            const Angles origin=s.trace[0].actual;
            for(std::size_t i=0;i<count;++i) {
                extent=std::max({extent,std::abs(Wrap(s.trace[i].desired.yaw-origin.yaw)),std::abs(Wrap(s.trace[i].actual.yaw-origin.yaw)),std::abs(s.trace[i].desired.pitch-origin.pitch),std::abs(s.trace[i].actual.pitch-origin.pitch)});
            }
            // Feste Arrays: keine Allokation für die Trace-Geometrie pro Frame.
            std::array<ImVec2,kTraceCapacity> wantedYaw{},actualYaw{},wantedPitch{},actualPitch{};
            for(std::size_t i=0;i<count;++i) {
                const auto& p=s.trace[i]; const float x=start.x+kPadding+(width-2*kPadding)*static_cast<float>(i)/static_cast<float>(count-1);
                auto point=[&](double value) { return ImVec2(x,y+kPlotHeight*0.5f-static_cast<float>(value/extent)*kPlotHeight*0.45f); };
                wantedYaw[i]=point(Wrap(p.desired.yaw-origin.yaw)); actualYaw[i]=point(Wrap(p.actual.yaw-origin.yaw));
                wantedPitch[i]=point(p.desired.pitch-origin.pitch); actualPitch[i]=point(p.actual.pitch-origin.pitch);
            }
            draw.AddPolyline(wantedYaw.data(),static_cast<int>(count),kTarget,0,1);
            draw.AddPolyline(actualYaw.data(),static_cast<int>(count),kText,0,1.5f);
            draw.AddPolyline(wantedPitch.data(),static_cast<int>(count),IM_COL32(255,163,136,100),0,1);
            draw.AddPolyline(actualPitch.data(),static_cast<int>(count),IM_COL32(236,236,239,100),0,1);
        }
    }
}
} // namespace mcov::combat

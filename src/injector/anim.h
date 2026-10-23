#pragma once
#include "../common/workspace_style.hpp"
#include <algorithm>
#include <unordered_map>
#include <string>
#include <vector>

// Launcher-only motion tokens. Seconds throughout; no frame counters or sleeps.
namespace mcov::launcher::anim {
inline constexpr float Hover=.15f, Press=.15f, Transition=.24f, Focus=.2f;
inline constexpr bool IntroEnabled=true;
inline constexpr float Intro=2.4f, IntroReveal=.3f, IntroExit=.3f;
inline constexpr float Success=.3f, Error=.3f, Copied=1.4f;
inline constexpr float Slide=7.f, Shake=4.f;
inline constexpr unsigned Accent=style::workspace::AccentRGB; // #B3DB8F
inline constexpr unsigned AccentHover=style::workspace::AccentHoverRGB;
inline constexpr unsigned AccentPressed=style::workspace::AccentPressedRGB;
inline constexpr unsigned Danger=0xF28080, Disabled=0x3E4940;
constexpr float unit(float t) { return t<0?0:t>1?1:t; }
constexpr float easeOutCubic(float t) { t=1-unit(t); return 1-t*t*t; }
constexpr float easeInOutCubic(float t) { t=unit(t); return t<.5f?4*t*t*t:1-(-2*t+2)*(-2*t+2)*(-2*t+2)/2; }
inline ImVec4 mix(ImVec4 a,ImVec4 b,float t) {
    return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,a.w+(b.w-a.w)*t};
}
struct Tween {
    float value=0,from=0,target=0,elapsed=0;
    bool initialized=false;
    float step(float next,float dt,bool enabled,float duration=Transition,float initial=0) {
        if(!initialized) { value=from=target=initial; initialized=true; }
        if(next!=target) { from=value; target=next; elapsed=0; }
        elapsed+=std::max(0.f,dt);
        value=enabled?from+(target-from)*easeOutCubic(elapsed/duration):target;
        if(!enabled) { from=target; elapsed=duration; }
        return value;
    }
};
struct State {
    // IDs are scoped by ImGui's window/PushID stack; state belongs to one View.
    std::unordered_map<ImGuiID,Tween> widgets;
    float dt=0,clock=0,introAge=0,successAge=Success,errorAge=Error,copyAge=Copied+Transition;
    bool introDone=false,wasLicensed=false,wasError=false,wasBusy=false;
    int page=-1;
    float pageAge=Transition;
    std::string logSnapshot;
    std::vector<std::string> lines;
    std::vector<float> lineBirth;
    float to(ImGuiID id,float target,bool enabled,float duration=Transition,float initial=0) {
        return widgets[id].step(target,dt,enabled,duration,initial);
    }
};
} // namespace mcov::launcher::anim

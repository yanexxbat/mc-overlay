#pragma once
#include <algorithm>
#include <unordered_map>
#include "imgui.h"

namespace mcov::control_panel::motion {
inline constexpr float kFeedbackSeconds = .12f;
inline constexpr float kHoverSeconds = .14f;
inline constexpr float kPageSeconds = .16f;

// The UI's shared ease-out curve: cubic-bezier(0.23, 1, 0.32, 1).
// Solve x numerically; use actual elapsed time rather than frame-dependent lerp.
inline float EaseOut(float progress) {
    if (progress <= 0.f) return 0.f;
    if (progress >= 1.f) return 1.f;
    float low = 0.f, high = 1.f;
    for (int i = 0; i < 16; ++i) {
        const float t = (low + high) * .5f, u = 1.f - t;
        const float x = 3.f * u * u * t * .23f + 3.f * u * t * t * .32f + t * t * t;
        if (x < progress) low = t; else high = t;
    }
    const float t = (low + high) * .5f, u = 1.f - t;
    return 1.f - u * u * u;
}
// Menu-only state; each control panel owns this map. Existing world-effect
// EaseOut remains unchanged. Seconds and pixels are the tuning surface.
inline constexpr float Hover=.15f, Press=.15f, Transition=.24f, Focus=.20f, Slide=7.f;
inline float Cubic(float t) { t=1-std::clamp(t,0.f,1.f); return 1-t*t*t; }
inline ImVec4 Mix(ImVec4 a,ImVec4 b,float t) {
    return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,a.w+(b.w-a.w)*t};
}
struct Tween {
    float value=0,from=0,target=0,elapsed=0;
    bool initialized=false;
    float Step(float next,float dt,bool enabled,float duration=Transition,float initial=0) {
        if(!initialized) { value=from=target=initial; initialized=true; }
        if(next!=target) { from=value; target=next; elapsed=0; }
        elapsed+=std::max(0.f,dt);
        value=enabled?from+(target-from)*Cubic(elapsed/duration):target;
        if(!enabled) { from=target; elapsed=duration; }
        return value;
    }
};
struct State {
    std::unordered_map<ImGuiID,Tween> widgets;
    Tween window;
    bool visible=true,enabled=true;
    float dt=0,pageAge=Transition,detailAge=Transition;
    float To(ImGuiID id,float target,float duration=Transition,float initial=0) {
        return widgets[id].Step(target,dt,enabled,duration,initial);
    }
};} // namespace mcov::control_panel::motion

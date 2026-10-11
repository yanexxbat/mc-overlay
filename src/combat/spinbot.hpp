#pragma once
#include "types.hpp"

namespace mcov::combat {
struct SpinConfig {
    bool enabled=false;
    float speed=720.f;             // Grad pro Sekunde, unabhaengig von Render-FPS.
    int direction=2;               // 0: rechts, 1: links, 2: wechselnd.
    float reverseSeconds=3.f;
    int pitchMode=0;               // 0: Kamera-Pitch, 1: unten, 2: oben, 3: Welle.
    float pitchAmplitude=35.f;
    float pitchFrequency=.6f;
};
enum class SpinStatus : std::uint32_t { Disabled, Active, Paused, CombatPriority, NoWorld, SilentUnavailable, Error };
inline const char* SpinStatusText(SpinStatus status) {
    switch(status) {
        case SpinStatus::Active:return "Spinning / independent camera";
        case SpinStatus::Paused:return "Paused - close menus and focus Minecraft";
        case SpinStatus::CombatPriority:return "Kill Aura has rotation priority";
        case SpinStatus::NoWorld:return "Join a world";
        case SpinStatus::SilentUnavailable:return "Camera hook missing - restart Minecraft";
        case SpinStatus::Error:return "Stopped - reconnect with injector";
        default:return "Off";
    }
}
inline bool Valid(const SpinConfig& c) {
    return std::isfinite(c.speed)&&c.speed>=30&&c.speed<=1800&&c.direction>=0&&c.direction<=2&&
        std::isfinite(c.reverseSeconds)&&c.reverseSeconds>=.5f&&c.reverseSeconds<=10&&
        c.pitchMode>=0&&c.pitchMode<=3&&std::isfinite(c.pitchAmplitude)&&c.pitchAmplitude>=0&&c.pitchAmplitude<=80&&
        std::isfinite(c.pitchFrequency)&&c.pitchFrequency>=.1f&&c.pitchFrequency<=3;
}
class Spinbot final {
public:
    void Reset() noexcept { active_=false; time_=velocity_=0; }
    std::optional<Angles> Step(const SpinConfig& c,Angles observed,Angles camera,double dt,bool combatPriority=false) noexcept {
        if(!c.enabled||!Valid(c)||!Finite(observed)||!Finite(camera)||!std::isfinite(dt)||dt<0||combatPriority) {
            Reset(); return {};
        }
        constexpr double maxDt=.1,maxStep=90,responseSeconds=.18;
        dt=std::min(dt,maxDt);
        const double sign=c.direction==1?-1:c.direction==2&&std::fmod(time_,2*c.reverseSeconds)>=c.reverseSeconds?-1:1;
        const double desired=sign*c.speed;
        if(!active_) { velocity_=desired; active_=true; }
        else velocity_+=(desired-velocity_)*(1-std::exp(-dt/responseSeconds));
        const double yaw=Wrap(observed.yaw+std::clamp(velocity_*dt,-maxStep,maxStep));
        double pitch=camera.pitch;
        if(c.pitchMode==1) pitch=89;
        else if(c.pitchMode==2) pitch=-89;
        else if(c.pitchMode==3) pitch+=std::sin(time_*2*kPi*c.pitchFrequency)*c.pitchAmplitude;
        // Zeitphase begrenzen; mehrere Wochen Laufzeit verlieren keine Winkelpraezision.
        time_+=dt;
        if(time_>3600) time_=std::fmod(time_,2*c.reverseSeconds);
        return Angles{yaw,std::clamp(pitch,-89.0,89.0)};
    }
private:
    bool active_{};
    double time_{},velocity_{};
};
} // namespace mcov::combat

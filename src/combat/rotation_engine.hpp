#pragma once
#include "config.hpp"

namespace mcov::combat {
inline double Bezier(double a,double b,double c,double d,double t) {
    const double u=1-t;
    return u*u*u*a+3*u*u*t*b+3*u*t*t*c+t*t*t*d;
}
class RotationEngine {
public:
    void Reset(Angles observed={}) { active_=false; current_=Normalize(observed); velocity_={}; tick_=~std::uint64_t{}; usedYaw_=usedPitch_=0; }
    bool Reacting(Seconds now) const { return active_ && now<start_; }
    Angles Step(const Frame& f,const Target& target,const RotationConfig& c,Random& rng,double speedScale=1) {
        const bool newTarget=!active_ || target.id!=id_ || target.uuid!=uuid_;
        if(newTarget) { if(!active_) current_=Normalize(f.observedRotation); Plan(target,f.now,c,rng,true); }
        else if(AngularDistance(goal_,target.desired)>c.replanDegrees) Plan(target,f.now,c,rng,false);
        if(f.tick!=tick_) { tick_=f.tick; usedYaw_=usedPitch_=0; }
        if(!c.enabled || f.now<start_ || f.dt<=0) return current_;
        const Seconds elapsed=f.now-start_;
        Angles wanted{Evaluate(yaw_,elapsed,c),Evaluate(pitch_,elapsed,c)};
        if(f.now>=noiseEnd_) {
            noiseFrom_=noiseTo_;
            const double amplitude=rng.Sample(c.jitterDegrees);
            noiseTo_={rng.Sample({-amplitude,amplitude}),rng.Sample({-amplitude,amplitude})};
            noiseStart_=f.now; noiseEnd_=f.now+rng.Sample(c.jitterPeriod);
        }
        const double u=std::clamp((f.now-noiseStart_)/(noiseEnd_-noiseStart_),0.0,1.0);
        const double smooth=u*u*(3-2*u);
        wanted.yaw+=std::lerp(noiseFrom_.yaw,noiseTo_.yaw,smooth);
        wanted.pitch+=std::lerp(noiseFrom_.pitch,noiseTo_.pitch,smooth);
        if(c.jitterDegrees.max==0 && std::abs(Wrap(wanted.yaw-current_.yaw))<c.settleDegrees) wanted.yaw=current_.yaw;
        if(c.jitterDegrees.max==0 && std::abs(wanted.pitch-current_.pitch)<c.settleDegrees) wanted.pitch=current_.pitch;
        // Zwei Grenzen: zeitabhängiger Schritt UND Restbudget des echten Spielticks.
        const double fraction=std::min(f.dt,c.tickSeconds)/c.tickSeconds;
        const double capYaw=std::min(c.maxYawPerTick*fraction*speedScale,std::max(0.0,c.maxYawPerTick-usedYaw_));
        const double capPitch=std::min(c.maxPitchPerTick*fraction*speedScale,std::max(0.0,c.maxPitchPerTick-usedPitch_));
        const Angles step{std::clamp(Wrap(wanted.yaw-current_.yaw),-capYaw,capYaw),std::clamp(wanted.pitch-current_.pitch,-capPitch,capPitch)};
        usedYaw_+=std::abs(step.yaw); usedPitch_+=std::abs(step.pitch);
        velocity_={step.yaw/f.dt,step.pitch/f.dt};
        current_=Normalize({current_.yaw+step.yaw,current_.pitch+step.pitch});
        return current_;
    }
private:
    struct Axis { double begin{},control{},overshoot{},goal{},duration{}; };
    static double Evaluate(const Axis& a,Seconds elapsed,const RotationConfig& c) {
        if(elapsed<a.duration) return Bezier(a.begin,a.control,a.overshoot,a.overshoot,std::clamp(elapsed/a.duration,0.0,1.0));
        // Nullgeschwindigkeit am Umkehrpunkt; Korrektur startet ohne Tangentensprung.
        const double u=std::clamp((elapsed-a.duration)/c.correctionDuration,0.0,1.0);
        return Bezier(a.overshoot,a.overshoot,a.goal,a.goal,u);
    }
    void Plan(const Target& t,Seconds now,const RotationConfig& c,Random& rng,bool react) {
        const Angles delta{Wrap(t.desired.yaw-current_.yaw),t.desired.pitch-current_.pitch};
        const double fraction=rng.Sample(c.overshootFraction);
        auto axis=[&](double begin,double change,double velocity,Range<double> duration) {
            const double time=rng.Sample(duration)*c.smoothing;
            const double overshoot=std::copysign(std::min(std::abs(change)*fraction,c.maxOvershootDegrees),change);
            const double end=begin+change+overshoot;
            const double control=begin+std::clamp(velocity*time*c.tangentFraction,-std::abs(change),std::abs(change));
            return Axis{begin,control,end,begin+change,time};
        };
        yaw_=axis(current_.yaw,delta.yaw,velocity_.yaw,c.yawDuration);
        pitch_=axis(current_.pitch,delta.pitch,velocity_.pitch,c.pitchDuration);
        goal_=t.desired; start_=now+(react ? rng.Sample(c.reactionDelay) : 0);
        if(!active_) { noiseFrom_=noiseTo_={}; noiseStart_=now; noiseEnd_=now+rng.Sample(c.jitterPeriod); }
        active_=true; id_=t.id; uuid_=t.uuid;
    }
    Axis yaw_{},pitch_{};
    Angles current_{},goal_{},velocity_{},noiseFrom_{},noiseTo_{};
    std::uint32_t id_{};
    Identity uuid_{};
    std::uint64_t tick_{~std::uint64_t{}};
    Seconds start_{},noiseStart_{},noiseEnd_{};
    double usedYaw_{},usedPitch_{};
    bool active_{};
};
} // namespace mcov::combat

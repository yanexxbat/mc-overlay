#pragma once
#include "target_selector.hpp"

namespace mcov::combat {
inline bool CanAttack(const Frame& f,const Target& t,const Config& c) {
    return c.attack.enabled && f.inputAllowed && Fresh(f.sampledAt,f.now,c.frameMaxAge) &&
        (!c.attack.silentRotation || f.silentSupported) && f.attackHitId==t.id &&
        f.attackCooldown && Within(*f.attackCooldown,c.attack.cooldownThreshold,1) &&
        f.gameAttackReach && Within(*f.gameAttackReach,0.1,64) &&
        t.range<=std::min(c.attack.range,*f.gameAttackReach) &&
        AngularDistance(f.observedRotation,t.desired)<=c.attack.alignmentDegrees;
}
class AttackController {
public:
    void Reset() { target_=0; pending_=false; waitingReset_=false; readyAt_=0; }
    bool Prepare(const Frame& f,const Target& t,const Config& c,Random& rng) {
        if(target_!=t.id || uuid_!=t.uuid) { Reset(); target_=t.id; uuid_=t.uuid; readyAt_=f.now+rng.Sample(c.attack.firstClickDelay); }
        if(f.attackCooldown && *f.attackCooldown<c.attack.cooldownThreshold) waitingReset_=false;
        if(pending_ || waitingReset_ || f.now<readyAt_ || !CanAttack(f,t,c)) return false;
        pending_=true; return true;
    }
    void Discarded() { pending_=false; }
    void Sent() { pending_=false; waitingReset_=true; } // Kein zweiter Schlag auf denselben alten Cooldown-Wert.
private:
    std::uint32_t target_{};
    Identity uuid_{};
    Seconds readyAt_{};
    bool pending_{},waitingReset_{};
};
} // namespace mcov::combat

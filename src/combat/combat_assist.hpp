#pragma once
#include "rotation_engine.hpp"
#include "sync_layer.hpp"

namespace mcov::combat {
class DebugMailbox {
public:
    void Publish(const DebugSnapshot& state) { const std::scoped_lock lock(mutex_); state_=state; }
    DebugSnapshot Read() const { const std::scoped_lock lock(mutex_); return state_; }
private:
    mutable std::mutex mutex_;
    DebugSnapshot state_{};
};
// Step besitzt einen einzigen Owner-Thread. Nur Config, Freunde, Queue und HUD-Mailbox sind geteilt.
class CombatAssist {
public:
    CombatAssist(ConfigStore& config,FriendEnemyBook& relations,Transport& transport)
        : config_(config),relations_(relations),transport_(transport) {}
    CombatAssist(const CombatAssist&)=delete;
    CombatAssist& operator=(const CombatAssist&)=delete;
    DebugSnapshot Debug() const { return debug_.Read(); }
    DebugSnapshot Step(const Frame& f) {
        const auto version=config_.Read(); const Config& c=version.values;
        if(version.revision!=revision_) {
            sync_.Configure(c.sync,c.seed^kSyncSeed);
            selector_.Reset(); rotation_.Reset(f.observedRotation); attack_.Reset(); lastTarget_={};
            if(!seeded_ || c.seed!=seed_) {
                selectionRandom_.Seed(c.seed^kSelectionSeed); rotationRandom_.Seed(c.seed^kRotationSeed); attackRandom_.Seed(c.seed^kAttackSeed);
                seed_=c.seed; seeded_=true;
            }
            revision_=version.revision;
        }
        DebugSnapshot d;
        d.cooldown=f.attackCooldown.value_or(0); d.cooldownUnavailable=!f.attackCooldown;
        d.silentUnavailable=c.attack.silentRotation && !f.silentSupported;
        const bool valid=std::isfinite(f.now) && std::isfinite(f.dt) && f.dt>=0 && f.world &&
            Finite(f.eye) && Finite(f.playerPosition) && Finite(f.observedRotation) && Finite(f.cameraRotation) &&
            Within(f.observedRotation.pitch,-90,90) && Within(f.cameraRotation.pitch,-90,90) &&
            Within(f.verticalFovDeg,1,179) && Within(f.viewportHeight,1,16384) && Fresh(f.sampledAt,f.now,c.frameMaxAge);
        const bool worldChange=f.world!=world_ || (valid && f.now<lastTime_);
        if(worldChange) {
            world_=f.world; sync_.ResetWorld(); selector_.Reset(); rotation_.Reset(f.observedRotation);
            attack_.Reset(); lastTarget_={}; traceCount_=traceCursor_=0;
        }
        if(valid) lastTime_=f.now;
        d.enabled=c.enabled && valid && f.inputAllowed;
        if(d.enabled) {
            const auto previousCorrections=sync_.Stats().corrections;
            sync_.Observe(f);
            if(sync_.Stats().corrections!=previousCorrections) {
                rotation_.Reset(f.observedRotation); attack_.Reset();
            }
            d.target=selector_.Select(f,c.target,relations_.Read(),selectionRandom_);
            if(d.target) {
                if(!lastTarget_ || lastTarget_->id!=d.target->id || lastTarget_->uuid!=d.target->uuid) {
                    sync_.Cancel(); attack_.Reset();
                }
                lastTarget_=d.target;
                d.resyncing=sync_.Recovering(f.now);
                const Angles wanted=c.rotation.enabled ? rotation_.Step(f,*d.target,c.rotation,rotationRandom_,d.resyncing ? c.sync.recoverySpeedScale : 1) : f.observedRotation;
                d.current=f.observedRotation; d.error=AngularDistance(f.observedRotation,d.target->desired);
                d.reacting=rotation_.Reacting(f.now); d.fovRadiusPx=FovRadius(f,c.target);
                Command command;
                command.world=f.world; command.target=d.target->id; command.identity=d.target->uuid;
                command.sampledAt=f.sampledAt; command.pose={f.playerPosition,wanted}; command.silent=c.attack.silentRotation;
                if(!d.silentUnavailable && c.sync.enabled) {
                    if(c.rotation.enabled && !d.reacting) sync_.Enqueue(command,f.now);
                    if(!d.resyncing && attack_.Prepare(f,*d.target,c,attackRandom_)) {
                        command.kind=CommandKind::Attack;
                        if(!sync_.Enqueue(command,f.now)) attack_.Discarded();
                    }
                }
                const auto delivered=sync_.Pump(f,d.target,c,transport_);
                if(delivered.attackSent) attack_.Sent(); else if(delivered.attackDiscarded) attack_.Discarded();
                trace_[traceCursor_]={f.now,d.target->desired,f.observedRotation};
                traceCursor_=(traceCursor_+1)%kTraceCapacity; traceCount_=std::min(traceCount_+1,kTraceCapacity);
            } else Stop(f.observedRotation);
        } else Stop(valid ? f.observedRotation : Angles{});
        d.packets=sync_.Stats();
        d.traceCount=traceCount_;
        for(std::size_t i=0;i<traceCount_;++i) d.trace[i]=trace_[(traceCursor_+kTraceCapacity-traceCount_+i)%kTraceCapacity];
        debug_.Publish(d); return d;
    }
private:
    void Stop(Angles rotation) { sync_.Cancel(); selector_.Reset(); rotation_.Reset(rotation); attack_.Reset(); lastTarget_={}; }
    static constexpr std::uint64_t kSyncSeed=0x53594e43, kSelectionSeed=0x54415247, kRotationSeed=0x524f5441, kAttackSeed=0x4154544b;
    ConfigStore& config_;
    FriendEnemyBook& relations_;
    Transport& transport_;
    TargetSelector selector_{};
    RotationEngine rotation_{};
    AttackController attack_{};
    SyncLayer sync_{};
    Random selectionRandom_{},rotationRandom_{},attackRandom_{};
    DebugMailbox debug_{};
    std::optional<Target> lastTarget_;
    std::array<TracePoint,kTraceCapacity> trace_{};
    std::size_t traceCount_{},traceCursor_{};
    std::uint64_t revision_{~std::uint64_t{}},seed_{},world_{};
    Seconds lastTime_{};
    bool seeded_{};
};
} // namespace mcov::combat

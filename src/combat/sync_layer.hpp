#pragma once
#include "attack_controller.hpp"

namespace mcov::combat {
// Ein Adapter führt den Auftrag synchron auf seinem Client-Thread aus.
// true bedeutet "vom Transport angenommen", niemals "vom Server bestätigt".
class Transport {
public:
    virtual ~Transport()=default;
    virtual bool Send(const Command& command)=0;
    // Nur nach synchron angewendeter Pose: Rotation, Cooldown und Hit-Ray neu
    // lesen. Ein Adapter ohne Rückmeldung lässt den ursprünglichen Frame stehen.
    virtual void RefreshAfterPose(Frame&) {}
};
struct PumpResult { bool poseSent{},attackSent{},attackDiscarded{}; };
class SyncLayer {
public:
    void Configure(const SyncConfig& c,std::uint64_t seed,ExecutionMode mode=ExecutionMode::Smoothed) {
        const std::scoped_lock consumer(dispatchMutex_);
        const std::scoped_lock lock(mutex_);
        config_=c; mode_=mode; rng_.Seed(seed); ClearLocked(); lastDue_=0;
    }
    void Cancel() {
        const std::scoped_lock consumer(dispatchMutex_);
        const std::scoped_lock lock(mutex_); ClearLocked(); lastDue_=0;
    }
    void ResetWorld() {
        const std::scoped_lock consumer(dispatchMutex_);
        const std::scoped_lock lock(mutex_);
        ClearLocked(); lastDue_=0; correctionSerial_=0; recoveringUntil_=0;
        dispatchTick_=~std::uint64_t{}; dispatchWorld_=0; dispatchedYaw_=dispatchedPitch_=0;
    }
    bool Enqueue(Command command,Seconds now) {
        const std::scoped_lock lock(mutex_);
        if(!config_.enabled || !command.target || !std::isfinite(now) || !Finite(command.pose.position) || !Finite(command.pose.rotation)) return false;
        // Nur zusammenhängende Zustandsaufträge ersetzen. Niemals über einen Angriff hinweg.
        if(count_ && command.kind==CommandKind::Pose) {
            auto& back=queue_[(head_+count_-1)%kQueueCapacity];
            if(back.kind==CommandKind::Pose && back.world==command.world && back.target==command.target && back.identity==command.identity) {
                const auto sequence=back.sequence; const auto due=back.due;
                back=command; back.sequence=sequence; back.due=due; ++stats_.coalesced; return true;
            }
        }
        if(count_==kQueueCapacity) { ++stats_.discarded; return false; }
        command.sequence=++sequence_;
        // Der Angriff folgt der Rotation im selben Batch; eine zweite Warteperiode würde ihn veralten lassen.
        const double delay=command.kind==CommandKind::Pose && mode_==ExecutionMode::Smoothed ? rng_.Sample(config_.interval) : 0;
        command.due=std::max(now,lastDue_)+delay;
        lastDue_=command.due;
        queue_[(head_+count_)%kQueueCapacity]=command; ++count_; return true;
    }
    // Genau ein Consumer. Producer können während Send weiterhin Daten puffern.
    PumpResult Pump(const Frame& frame,const std::optional<Target>& target,const Config& config,Transport& transport) {
        const std::scoped_lock consumer(dispatchMutex_);
        PumpResult result;
        for(;;) {
            Command command;
            SyncConfig settings;
            ExecutionMode mode;
            Seconds recoveringUntil;
            {
                const std::scoped_lock lock(mutex_);
                if(!count_ || queue_[head_].due>frame.now) break;
                command=queue_[head_]; head_=(head_+1)%kQueueCapacity; --count_; settings=config_; mode=mode_; recoveringUntil=recoveringUntil_;
            }
            const double maxAge=command.kind==CommandKind::Attack ? settings.attackMaxAge : settings.poseMaxAge;
            const bool valid=config.enabled && settings.enabled && frame.inputAllowed && target &&
                Finite(frame.observedRotation) && Within(frame.observedRotation.pitch,-90,90) &&
                command.world==frame.world && command.target==target->id && command.identity==target->uuid &&
                Fresh(frame.sampledAt,frame.now,config.frameMaxAge) && Fresh(command.sampledAt,frame.now,maxAge) &&
                (!command.silent || frame.silentSupported) &&
                (command.kind!=CommandKind::Attack || CanAttack(frame,*target,config));
            Angles dispatchedDelta{};
            if(valid && command.kind==CommandKind::Pose && mode==ExecutionMode::Smoothed) {
                if(dispatchTick_!=frame.tick || dispatchWorld_!=frame.world) {
                    dispatchTick_=frame.tick; dispatchWorld_=frame.world; dispatchedRotation_=Normalize(frame.observedRotation);
                    dispatchedYaw_=dispatchedPitch_=0;
                }
                // Auch coalescte/verzögerte Posen dürfen das reale Tick-Budget nicht bündeln.
                const double scale=frame.now<recoveringUntil ? settings.recoverySpeedScale : 1;
                const double yawCap=std::max(0.0,config.rotation.maxYawPerTick*scale-dispatchedYaw_);
                const double pitchCap=std::max(0.0,config.rotation.maxPitchPerTick*scale-dispatchedPitch_);
                dispatchedDelta={std::clamp(Wrap(command.pose.rotation.yaw-dispatchedRotation_.yaw),-yawCap,yawCap),
                    std::clamp(command.pose.rotation.pitch-dispatchedRotation_.pitch,-pitchCap,pitchCap)};
                command.pose.rotation=Normalize({dispatchedRotation_.yaw+dispatchedDelta.yaw,dispatchedRotation_.pitch+dispatchedDelta.pitch});
            }
            bool sent=false;
            if(valid) { try { sent=transport.Send(command); } catch(...) { sent=false; } }
            {
                const std::scoped_lock lock(mutex_);
                if(sent) ++stats_.sent; else { ++stats_.discarded; if(valid) ++stats_.failed; }
            }
            if(sent && command.kind==CommandKind::Pose && mode==ExecutionMode::Smoothed) {
                dispatchedRotation_=command.pose.rotation;
                dispatchedYaw_+=std::abs(dispatchedDelta.yaw); dispatchedPitch_+=std::abs(dispatchedDelta.pitch);
            }
            if(sent && command.kind==CommandKind::Pose) result.poseSent=true;
            if(command.kind==CommandKind::Attack) { result.attackSent|=sent; result.attackDiscarded|=!sent; }
        }
        return result;
    }
    void Observe(const Frame& f) {
        const std::scoped_lock consumer(dispatchMutex_);
        const std::scoped_lock lock(mutex_);
        if(!f.correction) return;
        const auto& s=*f.correction;
        if(s.world!=f.world || s.serial<=correctionSerial_ || !Fresh(s.sampledAt,f.now,config_.correctionMaxAge) ||
           !Finite(s.server.position) || !Finite(s.expectedAtSameTime.position) ||
           !Finite(s.server.rotation) || !Finite(s.expectedAtSameTime.rotation)) return;
        correctionSerial_=s.serial;
        if(Length(Difference(s.server.position,s.expectedAtSameTime.position))>config_.positionDesync ||
           AngularDistance(s.server.rotation,s.expectedAtSameTime.rotation)>config_.angleDesync) {
            recoveringUntil_=f.now+config_.recoverySeconds; ++stats_.corrections; ClearLocked(); lastDue_=0;
            // Die Vanilla-Korrektur ist schon angewendet: neue Basis, unverbrauchtes Budget bleibt erhalten.
            if(Finite(f.observedRotation)) dispatchedRotation_=Normalize(f.observedRotation);
        }
    }
    bool Recovering(Seconds now) const { const std::scoped_lock lock(mutex_); return now<recoveringUntil_; }
    PacketStats Stats() const { const std::scoped_lock lock(mutex_); auto s=stats_; s.buffered=count_; return s; }
private:
    void ClearLocked() { stats_.discarded+=count_; count_=0; head_=0; }
    mutable std::mutex mutex_;
    std::mutex dispatchMutex_;
    std::array<Command,kQueueCapacity> queue_{};
    std::size_t head_{},count_{};
    std::uint64_t sequence_{},correctionSerial_{};
    Seconds lastDue_{},recoveringUntil_{};
    SyncConfig config_{};
    ExecutionMode mode_{ExecutionMode::Smoothed};
    Random rng_{};
    PacketStats stats_{};
    // Nur unter dispatchMutex_: bestätigte Ausgabe, unabhängig vom geplanten Engine-Zustand.
    std::uint64_t dispatchTick_{~std::uint64_t{}},dispatchWorld_{};
    Angles dispatchedRotation_{};
    double dispatchedYaw_{},dispatchedPitch_{};
};
} // namespace mcov::combat

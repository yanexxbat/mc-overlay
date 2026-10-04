#pragma once
#include "combat_assist.hpp"

namespace mcov::combat {
// Vollständig ausführbarer Referenzadapter. Kein Zugriff auf Minecraft, JNI oder Sockets.
class SimulatedGame final : public Transport {
public:
    static constexpr std::size_t kEntities=4, kCommands=1024;
    static constexpr double kAttackPeriod=0.600, kDamage=4.0, kEyeHeight=1.620, kReach=3.0;
    SimulatedGame() {
        PutEntity(0,1,"reference_target",{0.5,0,2.6},20);
        PutEntity(1,2,"low_health",{-1,0,3.5},8);
        PutEntity(2,3,"friend",{0,0,2.0},20);
    }
    void PutEntity(std::size_t index,std::uint32_t id,const char* name,Vec3 base,double health) {
        if(index>=kEntities) return;
        Entity e;
        e.id=id; e.uuid={0x4d434f56,id}; e.health=health; e.player=e.living=true;
        for(std::size_t i=0; i+1<e.name.size() && name[i]; ++i) e.name[i]=name[i];
        e.boundsMin={base.x-0.3,base.y,base.z-0.3}; e.boundsMax={base.x+0.3,base.y+1.8,base.z+0.3};
        e.visibility=Visibility{true,0}; entities_[index]=e;
        count_=std::max(count_,index+1);
    }
    Frame Capture(Seconds now,Seconds dt) {
        now_=now;
        for(std::size_t i=0;i<count_;++i) if(entities_[i].visibility) entities_[i].visibility->sampledAt=now;
        const Angles rotation=serverRotation_;
        Frame f;
        f.now=f.sampledAt=now; f.dt=dt; f.world=world_; f.tick=static_cast<std::uint64_t>(now/0.050);
        f.eye={position_.x,position_.y+kEyeHeight,position_.z}; f.playerPosition=position_; f.observedRotation=rotation;
        f.cameraRotation=cameraRotation_;
        f.entities={entities_.data(),count_}; f.crosshairId=Raycast(f.eye,cameraRotation_); f.attackHitId=Raycast(f.eye,rotation);
        f.attackCooldown=std::clamp((now-lastAttack_)/kAttackPeriod,0.0,1.0); f.gameAttackReach=kReach;
        f.inputAllowed=f.silentSupported=true; return f;
    }
    bool Send(const Command& command) override {
        if(command.world!=world_ || command.sequence<=lastSequence_ || (command.silent && !silentSupported_)) return false;
        if(logCount_==kCommands) return false;
        if(command.kind==CommandKind::Pose) {
            // Silent trennt die Kameradarstellung vom Zustand des simulierten Servers.
            serverPosition_=command.pose.position;
            serverRotation_=Normalize(command.pose.rotation);
            if(!command.silent) cameraRotation_=serverRotation_;
        } else {
            const Vec3 eye{position_.x,position_.y+kEyeHeight,position_.z};
            if(now_-lastAttack_<kAttackPeriod || Raycast(eye,serverRotation_)!=command.target) return false;
            for(auto& e:entities_) if(e.id==command.target && e.uuid==command.identity) {
                e.health=std::max(0.0,e.health-kDamage); lastAttack_=now_; ++attacks_; break;
            }
        }
        lastSequence_=command.sequence; log_[logCount_++]=command; return true;
    }
    std::span<const Command> Commands() const { return {log_.data(),logCount_}; }
    std::uint64_t Attacks() const { return attacks_; }
    Angles Camera() const { return cameraRotation_; }
    Angles ServerRotation() const { return serverRotation_; }
    Pose ServerPose() const { return {serverPosition_,serverRotation_}; }
    std::array<Entity,kEntities>& Entities() { return entities_; }
private:
    std::uint32_t Raycast(Vec3 eye,Angles rotation) const {
        const auto direction=Direction(rotation);
        double nearest=kReach; std::uint32_t id=0;
        for(std::size_t i=0;i<count_;++i) {
            const auto& e=entities_[i];
            if(e.health<=0 || !e.visibility || !e.visibility->visible) continue;
            double lo=0,hi=kReach;
            const std::array<double,3> origin{eye.x,eye.y,eye.z}, ray{direction.x,direction.y,direction.z},
                minimum{e.boundsMin.x,e.boundsMin.y,e.boundsMin.z}, maximum{e.boundsMax.x,e.boundsMax.y,e.boundsMax.z};
            for(std::size_t axis=0;axis<3;++axis) {
                if(std::abs(ray[axis])<std::numeric_limits<double>::epsilon()) {
                    if(origin[axis]<minimum[axis] || origin[axis]>maximum[axis]) { hi=-1; break; }
                } else {
                    const double a=(minimum[axis]-origin[axis])/ray[axis],b=(maximum[axis]-origin[axis])/ray[axis];
                    lo=std::max(lo,std::min(a,b)); hi=std::min(hi,std::max(a,b));
                }
            }
            if(lo<=hi && lo<nearest) { nearest=lo; id=e.id; }
        }
        return id;
    }
    std::array<Entity,kEntities> entities_{};
    std::array<Command,kCommands> log_{};
    std::size_t count_{},logCount_{};
    std::uint64_t attacks_{},lastSequence_{},world_{1};
    Seconds now_{},lastAttack_{-kAttackPeriod};
    Vec3 position_{},serverPosition_{};
    Angles cameraRotation_{},serverRotation_{};
    bool silentSupported_{true};
};
} // namespace mcov::combat

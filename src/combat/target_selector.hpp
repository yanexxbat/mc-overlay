#pragma once
#include "config.hpp"

namespace mcov::combat {
enum class Relation { Neutral, Friend, Enemy };
struct RelationEntry { Identity uuid{}; Relation relation{Relation::Neutral}; bool used{}; };
using Relations = std::array<RelationEntry,kRelationCapacity>;
class FriendEnemyBook {
public:
    bool Set(Identity id, Relation relation) {
        const std::scoped_lock lock(mutex_);
        for (auto& e:entries_) if(e.used && e.uuid==id) { e.relation=relation; e.used=relation!=Relation::Neutral; return true; }
        if(relation==Relation::Neutral) return true;
        for (auto& e:entries_) if(!e.used) { e={id,relation,true}; return true; }
        return false; // Volles Buch überschreibt keine Freunde.
    }
    Relations Read() const { const std::scoped_lock lock(mutex_); return entries_; }
private:
    mutable std::mutex mutex_;
    Relations entries_{};
};
inline Relation Lookup(const Relations& book, Identity id) {
    for(const auto& e:book) if(e.used && e.uuid==id) return e.relation;
    return Relation::Neutral;
}
inline double ConeDegrees(const Frame& f, const TargetConfig& c) {
    if(c.fovMode==FovMode::DynamicDegrees) return c.coneDegrees;
    const double focal=f.viewportHeight/(2*std::tan(f.verticalFovDeg/(2*kDegrees)));
    return std::atan(c.radiusPixels/focal)*kDegrees;
}
inline double FovRadius(const Frame& f, const TargetConfig& c) {
    if(c.mode==TargetMode::NearestEntity) return 0; // Auswahl rundherum, unabhängig von der Kamera.
    if(c.fovMode==FovMode::StaticPixels) return c.radiusPixels;
    // Ein 180-Grad-Halbkegel umfasst die ganze Umgebung, keinen projizierbaren Kreis.
    if(c.coneDegrees>=90) return 0;
    return f.viewportHeight*std::tan(c.coneDegrees/kDegrees)/(2*std::tan(f.verticalFovDeg/(2*kDegrees)));
}
inline bool Fresh(Seconds sampled, Seconds now, Seconds age) {
    return std::isfinite(sampled)&&sampled<=now&&now-sampled<=age;
}
inline std::optional<Target> Evaluate(const Entity& e, const Frame& f, const TargetConfig& c, const Relations& book) {
    const auto relation=Lookup(book,e.uuid);
    if(!e.id || e.self || !e.living || !std::isfinite(e.health) || e.health<=0 || relation==Relation::Friend ||
       ((c.playersOnly || c.mode==TargetMode::NearestPlayer) && !e.player) ||
       !Finite(e.boundsMin) || !Finite(e.boundsMax) || e.boundsMax.x<e.boundsMin.x ||
       e.boundsMax.y<e.boundsMin.y || e.boundsMax.z<e.boundsMin.z) return {};
    if(c.requireVisibility && (!e.visibility || !e.visibility->visible || !Fresh(e.visibility->sampledAt,f.now,c.visibilityMaxAge))) return {};
    Vec3 nearest{std::clamp(f.eye.x,e.boundsMin.x,e.boundsMax.x),std::clamp(f.eye.y,e.boundsMin.y,e.boundsMax.y),std::clamp(f.eye.z,e.boundsMin.z,e.boundsMax.z)};
    const double range=Length(Difference(nearest,f.eye));
    const Vec3 aim{std::midpoint(e.boundsMin.x,e.boundsMax.x),e.boundsMin.y+(e.boundsMax.y-e.boundsMin.y)*c.aimHeight,std::midpoint(e.boundsMin.z,e.boundsMax.z)};
    if(!Finite(aim)) return {};
    const Angles desired=LookAt(f.eye,aim);
    const double angle=AngularDistance(f.cameraRotation,desired), cone=ConeDegrees(f,c);
    if(!std::isfinite(range) || !std::isfinite(angle) || !std::isfinite(cone) || range>c.range ||
       (c.mode!=TargetMode::NearestEntity && angle>cone) ||
       (c.mode==TargetMode::Crosshair && f.crosshairId!=e.id)) return {};
    double score=(c.angleWeight*(angle/cone)+c.distanceWeight*(range/c.range))/(c.angleWeight+c.distanceWeight);
    if(c.mode==TargetMode::NearestPlayer || c.mode==TargetMode::NearestEntity) score=range/c.range;
    // Monotoner HP-Term ohne Annahme, dass modifizierte Entities maximal 20 HP besitzen.
    if(c.mode==TargetMode::LowestHealth) score=e.health/(e.health+1);
    return Target{e.id,e.uuid,e.name,aim,desired,range,angle,score,relation==Relation::Enemy};
}
class TargetSelector {
public:
    void Reset() { current_={}; pending_={}; pendingSince_=0; delay_=0; }
    std::optional<Target> Select(const Frame& f, const TargetConfig& c, const Relations& book, Random& rng) {
        if(!c.enabled) { Reset(); return {}; }
        std::optional<Target> best, current;
        for(const Entity& e:f.entities) if(auto t=Evaluate(e,f,c,book)) {
            if(current_ && t->id==current_->id && t->uuid==current_->uuid) current=t;
            if(!best || Better(*t,*best,c)) best=t;
        }
        // Kill Aura nutzt pro Tick die nächste gültige Hitbox, ohne alten Lock oder Wechselverzögerung.
        if(c.mode==TargetMode::NearestEntity) { current_=best; pending_={}; return current_; }
        if(!current) { current_=best; pending_={}; return current_; } // Verlust bricht den alten Lock sofort ab.
        if(!best || best->id==current->id || !c.multiTarget) { current_=current; pending_={}; return current_; }
        const bool promoted=c.prioritizeEnemies && best->enemy && !current->enemy;
        if(!promoted && (!Better(*best,*current,c) || current->score-best->score<c.switchImprovement)) { pending_={}; current_=current; return current_; }
        if(!pending_ || pending_->id!=best->id || pending_->uuid!=best->uuid) {
            pending_=best; pendingSince_=f.now; delay_=rng.Sample(c.switchDelay);
        }
        current_=(f.now-pendingSince_>=delay_) ? best : current;
        if(current_->id==best->id) pending_={};
        return current_;
    }
private:
    static bool Better(const Target& a, const Target& b, const TargetConfig& c) {
        if(c.mode==TargetMode::NearestEntity) return a.range!=b.range ? a.range<b.range : a.id<b.id;
        if(c.prioritizeEnemies && a.enemy!=b.enemy) return a.enemy;
        if(a.score!=b.score) return a.score<b.score;
        if(a.range!=b.range) return a.range<b.range;
        if(a.angle!=b.angle) return a.angle<b.angle;
        return a.id<b.id;
    }
    std::optional<Target> current_,pending_;
    Seconds pendingSince_{},delay_{};
};
} // namespace mcov::combat

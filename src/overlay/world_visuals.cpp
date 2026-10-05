#include "world_visuals.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>

namespace mcov {
namespace {
constexpr double kSnapshotTimeout=.35,kTeleportDistance=8,kSampleInterval=1./30.;
constexpr double kSampleDistance=.018,kHealthThreshold=.01;
constexpr float kAmbientLifetime=8.f;
float Safe(float value,float fallback,float low,float high) {
    return std::clamp(std::isfinite(value)?value:fallback,low,high);
}
bool Finite(WorldPoint p) { return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z); }
double Distance(WorldPoint a,WorldPoint b) {
    const double x=a.x-b.x,y=a.y-b.y,z=a.z-b.z; return std::sqrt(x*x+y*y+z*z);
}
ImVec4 Mix(ImVec4 a,ImVec4 b,float t) {
    return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,a.w+(b.w-a.w)*t};
}
ImU32 Colour(ImVec4 c,float alpha) {
    c.x=Safe(c.x,1,0,1); c.y=Safe(c.y,1,0,1); c.z=Safe(c.z,1,0,1);
    c.w=Safe(c.w,1,0,1)*std::clamp(alpha,0.f,1.f); return ImGui::ColorConvertFloat4ToU32(c);
}
ImVec4 Palette(const EffectPalette& palette,ImVec4 start,ImVec4 end,float t) {
    return palette.auraColours ? Mix(start,end,t) : Mix(palette.start,palette.end,t);
}
ImVec2 Pixel(ScreenPoint p,ImVec2 origin) { return {p.x+origin.x,p.y+origin.y}; }
bool Visible(ScreenPoint p,float radius,ImVec2 view) {
    return radius>0&&p.x+radius>=0&&p.y+radius>=0&&p.x-radius<=view.x&&p.y-radius<=view.y;
}
struct TextureScope {
    ImDrawList& draw; bool active;
    TextureScope(ImDrawList& list,ImTextureID texture):draw(list),active(texture!=0) {
        if(active) draw.PushTextureID(texture);
    }
    ~TextureScope() { if(active) draw.PopTextureID(); }
};
void Sprite(ImDrawList& draw,ImTextureID texture,ImVec2 p,float size,ImVec4 colour,float alpha) {
    draw.AddImage(texture,{p.x-size,p.y-size},{p.x+size,p.y+size},{0,0},{1,1},Colour(colour,alpha));
}
// Dieselbe Gauß-Textur und Kern-/Nebel-Schichtung wie bei der Target Aura.
void Halo(ImDrawList& draw,ImVec2 p,float size,ImVec4 colour,float alpha,ImTextureID texture,float intensity) {
    if(!texture||intensity<=0) return;
    Sprite(draw,texture,p,size*4,colour,alpha*.45f*intensity);
    Sprite(draw,texture,p,size*2,colour,alpha*.22f*intensity);
}
void Orb(ImDrawList& draw,ImVec2 p,float size,ImVec4 colour,float alpha,ImTextureID glow,float intensity) {
    TextureScope texture(draw,glow);
    Halo(draw,p,size,colour,alpha,glow,intensity);
    if(glow) {
        Sprite(draw,glow,p,size,Mix(colour,{1,1,1,colour.w},.88f),alpha*.92f);
        Sprite(draw,glow,p,size*.6f,{1,1,1,colour.w},alpha*.70f);
    }
    else {
        draw.AddCircleFilled(p,size*2.5f,Colour(colour,alpha*.055f),12);
        draw.AddCircleFilled(p,size*1.7f,Colour(colour,alpha*.12f),12);
    }
    if(!glow) {
        draw.AddCircleFilled(p,size,Colour(colour,alpha*.3f),20);
        draw.AddCircleFilled(p,size*.25f,Colour(Mix(colour,{1,1,1,colour.w},.75f),alpha*.85f),16);
    }
}
void Star(ImDrawList& draw,ImVec2 p,float size,float angle,ImVec4 colour,float alpha,ImTextureID glow,float intensity) {
    {
        TextureScope texture(draw,glow);
        Halo(draw,p,size,colour,alpha,glow,intensity);
        if(glow) Sprite(draw,glow,p,size*.8f,Mix(colour,{1,1,1,colour.w},.8f),alpha*.8f);
    }
    const float c=std::cos(angle),s=std::sin(angle);
    for(int axis=0;axis<2;++axis) {
        const std::array<ImVec2,4> local={ImVec2{-size,0},{0,-size*.18f},{size,0},{0,size*.18f}};
        std::array<ImVec2,4> points;
        for(int i=0;i<4;++i) {
            const float x=axis ? -local[i].y : local[i].x,y=axis ? local[i].x : local[i].y;
            points[i]={p.x+x*c-y*s,p.y+x*s+y*c};
        }
        draw.AddConvexPolyFilled(points.data(),4,Colour(Mix(colour,{1,1,1,colour.w},.5f),alpha));
    }
}
void GlowLine(ImDrawList& draw,ImVec2 a,ImVec2 b,float width,ImVec4 colour,float alpha,float intensity=1) {
    draw.AddLine(a,b,Colour(colour,alpha*.05f*intensity),width*7);
    draw.AddLine(a,b,Colour(colour,alpha*.16f*intensity),width*3);
    draw.AddLine(a,b,Colour(colour,alpha),width);
}
// Der normale ImGui-Zustand wird auch nach leeren/geclippten Effekten wiederhergestellt.
struct BlendScope {
    ImDrawList& draw; bool active;
    BlendScope(ImDrawList& list,AuraBlend blend):draw(list),active(blend.begin!=nullptr) {
        if(active) draw.AddCallback(blend.begin,blend.data);
    }
    ~BlendScope() { if(active) draw.AddCallback(ImDrawCallback_ResetRenderState,nullptr); }
};
struct FillScope {
    ImDrawList& draw; ImDrawListFlags flags;
    explicit FillScope(ImDrawList& list):draw(list),flags(list.Flags) { draw.Flags&=~ImDrawListFlags_AntiAliasedFill; }
    ~FillScope() { draw.Flags=flags; }
};
}
WorldVisualsRenderer::WorldVisualsRenderer() noexcept {
    for(int i=0;i<kRingSegments;++i) {
        const double angle=2*std::numbers::pi*i/kRingSegments;
        circle_[i]={float(std::cos(angle)),float(std::sin(angle))};
    }
}
void WorldVisualsRenderer::Prepare(ImDrawList& draw) {
    draw.VtxBuffer.reserve(std::max(draw.VtxBuffer.Capacity,450000));
    draw.IdxBuffer.reserve(std::max(draw.IdxBuffer.Capacity,850000));
    draw.CmdBuffer.reserve(std::max(draw.CmdBuffer.Capacity,8192));
    draw._Path.reserve(std::max(draw._Path.Capacity,64));
    draw._TextureIdStack.reserve(std::max(draw._TextureIdStack.Capacity,4));
}
void WorldVisualsRenderer::Clear() {
    for(auto& track:tracks_) track={};
    for(auto& particle:hits_) particle={};
    for(auto& particle:ambient_) particle={};
    for(auto& particle:kills_) particle={};
    for(auto& ring:rings_) ring={};
    crumbCount_=crumbHead_=0;
}
void WorldVisualsRenderer::Reset() {
    if(initialized_) Clear();
    initialized_=false; frame_=serial_=0; lastFrameAt_=now_=0;
    random_=0x4D434F56; target_=0;
}
float WorldVisualsRenderer::Random() {
    random_^=random_<<13; random_^=random_>>17; random_^=random_<<5;
    return float(random_>>8)/16777216.f;
}
WorldVisualsRenderer::Track* WorldVisualsRenderer::Observe(const EntitySnapshot& entity,bool self) {
    Track* free=nullptr;
    for(auto& track:tracks_) {
        if(track.valid&&track.self==self&&track.id==entity.entityId) {
            if(track.uuid!=entity.uuid) { track={}; free=&track; break; }
            return &track;
        }
        if(!track.valid||now_-track.seen>2.) free=&track;
    }
    // Innerhalb eines Snapshots wird kein bereits beobachteter Track verdrängt.
    if(!free) for(auto& track:tracks_) if(track.serial!=serial_&&(!free||track.seen<free->seen)) free=&track;
    if(!free) return nullptr;
    *free={}; free->valid=true; free->self=self; free->id=entity.entityId; free->uuid=entity.uuid;
    return free;
}
bool WorldVisualsRenderer::Subject(const Track& track,TrailSubjects scope,std::uint32_t target) {
    switch(scope) {
        case TrailSubjects::Self:return track.self;
        case TrailSubjects::Players:return track.self||(track.flags&kEntityIsPlayer);
        case TrailSubjects::Target:return !track.self&&target&&track.id==target;
        case TrailSubjects::Living:return track.self||(track.flags&kEntityIsLiving);
    }
    return false;
}
void WorldVisualsRenderer::Burst(WorldPoint position,const HitParticleConfig& config) {
    int left=std::clamp(config.amount,1,24);
    const float speed=Safe(config.speed,1.6f,.1f,4.f);
    for(auto& particle:hits_) if(!particle.valid&&left>0) {
        const float angle=Random()*2*std::numbers::pi_v<float>,rise=Random()*.8f+.2f;
        const float spread=(.35f+Random()*.65f)*speed;
        particle={true,position,{std::cos(angle)*spread,rise*speed,std::sin(angle)*spread},now_,Random()*6.28f,Random()};
        --left;
    }
}
void WorldVisualsRenderer::KillBurst(WorldPoint position,const KillEffectConfig& config) {
    int left=std::clamp(config.amount,8,48);
    const float speed=Safe(config.speed,2,.1f,4);
    const float spreadRadius=Safe(config.radius,1.2f,.3f,3)*.25f;
    for(auto& p:kills_) if(!p.valid&&left>0) {
        const float angle=Random()*2*std::numbers::pi_v<float>,spread=(.4f+Random()*.6f)*speed;
        const float offset=spreadRadius*(.5f+Random()*.5f);
        // Verteilte Startpunkte verhindern einen ausgebrannten weißen Fleck bei großen Bursts.
        const WorldPoint start{position.x+std::cos(angle)*offset,position.y+(Random()-.5f)*spreadRadius*3,position.z+std::sin(angle)*offset};
        p={true,start,{std::cos(angle)*spread,(.4f+Random())*speed,std::sin(angle)*spread},now_,Random()*6.28f,Random()};
        --left;
    }
}
void WorldVisualsRenderer::SpawnRing(WorldPoint position,const EntitySnapshot& e,bool kill) {
    for(auto& ring:rings_) if(!ring.valid) {
        ring={true,kill,position,now_,Random(),e.entityId,e.uuid}; return;
    }
}
void WorldVisualsRenderer::Update(const WorldSnapshot& s,double now,const WorldVisualsConfig& config,std::uint32_t targetId) {
    if(!std::isfinite(now)) return;
    const WorldPoint local{s.posX,s.posY,s.posZ};
    if(!Finite(local)||!std::isfinite(s.maxHealth)||s.maxHealth<=0) { Reset(); return; }
    const bool worldChanged=initialized_&&(std::memcmp(dimension_.data(),s.dimension,dimension_.size())||
        std::memcmp(server_.data(),s.serverAddress,server_.size())||s.frameIndex<frame_||now<now_||
        now-now_>.5||Distance(local,local_)>32);
    if(worldChanged) Reset();
    now_=now; target_=targetId;
    crumbLife_=Safe(config.crumbs.lifetime,2,.3f,4);
    if(!initialized_) {
        std::memcpy(dimension_.data(),s.dimension,dimension_.size());
        std::memcpy(server_.data(),s.serverAddress,server_.size());
        initialized_=true; frame_=s.frameIndex-1;
    }
    const float hitLife=Safe(config.hits.lifetime,.9f,.2f,3);
    for(auto& p:hits_) if(p.valid&&(!config.hits.enabled||now_-p.born>=hitLife)) p.valid=false;
    for(auto& p:kills_) if(p.valid&&(!config.kills.enabled||now_-p.born>=Safe(config.kills.lifetime,1.4f,.3f,3))) p.valid=false;
    for(auto& r:rings_) if(r.valid) {
        const bool enabled=r.kill?config.kills.enabled:config.bubbles.enabled;
        const float life=r.kill?Safe(config.kills.lifetime,1.4f,.3f,3):Safe(config.bubbles.lifetime,.75f,.2f,2);
        if(!enabled||now_-r.born>=life) r.valid=false;
    }
    if(!config.crumbs.enabled) crumbCount_=crumbHead_=0;
    if(s.frameIndex!=frame_) {
        frame_=s.frameIndex; lastFrameAt_=now_; ++serial_;
        const float range=Safe(config.maxDistance,64,4,128);
        auto observe=[&](const EntitySnapshot& e,bool self) {
            const WorldPoint point{e.x,e.y,e.z};
            if(!Finite(point)||!std::isfinite(e.health)||(!self&&Distance(point,local)>range)) return;
            auto* track=Observe(e,self); if(!track) return;
            const bool continuous=track->serial&&track->serial+1==serial_&&Distance(track->p,point)<kTeleportDistance;
            if(!continuous) { track->count=0; track->head=0; track->lastSample=0; }
            const bool damaged=continuous&&e.health>=0&&track->health>e.health+kHealthThreshold;
            if(config.hits.enabled&&!self&&(e.flags&kEntityIsLiving)&&damaged&&(!config.hits.targetOnly||e.entityId==target_))
                Burst({e.x,e.y+std::max(.1f,e.height)*.55,e.z},config.hits);
            if(!self&&(e.flags&kEntityIsLiving)&&damaged) {
                if(config.bubbles.enabled&&(!config.bubbles.targetOnly||e.entityId==target_))
                    SpawnRing({e.x,e.y+std::max(.1f,e.height)*.55,e.z},e,false);
                // Nur der echte Übergang von lebendig zu 0 HP; Verschwinden allein zählt nicht als Tod.
                if(config.kills.enabled&&track->health>0&&e.health==0&&
                   (!config.kills.targetOnly||e.entityId==target_||track->wasTarget)) {
                    SpawnRing({e.x,e.y+.04,e.z},e,true);
                    KillBurst({e.x,e.y+std::max(.1f,e.height)*.5,e.z},config.kills);
                }
            }
            track->flags=e.flags; track->health=e.health;
            if(config.trails.enabled&&e.health>0&&Subject(*track,config.trails.subjects,target_)) {
                const auto last=(track->head-1+kTrailSamples)%kTrailSamples;
                if(track->count==0||(now_-track->lastSample>=kSampleInterval&&Distance(track->samples[last].p,point)>=kSampleDistance)) {
                    track->samples[track->head]={point,now_}; track->head=(track->head+1)%kTrailSamples;
                    track->count=std::min(track->count+1,kTrailSamples); track->lastSample=now_;
                }
            } else track->count=0;
            if(self&&config.crumbs.enabled&&e.health>0) {
                if(!continuous) crumbCount_=crumbHead_=0;
                const int last=(crumbHead_-1+kCrumbs)%kCrumbs;
                const WorldPoint foot{point.x,point.y+.025,point.z};
                if(!crumbCount_||Distance(crumbs_[last].p,foot)>=Safe(config.crumbs.spacing,.25f,.1f,1)) {
                    crumbs_[crumbHead_]={foot,now_}; crumbHead_=(crumbHead_+1)%kCrumbs;
                    crumbCount_=std::min(crumbCount_+1,kCrumbs);
                }
            }
            track->wasTarget=!self&&e.entityId==target_;
            track->p=point; track->seen=now_; track->serial=serial_;
        };
        // Das Ziel bekommt bei voller Kapazität zuerst einen Platz; dann der eigene Spieler.
        const auto count=std::min(s.entityCount,kMaxEntities);
        for(std::uint32_t i=0;i<count;++i) if(target_&&s.entities[i].entityId==target_&&!(s.entities[i].flags&kEntityIsSelf)) observe(s.entities[i],false);
        const EntitySnapshot* body=nullptr;
        for(std::uint32_t i=0;i<count;++i) if(s.entities[i].flags&kEntityIsSelf) { body=&s.entities[i]; break; }
        EntitySnapshot self{}; self.x=local.x; self.y=local.y; self.z=local.z; self.health=s.health;
        self.height=1.8f; self.flags=kEntityIsSelf|kEntityIsLiving|kEntityIsPlayer;
        observe(body?*body:self,true);
        for(std::uint32_t i=0;i<count;++i) if(!(s.entities[i].flags&kEntityIsSelf)&&(!target_||s.entities[i].entityId!=target_)) observe(s.entities[i],false);
    } else if(now_-lastFrameAt_>kSnapshotTimeout) {
        // Ein alter Frame darf weder neue Treffer auslösen noch eine alte Welt weiter beleuchten.
        Clear(); local_=local; return;
    }
    local_=local;
    const int ambientCount=config.ambient.enabled ? std::clamp(config.ambient.amount,1,kAmbientParticles) : 0;
    const float radius=Safe(config.ambient.radius,7,2,16),speed=Safe(config.ambient.speed,.3f,0,2);
    for(int i=0;i<kAmbientParticles;++i) {
        auto& p=ambient_[i]; if(i>=ambientCount) { p.valid=false; continue; }
        const double age=now_-p.born;
        const float vertical=config.ambient.shape==AmbientShape::Snow ? -.5f : .1f;
        const WorldPoint moving{p.p.x+p.velocity.x*age*speed,p.p.y+vertical*age*speed,p.p.z+p.velocity.z*age*speed};
        const double dx=moving.x-local.x,dz=moving.z-local.z;
        if(p.valid&&age<kAmbientLifetime&&dx*dx+dz*dz<radius*radius*1.96&&moving.y>local.y-.5&&moving.y<local.y+5) continue;
        const float angle=Random()*2*std::numbers::pi_v<float>,distance=std::sqrt(Random())*radius;
        p={true,{local.x+std::cos(angle)*distance,local.y+.15+Random()*4.5,local.z+std::sin(angle)*distance},
            {Random()-.5f,.1f,Random()-.5f},now_,Random()*6.28f,Random()};
    }
}
WorldVisualStats WorldVisualsRenderer::Stats() const {
    WorldVisualStats stats;
    for(const auto& t:tracks_) if(t.valid&&t.count>1) ++stats.trails;
    for(const auto& p:hits_) stats.hitParticles+=p.valid;
    for(const auto& p:ambient_) stats.ambientParticles+=p.valid;
    for(const auto& p:kills_) stats.killParticles+=p.valid;
    for(const auto& r:rings_) if(r.valid) { if(r.kill) ++stats.kills; else ++stats.bubbles; }
    for(int i=0;i<crumbCount_;++i) if(now_-crumbs_[(crumbHead_-1-i+kCrumbs)%kCrumbs].born<crumbLife_) ++stats.crumbs;
    return stats;
}
void WorldVisualsRenderer::Draw(ImDrawList& draw,const CameraProjection& projection,ImVec2 viewport,
        const WorldVisualsConfig& config,ImVec4 start,ImVec4 end,AuraBlend blend,ImTextureID glow,ImVec2 origin,bool reduceMotion) const {
    if(!initialized_||viewport.x<1||viewport.y<1||!std::isfinite(viewport.x)||!std::isfinite(viewport.y)) return;
    draw.PushClipRect(origin,{origin.x+viewport.x,origin.y+viewport.y},true);
    {
        BlendScope blending(draw,blend);
        if(config.trails.enabled) for(const auto& track:tracks_) {
            if(!track.valid||track.count<2||!Subject(track,config.trails.subjects,target_)||
                Distance(track.p,local_)>Safe(config.maxDistance,64,4,128)) continue;
            const float life=Safe(config.trails.lifetime,1.2f,.2f,3),height=Safe(config.trails.height,.85f,.1f,2);
            for(int i=1;i<=track.count;++i) {
                const auto& a=track.samples[(track.head-track.count+i-1+kTrailSamples)%kTrailSamples];
                const auto b=i==track.count ? Sample{track.p,track.lastSample} : track.samples[(track.head-track.count+i+kTrailSamples)%kTrailSamples];
                if(Distance(a.p,b.p)<.001) continue;
                const float age=float(now_-b.born)/life;
                if(age<0||age>=1) continue;
                const float alpha=(1-age)*(1-age)*Safe(config.trails.opacity,.65f,0,1);
                const auto colour=Palette(config.trails.colours,start,end,age);
                const WorldPoint lowA{a.p.x,a.p.y+.04,a.p.z},lowB{b.p.x,b.p.y+.04,b.p.z};
                ScreenPoint pa,pb;
                if(config.trails.shape==TrailShape::Dots) {
                    if(i%4&&i!=track.count) continue;
                    float radius;
                    if(projection.Billboard({b.p.x,b.p.y+.25,b.p.z},Safe(config.trails.width,.035f,.01f,.15f)*1.5f,pb,radius)&&Visible(pb,radius*3,viewport))
                        Orb(draw,Pixel(pb,origin),std::clamp(radius,.5f,24.f),colour,alpha,glow,Safe(config.trails.glow,1,0,2));
                    continue;
                }
                if(config.trails.shape==TrailShape::Ribbon) {
                    ScreenPoint ta,tb;
                    if(projection.Point(lowA,pa)&&projection.Point(lowB,pb)&&
                       projection.Point({lowA.x,lowA.y+height,lowA.z},ta)&&projection.Point({lowB.x,lowB.y+height,lowB.z},tb)) {
                        const int first=draw.VtxBuffer.Size;
                        // Benachbarte Bandsegmente teilen Kanten ohne transparente AA-Nähte.
                        { FillScope fill(draw); draw.AddQuadFilled(Pixel(pa,origin),Pixel(pb,origin),Pixel(tb,origin),Pixel(ta,origin),Colour(colour,alpha*.3f)); }
                        // Oben läuft das Band weich aus; unten bleibt eine klare leuchtende Spur.
                        if(draw.VtxBuffer.Size>=first+4) {
                            draw.VtxBuffer[first+2].col=Colour(colour,alpha*.025f);
                            draw.VtxBuffer[first+3].col=Colour(colour,alpha*.025f);
                        }
                    }
                }
                if(projection.Segment(lowA,lowB,pa,pb)) {
                    ScreenPoint middle; float radius;
                    if(projection.Billboard({(lowA.x+lowB.x)*.5,(lowA.y+lowB.y)*.5,(lowA.z+lowB.z)*.5},
                        Safe(config.trails.width,.035f,.01f,.15f),middle,radius))
                        GlowLine(draw,Pixel(pa,origin),Pixel(pb,origin),std::clamp(radius,1.f,12.f),colour,alpha,Safe(config.trails.glow,1,0,2));
                }
            }
        }
        auto particle=[&](const Particle& p,WorldPoint position,float worldSize,ImVec4 colour,float alpha,bool star,float spin,float intensity) {
            ScreenPoint projected; float radius;
            if(!projection.Billboard(position,worldSize,projected,radius)||!Visible(projected,radius*4,viewport)) return;
            radius=std::clamp(radius,.4f,32.f);
            if(star) Star(draw,Pixel(projected,origin),radius,reduceMotion ? p.phase : spin,colour,alpha,glow,intensity);
            else Orb(draw,Pixel(projected,origin),radius,colour,alpha,glow,intensity);
        };
        if(config.hits.enabled) for(const auto& p:hits_) if(p.valid) {
            const float age=float(now_-p.born),life=Safe(config.hits.lifetime,.9f,.2f,3);
            const float t=std::clamp(age/life,0.f,1.f),motion=reduceMotion ? 0 : age;
            const WorldPoint position{p.p.x+p.velocity.x*motion,p.p.y+p.velocity.y*motion-Safe(config.hits.gravity,1.2f,0,5)*motion*motion*.5,
                p.p.z+p.velocity.z*motion};
            particle(p,position,Safe(config.hits.size,.065f,.02f,.2f),Palette(config.hits.colours,start,end,p.mix),
                (1-t)*(1-t)*Safe(config.hits.opacity,.9f,0,1),config.hits.shape==ParticleShape::Stars,p.phase+age*1.5f,Safe(config.hits.glow,1,0,2));
        }
        if(config.ambient.enabled) for(const auto& p:ambient_) if(p.valid) {
            const float age=float(now_-p.born),motion=reduceMotion ? 0 : age;
            const float speed=Safe(config.ambient.speed,.3f,0,2);
            const float vertical=config.ambient.shape==AmbientShape::Snow ? -.5f : .1f;
            const float fade=std::min(age/.3f,1.f)*std::clamp((kAmbientLifetime-age)/1.5f,0.f,1.f);
            const float drift=reduceMotion||config.ambient.shape==AmbientShape::Snow ? 0 : .12f*std::sin(age+p.phase);
            const WorldPoint position{p.p.x+p.velocity.x*motion*speed,p.p.y+vertical*motion*speed+drift,p.p.z+p.velocity.z*motion*speed};
            const bool star=config.ambient.shape==AmbientShape::Stars;
            particle(p,position,Safe(config.ambient.size,.045f,.015f,.15f),Palette(config.ambient.colours,start,end,p.mix),
                fade*Safe(config.ambient.opacity,.55f,0,1),star,p.phase+age*.35f,Safe(config.ambient.glow,1,0,2));
        }
        if(config.kills.enabled) for(const auto& p:kills_) if(p.valid) {
            const float age=float(now_-p.born),life=Safe(config.kills.lifetime,1.4f,.3f,3);
            const float t=std::clamp(age/life,0.f,1.f),motion=reduceMotion?0:age;
            const WorldPoint position{p.p.x+p.velocity.x*motion,p.p.y+p.velocity.y*motion-motion*motion*.65,p.p.z+p.velocity.z*motion};
            particle(p,position,Safe(config.kills.size,.07f,.02f,.2f),Palette(config.kills.colours,start,end,p.mix),
                (1-t)*(1-t)*Safe(config.kills.opacity,.9f,0,1),config.kills.shape==ParticleShape::Stars,p.phase+age,
                Safe(config.kills.glow,1,0,2));
        }
        // Ringe: Treffer als kameraorientierte Blase, Tod als horizontaler Welt-Ring.
        for(const auto& ring:rings_) if(ring.valid) {
            const bool kill=ring.kill;
            if((kill&&!config.kills.enabled)||(!kill&&!config.bubbles.enabled)||Distance(ring.p,local_)>Safe(config.maxDistance,64,4,128)) continue;
            const float life=kill?Safe(config.kills.lifetime,1.4f,.3f,3):Safe(config.bubbles.lifetime,.75f,.2f,2);
            const float t=std::clamp(float(now_-ring.born)/life,0.f,1.f);
            const float alpha=(1-t)*(1-t)*std::min(t/.06f,1.f)*(kill?Safe(config.kills.opacity,.9f,0,1):Safe(config.bubbles.opacity,.8f,0,1));
            const float expansion=reduceMotion?.8f:.25f+.75f*(1-(1-t)*(1-t));
            const float radius=(kill?Safe(config.kills.radius,1.2f,.3f,3):Safe(config.bubbles.radius,.65f,.15f,1.5f))*expansion;
            const auto& palette=kill?config.kills.colours:config.bubbles.colours;
            const float intensity=kill?Safe(config.kills.glow,1,0,2):Safe(config.bubbles.glow,1,0,2);
            ScreenPoint center; float screenRadius,stroke;
            if(alpha<=0||!projection.Billboard(ring.p,radius,center,screenRadius)||!Visible(center,screenRadius*1.5f,viewport)) continue;
            ScreenPoint unused;
            const float width=kill?.035f:Safe(config.bubbles.width,.025f,.005f,.08f);
            if(!projection.Billboard(ring.p,width,unused,stroke)) continue;
            stroke=std::clamp(stroke,.7f,8.f);
            std::array<ImVec2,kRingSegments> points{};
            bool valid=true;
            for(int i=0;i<kRingSegments;++i) {
                const auto v=circle_[i];
                if(kill) {
                    ScreenPoint p;
                    if(!projection.Point({ring.p.x+v.x*radius,ring.p.y,ring.p.z+v.y*radius},p)) { valid=false; break; }
                    points[i]=Pixel(p,origin);
                } else points[i]={center.x+origin.x+v.x*screenRadius,center.y+origin.y+v.y*screenRadius};
            }
            if(!valid||screenRadius>std::max(viewport.x,viewport.y)) continue;
            for(int i=0;i<kRingSegments;++i) {
                const auto colour=Palette(palette,start,end,.5f+.5f*std::sin(float(i)*6.2831853f/kRingSegments+ring.mix*6.2831853f));
                GlowLine(draw,points[i],points[(i+1)%kRingSegments],stroke,colour,alpha*.75f,intensity);
            }
            if(glow&&intensity>0) {
                TextureScope texture(draw,glow);
                for(int i=0;i<kRingSegments;++i) {
                    const auto colour=Palette(palette,start,end,.5f+.5f*std::sin(float(i)*6.2831853f/kRingSegments+ring.mix*6.2831853f));
                    // Überlappende Nebel-Sprites statt harter, breiter Polygonränder.
                    Sprite(draw,glow,points[i],stroke*6,colour,alpha*.09f*intensity);
                }
            }
        }
        if(config.crumbs.enabled) for(int i=0;i<crumbCount_;++i) {
            const auto& sample=crumbs_[(crumbHead_-1-i+kCrumbs)%kCrumbs];
            const float age=float(now_-sample.born)/Safe(config.crumbs.lifetime,2,.3f,4);
            if(age<0||age>=1||Distance(sample.p,local_)>Safe(config.maxDistance,64,4,128)) continue;
            ScreenPoint p; float radius;
            if(projection.Billboard(sample.p,Safe(config.crumbs.size,.06f,.02f,.15f),p,radius)&&Visible(p,radius*4,viewport))
                Orb(draw,Pixel(p,origin),std::clamp(radius,.4f,20.f),Palette(config.crumbs.colours,start,end,age),
                    (1-age)*(1-age)*Safe(config.crumbs.opacity,.75f,0,1),glow,Safe(config.crumbs.glow,1,0,2));
        }
    }
    draw.PopClipRect();
}
} // namespace mcov

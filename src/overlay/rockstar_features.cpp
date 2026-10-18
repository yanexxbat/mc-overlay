#include "rockstar_features.hpp"
#include "friend_hat_renderer.hpp"
#include "../common/block_scan_volume.hpp"
#include <numbers>
#include <cstdio>
#include <cstring>
#include <windows.h>

namespace mcov::rockstar {
namespace {
constexpr double rad=std::numbers::pi/180;
float Safe(float v,float lo,float hi,float fallback=1) { return std::clamp(std::isfinite(v)?v:fallback,lo,hi); }
bool Finite(WorldPoint p) { return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z); }
double Distance(WorldPoint a,WorldPoint b) { return std::hypot(std::hypot(a.x-b.x,a.z-b.z),a.y-b.y); }
ImVec4 Mix(ImVec4 a,ImVec4 b,float t) { return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,a.w+(b.w-a.w)*t}; }
ImU32 Ink(ImVec4 c,float alpha=1) { c.w*=std::clamp(alpha,0.f,1.f); return ImGui::ColorConvertFloat4ToU32(c); }
bool Player(const EntitySnapshot& e) { return (e.flags&kEntityIsPlayer)&&!(e.flags&kEntityIsSelf)&&e.health>0&&Finite({e.x,e.y,e.z}); }
void WorldLine(ImDrawList& d,const CameraProjection& p,WorldPoint a,WorldPoint b,ImVec4 color,float width,float glow,ImTextureID texture) {
    ScreenPoint sa,sb; float inverseA{},inverseB{};
    if(p.Segment(a,b,sa,sb,&inverseA,&inverseB))
        soft::Stroke(d,{sa.x,sa.y},{sb.x,sb.y},width,color,glow,texture,1/inverseA,1/inverseB);
}
void LineText(ImDrawList& d,float y,const char* text,ImU32 color=style::hud::Text) { style::hud::Label(d,{10,y},color,text); }
}
Config::Config() {
    (*this)[Feature::Crosshair].size=5; (*this)[Feature::Crosshair].range=3;
    (*this)[Feature::Crosshair].glow=0; (*this)[Feature::Crosshair].opacity=1;
    (*this)[Feature::BeaconRadius].size=50; (*this)[Feature::GlowEsp].size=1.5f;
    (*this)[Feature::DetectPlayer].range=32; (*this)[Feature::NameProtect].mode=0;
    (*this)[Feature::DamageMarkers].lifetime=1.6f; (*this)[Feature::Notifications].lifetime=4;
    (*this)[Feature::Arrows].size=10; (*this)[Feature::ChunkBorders].range=1;
    (*this)[Feature::ChestEsp].range=(*this)[Feature::BeaconRadius].range=24;
    (*this)[Feature::HitSounds].opacity=.5f; (*this)[Feature::ModuleSounds].opacity=.3f;
}
void Config::Clamp() {
    for(auto& c:modules) {
        c.mode=std::clamp(c.mode,0,3); c.range=Safe(c.range,0,&c==&(*this)[Feature::ChestEsp]?float(kMaxBlockScanRadius):256.f,64); c.size=Safe(c.size,.1f,128);
        c.lifetime=Safe(c.lifetime,.2f,10,1.2f); c.speed=Safe(c.speed,0,8); c.opacity=Safe(c.opacity,0,1,.8f);
        c.glow=Safe(c.glow,0,2,.75f);
        for(auto* color:{&c.start,&c.end}) *color={Safe(color->x,0,1),Safe(color->y,0,1),Safe(color->z,0,1),Safe(color->w,0,1)};
    }
    (*this)[Feature::ChestEsp].range=std::clamp((*this)[Feature::ChestEsp].range,float(kMinBlockScanRadius),float(kMaxBlockScanRadius));
    (*this)[Feature::BeaconRadius].range=std::clamp((*this)[Feature::BeaconRadius].range,4.f,48.f);
}
const char* Pack::DisplayName(const char* name,bool protect,bool player) { return protect&&player?"Player":name; }
void Pack::ResetWorld() {
    tracks_={}; damage_={}; rings_={}; notices_={}; clicks_={}; clickHeads_={}; input_={};
    damageHead_=ringHead_=noticeHead_=0;
    stats_={}; initialized_=false; hitSound_=false; started_=sampled_=0; positionQpc_=0; hurtAt_=lastSoundAt_=-1000;
}
void Pack::Notify(const char* text,double now) {
    auto& n=notices_[noticeHead_++%kNotices]; std::snprintf(n.text.data(),n.text.size(),"%s",text); n.born=now;
}
bool Pack::AddMarker(const WorldSnapshot& s,const char* name,bool death) {
    if(!Finite({s.posX,s.posY,s.posZ})||!s.dimension[0]) return false;
    Marker* slot=nullptr;
    for(auto& m:markers_) if(death&&m.used&&m.death) { slot=&m; break; }
    if(!slot) for(auto& m:markers_) if(!m.used) { slot=&m; break; }
    if(!slot) return false;
    *slot={}; slot->used=true; slot->death=death; slot->p={s.posX,s.posY,s.posZ};
    std::snprintf(slot->label.data(),slot->label.size(),"%s",name);
    std::memcpy(slot->dimension.data(),s.dimension,kNameLen); std::memcpy(slot->server.data(),s.serverAddress,kAddrLen);
    return true;
}
void Pack::Input(const keys::Input& next,bool allowed,double now) {
    for(int button=0;button<2;++button) {
        const int key=button?VK_RBUTTON:VK_LBUTTON;
        if(allowed&&next[key]&&!input_[key]) clicks_[button][clickHeads_[button]++%96]=now;
    }
    input_=allowed?next:keys::Input{};
}
Stats Pack::Metrics(double now) const {
    auto result=stats_; result.seconds=initialized_?std::max(0.,now-started_):0;
    for(int button=0;button<2;++button) for(double t:clicks_[button])
        if(t>0&&now>=t&&now-t<1) (button?result.rightCps:result.leftCps)++;
    return result;
}
int Pack::DamageCount(double now,float life) const { int count=0; for(const auto& d:damage_) count+=now>=d.born&&now-d.born<life; return count; }
int Pack::RingCount(double now,float life) const { int count=0; for(const auto& r:rings_) count+=now>=r.born&&now-r.born<life; return count; }
void Pack::Update(const WorldSnapshot& s,double now,const Config& c,std::uint32_t target) {
    if(!std::isfinite(now)||!Finite({s.posX,s.posY,s.posZ})||!s.dimension[0]) { ResetWorld(); return; }
    now_=now;
    const bool worldChanged=std::memcmp(dimension_.data(),s.dimension,kNameLen)||std::memcmp(server_.data(),s.serverAddress,kAddrLen);
    if(initialized_&&(worldChanged||now<sampled_||now-sampled_>2)) ResetWorld();
    if(initialized_&&frame_>=s.frameIndex) return;
    WorldPoint local{s.posX,s.posY,s.posZ};
    for(const auto& entity:std::span(s.entities,std::min(s.entityCount,kMaxEntities))) {
        if((entity.flags&kEntityIsSelf)&&Finite({entity.x,entity.y,entity.z})) { local={entity.x,entity.y,entity.z}; break; }
    }
    // Geometrie wird jedes Kameraframe erfasst; posX/Y/Z gehoeren zum langsamen HUD-Sample.
    static const double frequency=[] { LARGE_INTEGER f{}; QueryPerformanceFrequency(&f); return double(f.QuadPart); }();
    const double dt=s.captureQpc>0&&positionQpc_>0&&frequency>0?double(s.captureQpc-positionQpc_)/frequency:now-sampled_;
    if(!initialized_) {
        previous_=local; previousHealth_=s.health; started_=now; previousVertical_=0;
        std::memcpy(dimension_.data(),s.dimension,kNameLen); std::memcpy(server_.data(),s.serverAddress,kAddrLen);
    } else if(dt>.0001&&dt<1) {
        const auto traveled=Distance(local,previous_);
        if(traveled<std::max(4.,dt*48)) {
            stats_.speed=static_cast<float>(std::hypot(local.x-previous_.x,local.z-previous_.z)/dt);
            stats_.verticalSpeed=static_cast<float>((local.y-previous_.y)/dt); stats_.distance+=traveled;
            if(c.Enabled(Feature::JumpCircles)&&s.health>0&&stats_.verticalSpeed>2.5f&&previousVertical_<.5f)
                rings_[ringHead_++%kRings]={previous_,now};
            previousVertical_=stats_.verticalSpeed;
        } else { stats_.speed=stats_.verticalSpeed=0; previousVertical_=0; tracks_={}; }
        if(std::isfinite(s.health)&&s.health<previousHealth_) {
            stats_.received+=previousHealth_-std::max(0.f,s.health); hurtAt_=now;
            if(c.Enabled(Feature::DeathCoordinates)&&s.health<=0&&previousHealth_>0) {
                AddMarker(s,"Death",true); char text[112]; std::snprintf(text,sizeof(text),"Death: %.0f %.0f %.0f",std::floor(s.posX),std::floor(s.posY),std::floor(s.posZ)); Notify(text,now);
            }
        }
    }
    for(const auto& e:std::span(s.entities,std::min(s.entityCount,kMaxEntities))) {
        if(!(e.flags&kEntityIsLiving)||(e.flags&kEntityIsSelf)||!std::isfinite(e.health)||!Finite({e.x,e.y,e.z})) continue;
        Track* track=nullptr;
        for(auto& t:tracks_) if(t.used&&t.id==e.entityId&&t.uuid==e.uuid) { track=&t; break; }
        if(!track) {
            for(auto& t:tracks_) if(!t.used||now-t.seen>2) { track=&t; break; }
            if(!track) continue;
            *track={true,e.entityId,e.uuid,e.health,now};
        } else if(now-track->seen<1&&e.health<track->health&&track->health>0) {
            const float amount=track->health-std::max(0.f,e.health); stats_.observedDamage+=amount;
            if(c.Enabled(Feature::DamageMarkers)) damage_[damageHead_++%kDamage]={{e.x,e.y+e.height+.1,e.z},amount,now};
            if(target&&e.entityId==target) {
                if(c.Enabled(Feature::HitSounds)&&now-lastSoundAt_>.12) { hitSound_=true; lastSoundAt_=now; }
                if(e.health<=0) ++stats_.targetDeaths;
            }
        }
        const bool nearby=Player(e)&&e.distance<=c[Feature::DetectPlayer].range;
        if(initialized_&&c.Enabled(Feature::DetectPlayer)&&nearby&&!track->nearby) {
            char text[112]; std::snprintf(text,sizeof(text),"Nearby: %s (%.1f m)",DisplayName(e.name,c.Enabled(Feature::NameProtect),true),e.distance); Notify(text,now);
        }
        track->nearby=nearby; track->health=e.health; track->seen=now;
    }
    previous_=local; previousHealth_=s.health; sampled_=now; positionQpc_=s.captureQpc; frame_=s.frameIndex; initialized_=true;
}
void Pack::DrawWorld(ImDrawList& d,const WorldSnapshot& s,const CameraProjection& p,const Config& config,
        const combat::Relations& friends,std::uint32_t target,ImVec2 viewport,ImVec4 auraA,ImVec4 auraB,
        AuraBlend blend,soft::Textures textures,bool reduced) const {
    (void)viewport;
    bool worldEffects=false;
    for(const auto f:{Feature::JumpCircles,Feature::Wings,Feature::Halo,Feature::GlowEsp,Feature::TargetCircle,Feature::ChestEsp,Feature::BeaconRadius,Feature::ChunkBorders}) worldEffects|=config.Enabled(f);
    if(!worldEffects) return;
    // Getrennte Commands kapseln additives Blending; danach wird der ImGui-State restauriert.
    soft::BlendScope blendScope(d,blend); soft::TextureScope glowScope(d,textures.glow);
    auto palette=[&](const Options& o,float alpha=1) {
        auto a=o.auraColours?auraA:o.start,b=o.auraColours?auraB:o.end; a.w*=o.opacity*alpha; b.w*=o.opacity*alpha;
        return std::pair{a,b};
    };
    const auto& jump=config[Feature::JumpCircles];
    int visibleRings=0;
    if(jump.enabled) for(const auto& r:rings_) {
        const float t=static_cast<float>((now_-r.born)/jump.lifetime);
        if(t<0||t>=1||visibleRings++>=8) continue;
        // Schneller weicher Eintritt, ruhiges Wachstum und langsames Auslaufen.
        const float enter=std::clamp(t/.08f,0.f,1.f),fade=enter*std::pow(std::cos(t*std::numbers::pi_v<float>*.5f),1.5f);
        const auto [a,b]=palette(jump,fade);
        const double radius=jump.size*(reduced?1:.45+.75*(1-std::pow(1-t,3))),rotation=reduced?0:(now_-r.born)*jump.speed*.4;
        auto center=r.p; center.y+=.035; soft::Ring(d,p,center,radius,rotation,a,b,textures,jump.glow);
        // Deterministische Lichtstaub-Punkte, keine neuen Pools oder Zufalls-Allokationen.
        for(int i=0;i<24;++i) {
            const double angle=i*2.399963229728653+rotation,spread=.75+.6*double((i*7)%23)/23;
            const WorldPoint point{center.x+std::cos(angle)*radius*spread,center.y+.025+(reduced?0:t*.12*(i%3)),center.z+std::sin(angle)*radius*spread};
            ScreenPoint screen; float size;
            if(p.Billboard(point,.012f*jump.size,screen,size)&&size<16) {
                auto color=Mix(a,b,float(i%7)/6); color.w*=.25f+.35f*float(std::sin(angle*3+t*8)*.5+.5);
                soft::Orb(d,textures.glow,{screen.x,screen.y},std::clamp(size,.5f,4.f),color,jump.glow,p.Depth(point));
            }
        }
    }
    int cosmetics=0;
    for(const auto& e:std::span(s.entities,std::min(s.entityCount,kMaxEntities))) {
        if(!(e.flags&kEntityIsLiving)||e.health<=0||!Finite({e.x,e.y,e.z})) continue;
        const bool self=(e.flags&kEntityIsSelf)!=0,friendEntity=combat::Lookup(friends,e.uuid)==combat::Relation::Friend;
        const auto bounds=EntityBounds(e);
        const bool selfVisible=!self||FriendHatRenderer::LocalVisible(e,{s.camX,s.camY,s.camZ},false);
        const auto& esp=config[Feature::GlowEsp];
        if(esp.enabled&&!self&&e.distance<=esp.range&&(esp.mode==1||(e.flags&kEntityIsPlayer))) {
            ScreenRect box;
            if(p.Bounds(bounds,box)) {
                auto [a,b]=palette(esp); if(friendEntity) a=b={.2f,1,.6f,esp.opacity};
                soft::Outline(d,{box.left,box.top},{box.right,box.bottom},8,esp.size,a,b,esp.glow,textures.glow);
            }
        }
        const auto& circle=config[Feature::TargetCircle];
        if(circle.enabled&&!self&&target&&e.entityId==target) {
            const auto [a,b]=palette(circle); const double spin=reduced?0:now_*circle.speed;
            const double y=bounds.min.y+(circle.mode==1?(bounds.max.y-bounds.min.y)*(.5+.5*std::sin(spin)):.04);
            soft::Ring(d,p,{e.x,y,e.z},std::max(.2f,e.width*.75f)*circle.size,spin*.2,a,b,textures,circle.glow);
        }
        if((!config.Enabled(Feature::Wings)&&!config.Enabled(Feature::Halo))||!(e.flags&kEntityIsPlayer)||!selfVisible||(!self&&(e.distance>64||cosmetics++>=32))) continue;
        const auto rig=BuildHumanoidRig(e);
        const auto& halo=config[Feature::Halo];
        if(halo.enabled) { const auto [a,b]=palette(halo); auto head=rig.joints[0]; head.y+=e.height*.16+.05;
            soft::Ring(d,p,head,e.width*.6*halo.size,reduced?0:now_*.12,a,b,textures,halo.glow); }
        const auto& wings=config[Feature::Wings];
        if(wings.enabled) {
            const auto [a,b]=palette(wings); const double yaw=e.yaw*rad;
            const auto base=rig.joints[1]; const double pulse=reduced?0:std::sin(now_*wings.speed*2)*.2;
            auto point=[&](double x,double y,double z) {
                if(e.pose==kPoseSwimming||e.pose==kPoseGliding) std::swap(y,z);
                return WorldPoint{base.x+x*std::cos(yaw)-z*std::sin(yaw),base.y+y,base.z+x*std::sin(yaw)+z*std::cos(yaw)};
            };
            soft::TextureScope featherScope(d,textures.feather);
            for(int side:{-1,1}) for(int feather=0;feather<8;++feather) {
                const double t=double(feather)/7,span=wings.size*e.height*.72;
                const double x=side*span*(.4+.6*t),y=span*(.7-1.05*t),z=-.16-span*(.18+pulse)*t;
                const double dx=x-side*.1,dy=y+t*.10,length=std::hypot(dx,dy),width=span*.20;
                const double nx=-dy/length*width,ny=dx/length*width;
                auto ca=Mix(a,b,float(t)),cb=soft::Mix(ca,{1,1,1,ca.w},.45f); ca.w*=.7f; cb.w*=.8f;
                const std::array<WorldPoint,4> quad={point(side*.1+nx,-t*.10+ny,-.10),point(x+nx,y+ny,z),point(x-nx,y-ny,z),point(side*.1-nx,-t*.10-ny,-.10)};
                soft::WorldQuad(d,p,quad,textures.feather,ca,cb);
                if(wings.glow>0) { auto mist=ca; mist.w*=.18f*wings.glow;
                    const std::array<WorldPoint,4> fog={point(side*.1+nx*2,-t*.10+ny*2,-.10),point(x+nx*2,y+ny*2,z),point(x-nx*2,y-ny*2,z),point(side*.1-nx*2,-t*.10-ny*2,-.10)};
                    soft::WorldQuad(d,p,fog,textures.feather,mist,mist);
                }
            }
        }
    }
    const auto& borders=config[Feature::ChunkBorders];
    if(borders.enabled) {
        const auto [a,b]=palette(borders); const int cx=static_cast<int>(std::floor(s.posX/16)),cz=static_cast<int>(std::floor(s.posZ/16));
        const int radius=std::clamp(int(borders.range),0,3);
        for(int x=cx-radius;x<=cx+radius;++x) for(int z=cz-radius;z<=cz+radius;++z) {
            const double lowX=x*16.,lowZ=z*16.,y=std::floor(s.posY);
            const WorldPoint points[]={{lowX,y,lowZ},{lowX+16,y,lowZ},{lowX+16,y,lowZ+16},{lowX,y,lowZ+16}};
            for(int i=0;i<4;++i) { WorldLine(d,p,points[i],points[(i+1)%4],a,1,borders.glow,textures.glow); auto top=points[i]; top.y+=16; WorldLine(d,p,points[i],top,b,1,borders.glow,textures.glow); }
        }
    }
    const auto& beacon=config[Feature::BeaconRadius];
    if(beacon.enabled) for(const auto& block:std::span(s.blocks,std::min(s.blockCount,kMaxBlocks))) if(block.kind==kBlockBeacon) {
        if(Distance({block.x,block.y,block.z},{s.posX,s.posY,s.posZ})>beacon.range) continue;
        const auto [a,b]=palette(beacon); soft::Ring(d,p,{block.x+.5,block.y+1.05,block.z+.5},beacon.size,reduced?0:now_*.08,a,b,textures,beacon.glow);
        WorldLine(d,p,{block.x+.5,block.y+1,block.z+.5},{block.x+.5,block.y+32,block.z+.5},b,2,beacon.glow,textures.glow);
    }
}
void Pack::DrawScreen(ImDrawList& d,const WorldSnapshot& s,const CameraProjection& projection,const Config& c,ImVec2 size,ImVec4 a,ImVec4 b,double now,AuraBlend blend,soft::Textures textures) const {
    const auto& hurt=c[Feature::DamageInfo];
    const float fade=std::clamp(1-float((now-hurtAt_)/hurt.lifetime),0.f,1.f)*hurt.opacity;
    if(hurt.enabled&&fade>0) {
        const auto red=Ink({1,.1f,.2f,fade*.6f}),clear=Ink({1,.1f,.2f,0}); const float edge=std::clamp(hurt.size*18,4.f,size.y*.2f);
        d.AddRectFilledMultiColor({0,0},{size.x,edge},red,red,clear,clear);
        d.AddRectFilledMultiColor({0,size.y-edge},size,clear,clear,red,red);
        d.AddRectFilledMultiColor({0,0},{edge,size.y},red,clear,clear,red);
        d.AddRectFilledMultiColor({size.x-edge,0},size,clear,red,red,clear);
    }
    const auto& markers=c[Feature::DamageMarkers];
    if(markers.enabled) for(const auto& hit:damage_) {
        const float age=float(now-hit.born); if(age<0||age>=markers.lifetime) continue;
        const auto point=WorldPoint{hit.p.x,hit.p.y+age*.5*markers.speed,hit.p.z}; ScreenPoint screen;
        if(projection.Point(point,screen)) {
            char text[24]; std::snprintf(text,sizeof(text),"-%.1f",hit.amount); const ImVec2 pos{screen.x,screen.y};
            auto color=Mix(markers.auraColours?a:markers.start,markers.auraColours?b:markers.end,age/markers.lifetime);
            color.w*=markers.opacity*(1-age/markers.lifetime);
            const float fontSize=ImGui::GetFontSize()*markers.size;
            for(int layer=2;layer>=1;--layer) for(const auto offset:{ImVec2{float(layer),0},ImVec2{-float(layer),0},ImVec2{0,float(layer)},ImVec2{0,-float(layer)}})
                d.AddText(ImGui::GetFont(),fontSize,{pos.x+offset.x,pos.y+offset.y},Ink(color,.09f*markers.glow),text);
            d.AddText(ImGui::GetFont(),fontSize,pos,Ink(color),text);
        }
    }
    {
    soft::BlendScope blendScope(d,blend); soft::TextureScope textureScope(d,textures.glow);
    const auto& cross=c[Feature::Crosshair];
    if(cross.enabled) {
        const ImVec2 center{size.x*.5f,size.y*.5f}; auto color=cross.auraColours?(s.hasTarget?b:a):cross.start; color.w*=cross.opacity;
        const float gap=cross.range+(cross.mode==3?std::min(stats_.speed*1.5f,12.f):0),length=cross.size,width=std::max(.7f,cross.speed);
        if(cross.mode==1) {
            for(int i=0;i<48;++i) {
                const double first=i*2*std::numbers::pi/48,next=(i+1)*2*std::numbers::pi/48;
                soft::Stroke(d,{center.x+float(std::cos(first))*(gap+length),center.y+float(std::sin(first))*(gap+length)},
                    {center.x+float(std::cos(next))*(gap+length),center.y+float(std::sin(next))*(gap+length)},width,color,cross.glow,textures.glow);
            }
        } else if(cross.mode==2) {
            soft::Orb(d,textures.glow,center,std::max(1.f,length*.3f),color,cross.glow);
        }
        else for(int side:{-1,1}) { soft::Stroke(d,{center.x+side*gap,center.y},{center.x+side*(gap+length),center.y},width,color,cross.glow,textures.glow); soft::Stroke(d,{center.x,center.y+side*gap},{center.x,center.y+side*(gap+length)},width,color,cross.glow,textures.glow); }
    }
    const auto& arrows=c[Feature::Arrows];
    if(arrows.enabled) for(const auto& e:std::span(s.entities,std::min(s.entityCount,kMaxEntities))) if(Player(e)&&e.distance<=arrows.range) {
        ScreenPoint screen;
        if(projection.Point({e.x,e.y+e.height*.5,e.z},screen)&&screen.x>=0&&screen.x<=size.x&&screen.y>=0&&screen.y<=size.y) continue;
        const double bearing=std::atan2(-(e.x-s.camX),e.z-s.camZ)-s.yawDeg*rad;
        const float x=float(std::sin(bearing)),y=-float(std::cos(bearing)); const float rr=std::min(size.x,size.y)*.38f;
        const ImVec2 p{size.x*.5f+x*rr,size.y*.5f+y*rr}; auto color=arrows.auraColours?a:arrows.start; color.w*=arrows.opacity;
        const float w=arrows.size;
        const ImVec2 tip{p.x+x*w,p.y+y*w},left{p.x-x*w*.7f+y*w*.6f,p.y-y*w*.7f-x*w*.6f},right{p.x-x*w*.7f-y*w*.6f,p.y-y*w*.7f+x*w*.6f};
        soft::Stroke(d,left,tip,1.5f,color,arrows.glow,textures.glow); soft::Stroke(d,tip,right,1.5f,color,arrows.glow,textures.glow);
        char text[20]; std::snprintf(text,sizeof(text),"%.0fm",e.distance);
        // Schrift verwendet den Font-Atlas und normale Transparenz, keine Gauß-UVs.
        if(blend.begin) d.AddCallback(ImDrawCallback_ResetRenderState,nullptr);
        { soft::TextureScope font(d,ImGui::GetIO().Fonts->TexID); d.AddText({p.x-10,p.y+w+3},Ink(color),text); }
        if(blend.begin) d.AddCallback(blend.begin,blend.data);
    }
    }
    for(const auto& m:markers_) if(m.used&&(m.death?c.Enabled(Feature::DeathCoordinates):c.Enabled(Feature::Waypoints))&&
            !std::memcmp(m.dimension.data(),s.dimension,kNameLen)&&!std::memcmp(m.server.data(),s.serverAddress,kAddrLen)) {
        ScreenPoint screen; if(!projection.Point({m.p.x,m.p.y+.8,m.p.z},screen)) continue;
        char text[96]; std::snprintf(text,sizeof(text),"%s | %.0f m",m.label.data(),Distance(m.p,{s.posX,s.posY,s.posZ}));
        { soft::BlendScope glow(d,blend); soft::Orb(d,textures.glow,{screen.x,screen.y},5,{.68f,.41f,.82f,1}); }
        d.AddText({screen.x+10,screen.y-6},IM_COL32(236,236,238,255),text);
    }
}
void Pack::DrawHud(ImDrawList& d,HudModule kind,const WorldSnapshot& s,const Config& config,const combat::Relations& friends,ImVec2 size,double now) const {
    const auto metrics=Metrics(now); char text[128];
    DrawHudPanel(d,{0,0},size); style::hud::Heading(d,{0,0},size.x,kHudNames[static_cast<int>(kind)]);
    switch(kind) {
        case HudModule::Speedometer:
            std::snprintf(text,sizeof(text),"%.2f m/s | %.1f km/h",metrics.speed,metrics.speed*3.6f); LineText(d,27,text);
            std::snprintf(text,sizeof(text),"Vertical: %+.2f m/s",metrics.verticalSpeed); LineText(d,45,text); break;
        case HudModule::Clock: { SYSTEMTIME time{}; GetLocalTime(&time); std::snprintf(text,sizeof(text),"%02d:%02d:%02d",time.wHour,time.wMinute,time.wSecond); LineText(d,27,text); break; }
        case HudModule::Session:
            std::snprintf(text,sizeof(text),"Time %02d:%02d | Distance %.0f m",int(metrics.seconds)/60,int(metrics.seconds)%60,metrics.distance); LineText(d,27,text);
            std::snprintf(text,sizeof(text),"Damage received: %.1f",metrics.received); LineText(d,45,text);
            std::snprintf(text,sizeof(text),"Observed damage: %.1f",metrics.observedDamage); LineText(d,63,text);
            std::snprintf(text,sizeof(text),"Target deaths observed: %d",metrics.targetDeaths); LineText(d,81,text); break;
        case HudModule::Cps: std::snprintf(text,sizeof(text),"LMB %d CPS | RMB %d CPS",metrics.leftCps,metrics.rightCps); LineText(d,27,text); break;
        case HudModule::Keystrokes: {
            constexpr int keys[]={0x57,0x41,0x53,0x44,0x20,1,2}; constexpr const char* labels[]={"W","A","S","D","SPACE","LMB","RMB"};
            constexpr ImVec2 positions[]={{48,26},{10,53},{48,53},{86,53},{10,80},{10,107},{69,107}};
            for(int i=0;i<7;++i) { const auto p=positions[i]; const float width=i==4?110.f:i>4?51.f:34.f;
                d.AddRectFilled(p,{p.x+width,p.y+23},input_[keys[i]]?style::hud::Pressed:style::hud::Track,4);
                const auto measure=ImGui::CalcTextSize(labels[i]);
                d.AddText({p.x+(width-measure.x)*.5f,p.y+(23-measure.y)*.5f},input_[keys[i]]?style::hud::AccentActive:style::hud::Text,labels[i]); }
            break;
        }
        case HudModule::Radar: {
            const ImVec2 center{size.x*.5f,size.y*.5f+8}; const float radius=std::min(size.x,size.y)*.38f;
            d.AddCircle(center,radius,style::hud::Muted,48);
            d.AddLine({center.x-radius,center.y},{center.x+radius,center.y},style::hud::Separator);
            d.AddLine({center.x,center.y-radius},{center.x,center.y+radius},style::hud::Separator);
            d.AddTriangleFilled({center.x,center.y-5},{center.x-4,center.y+4},{center.x+4,center.y+4},style::hud::Text);
            const double yaw=s.yawDeg*rad; const double range=std::max(8.f,config[Feature::DetectPlayer].range);
            for(const auto& e:std::span(s.entities,std::min(s.entityCount,kMaxEntities))) if(Player(e)) {
                const double dx=e.x-s.posX,dz=e.z-s.posZ,dist=std::hypot(dx,dz); if(dist>range) continue;
                const float x=float((-dx*std::cos(yaw)-dz*std::sin(yaw))/range)*radius,y=float((dx*std::sin(yaw)-dz*std::cos(yaw))/range)*radius;
                const bool friendEntity=combat::Lookup(friends,e.uuid)==combat::Relation::Friend;
                d.AddCircleFilled({center.x+x,center.y+y},3,friendEntity?style::hud::Positive:style::hud::Accent,12);
            } break;
        }
        case HudModule::ArmorStrip: {
            for(int i=0;i<4;++i) { const auto& item=s.equipment[i]; const float x=8+i*54.f;
                constexpr const char* names[]={"Head","Chest","Legs","Feet"}; d.AddText({x,28},style::hud::Muted,names[i]);
                const float remaining=item.maxDamage>0?std::clamp(1-float(item.damage)/item.maxDamage,0.f,1.f):item.present?1.f:0.f;
                d.AddRectFilled({x,46},{x+46,50},style::hud::Track,2);
                d.AddRectFilled({x,46},{x+46*remaining,50},remaining<.2f?style::hud::Danger:remaining<.5f?style::hud::Warning:style::hud::Accent,2);
                std::snprintf(text,sizeof(text),item.present?"%.0f%%":"--",remaining*100); d.AddText({x,55},style::hud::Text,text);
            } break;
        }
        case HudModule::Indicators: {
            const float target=s.hasTarget?s.targetHealth:0; std::snprintf(text,sizeof(text),"HP %.1f | Target %.1f",s.health,target); LineText(d,27,text);
            const float value=std::clamp((s.health-target+20)/40,0.f,1.f); d.AddRectFilled({10,48},{size.x-10,54},style::hud::Track,3);
            if(s.hasTarget) d.AddRectFilled({10,48},{10+(size.x-20)*value,54},s.health>=target?style::hud::Positive:style::hud::Danger,3);
            LineText(d,64,s.hasTarget?"Health advantage":"No crosshair target"); break;
        }
        case HudModule::Notifications: {
            float y=27; const double lifetime=config[Feature::Notifications].lifetime;
            for(int i=0;i<3;++i) { const auto& note=notices_[(noticeHead_-1-i+kNotices*2)%kNotices]; if(now<note.born||now-note.born>lifetime||!note.text[0]) continue;
                d.PushClipRect({10,y},{size.x-10,std::min(y+34,size.y-8)},true);
                d.AddText(ImGui::GetFont(),ImGui::GetFontSize(),{10,y},style::hud::Text,note.text.data(),nullptr,size.x-20);
                d.PopClipRect(); y+=38; }
            break;
        }
        case HudModule::Nearby: {
            float y=27; int count=0;
            for(const auto& e:std::span(s.entities,std::min(s.entityCount,kMaxEntities))) if(Player(e)&&e.distance<=config[Feature::DetectPlayer].range&&count++<8) {
                std::snprintf(text,sizeof(text),"%s | %.1fm | %.0f HP",DisplayName(e.name,config.Enabled(Feature::NameProtect),true),e.distance,e.health); LineText(d,y,text); y+=18;
            }
            if(!count) LineText(d,27,"No players in range"); break;
        }
        default: break;
    }
}
} // namespace mcov::rockstar

#include "target_cube_renderer.hpp"
#include "menu_motion.hpp"
#include "imgui_internal.h"
#include <numbers>

namespace mcov {
namespace {
float Safe(float v,float fallback,float lo,float hi) { return std::clamp(std::isfinite(v)?v:fallback,lo,hi); }
bool Finite(WorldPoint p) { return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z); }
ImU32 Tint(ImVec4 color,float alpha) {
    color={Safe(color.x,1,0,1),Safe(color.y,1,0,1),Safe(color.z,1,0,1),Safe(color.w,1,0,1)*alpha};
    return ImGui::ColorConvertFloat4ToU32(color);
}
class GlowScope final {
    ImDrawList& draw_;
public:
    GlowScope(ImDrawList& draw,AuraBlend blend,ImTextureID texture):draw_(draw) {
        if(blend.begin) draw_.AddCallback(blend.begin,blend.data);
        draw_.PushTextureID(texture);
    }
    ~GlowScope() { draw_.PopTextureID(); draw_.AddCallback(ImDrawCallback_ResetRenderState,nullptr); }
};
}
void TargetCubeRenderer::Prepare(ImDrawList& draw) {
    draw.VtxBuffer.reserve(draw.VtxBuffer.Size+kMaxVertices);
    draw.IdxBuffer.reserve(draw.IdxBuffer.Size+kMaxIndices);
    draw.CmdBuffer.reserve(draw.CmdBuffer.Size+12);
    draw._TextureIdStack.reserve(draw._TextureIdStack.Size+2);
    draw._Data->TempBuffer.reserve(5*kCubeCornerPoints);
    draw._Path.reserve(32);
}
void TargetCubeRenderer::Reset() noexcept { target_=0; uuid_={}; phase_=angle_=0; enterTime_=alpha_=0; }
void TargetCubeRenderer::Update(const EntitySnapshot* e,float dt,bool animate,const TargetCubeConfig& c) noexcept {
    if(!c.enabled || !e || !e->entityId || (e->flags&kEntityIsSelf) || !(e->flags&kEntityIsLiving) ||
       !std::isfinite(e->health) || e->health<=0) { Reset(); return; }
    if(target_!=e->entityId || uuid_!=e->uuid) { Reset(); target_=e->entityId; uuid_=e->uuid; }
    animate=animate&&c.animate;
    const float step=Safe(dt,0,0,.25f);
    const float fade=Safe(c.fadeSeconds,.16f,0,.5f);
    enterTime_=std::min(enterTime_+step,.5f);
    alpha_=animate&&fade>0 ? .65f+.35f*control_panel::motion::EaseOut(enterTime_/fade) : 1.f;
    if(animate) phase_=std::remainder(phase_+step*2*std::numbers::pi/Safe(c.directionCycleSeconds,7,2,20),2*std::numbers::pi);
    // Analytische Sinusbewegung: Position und Geschwindigkeit bleiben beim
    // Wechsel stetig. Die Phase bleibt bei einer Tempoänderung erhalten.
    angle_=(12+Safe(c.swingDegrees,105,20,180)*std::sin(phase_))*std::numbers::pi/180;
}
bool TargetCubeRenderer::BuildWorld(const EntitySnapshot& e,const WorldSnapshot& camera,
                                    const TargetCubeConfig& c,TargetCubeWorld& out) const noexcept {
    if(!c.enabled || !target_ || target_!=e.entityId || uuid_!=e.uuid || !std::isfinite(camera.yawDeg) || !std::isfinite(camera.pitchDeg)) return false;
    const auto bounds=EntityBounds(e);
    const double w=bounds.max.x-bounds.min.x,h=bounds.max.y-bounds.min.y,d=bounds.max.z-bounds.min.z;
    if(!Finite(bounds.min)||!Finite(bounds.max)||w<=0||h<=0||d<=0||std::max({w,h,d})>32) return false;
    const double side=std::clamp(std::max({h*Safe(c.sizeFraction,.68f,.3f,1.5f),w*1.25,d*1.25}),.2,6.0);
    out.center={std::midpoint(bounds.min.x,bounds.max.x),std::midpoint(bounds.min.y,bounds.max.y),std::midpoint(bounds.min.z,bounds.max.z)};
    out.worldStroke=static_cast<float>(side*Safe(c.strokeFraction,.05f,.005f,.08f));
    const double y=camera.yawDeg*std::numbers::pi/180,p=camera.pitchDeg*std::numbers::pi/180;
    const WorldPoint right{-std::cos(y),0,-std::sin(y)},up{-std::sin(y)*std::sin(p),std::cos(p),std::cos(y)*std::sin(p)},
                     forward{-std::sin(y)*std::cos(p),-std::sin(p),std::cos(y)*std::cos(p)};
    const double ca=std::cos(angle_),sa=std::sin(angle_);
    const WorldPoint r{right.x*ca+up.x*sa,right.y*ca+up.y*sa,right.z*ca+up.z*sa},
                     u{up.x*ca-right.x*sa,up.y*ca-right.y*sa,up.z*ca-right.z*sa};
    const double arm=side*Safe(c.cornerFraction,.28f,.1f,.4f),round=side*Safe(c.roundFraction,.045f,.005f,.09f);
    for(int i=0;i<8;++i) {
        const double sx=i&1?1:-1,sy=i&2?1:-1,sz=i&4?1:-1;
        auto point=[&](double a,double b) {
            return WorldPoint{out.center.x+r.x*sx*(side*.5-a)+u.x*sy*(side*.5-b)+forward.x*sz*side*.5,
                              out.center.y+r.y*sx*(side*.5-a)+u.y*sy*(side*.5-b)+forward.y*sz*side*.5,
                              out.center.z+r.z*sx*(side*.5-a)+u.z*sy*(side*.5-b)+forward.z*sz*side*.5};
        };
        auto& path=out.corners[i]; path[0]=point(arm,0);
        for(int j=1;j<kCubeCornerPoints-1;++j) {
            const double t=double(j-1)/(kCubeCornerPoints-3);
            path[j]=point(round*(1-t)*(1-t),round*t*t);
        }
        path.back()=point(0,arm);
    }
    return true;
}
bool TargetCubeRenderer::Project(const CameraProjection& projection,const TargetCubeWorld& world,
                                ImVec2 viewport,TargetCubeScreen& out,ImVec2 origin) noexcept {
    if(!std::isfinite(viewport.x)||!std::isfinite(viewport.y)||viewport.x<1||viewport.y<1) return false;
    ScreenPoint center; float stroke;
    if(!projection.Billboard(world.center,world.worldStroke,center,stroke)) return false;
    out.stroke=std::clamp(stroke,1.25f,14.f);
    float left=viewport.x,right=0,top=viewport.y,bottom=0;
    for(int i=0;i<8;++i) for(int j=0;j<kCubeCornerPoints;++j) {
        ScreenPoint p;
        // An der Near Plane vollständig aussetzen statt riesige Glow-Quads zu erzeugen.
        if(!projection.Point(world.corners[i][j],p)) return false;
        out.corners[i][j]={p.x+origin.x,p.y+origin.y};
        left=std::min(left,p.x); right=std::max(right,p.x); top=std::min(top,p.y); bottom=std::max(bottom,p.y);
    }
    const float margin=out.stroke*4;
    return right+margin>=0 && left-margin<=viewport.x && bottom+margin>=0 && top-margin<=viewport.y &&
           right-left<viewport.x*1.5f && bottom-top<viewport.y*1.5f;
}
void TargetCubeRenderer::Draw(ImDrawList& draw,const TargetCubeScreen& screen,const TargetCubeConfig& c,
                              AuraBlend blend,ImTextureID texture) const {
    const float alpha=alpha_*Safe(c.opacity,1,0,1),glow=Safe(c.glow,1,0,2);
    if(!c.enabled || alpha<=0 || !std::isfinite(screen.stroke)||screen.stroke<=0) return;
    for(const auto& path:screen.corners) for(auto p:path) if(!std::isfinite(p.x)||!std::isfinite(p.y)) return;
    // Dezente hintere Ecken und Tiefenlinien halten die Würfelform lesbar.
    if(c.showDepth) {
        for(int i=4;i<8;++i) draw.AddPolyline(screen.corners[i].data(),kCubeCornerPoints,Tint(c.lime,alpha*.13f),0,screen.stroke*.6f);
        for(int i=0;i<4;++i) draw.AddLine(screen.corners[i][5],screen.corners[i+4][5],Tint(c.lime,alpha*.12f),screen.stroke*.45f);
    }
    if(texture && glow>0) {
        GlowScope scope(draw,blend,texture);
        const auto lo=draw.GetClipRectMin(),hi=draw.GetClipRectMax();
        for(int corner=0;corner<4;++corner) {
            const auto& path=screen.corners[corner];
            std::array<float,kCubeCornerPoints> distance{};
            for(int j=1;j<kCubeCornerPoints;++j) distance[j]=distance[j-1]+std::hypot(path[j].x-path[j-1].x,path[j].y-path[j-1].y);
            int segment=1;
            const auto color=corner==3 ? c.yellow : c.lime;
            for(int i=0;i<kGlowSamples;++i) {
                const float position=distance.back()*i/(kGlowSamples-1);
                while(segment<kCubeCornerPoints-1 && distance[segment]<position) ++segment;
                const float length=distance[segment]-distance[segment-1];
                const float t=length>0?(position-distance[segment-1])/length:0;
                const ImVec2 p{std::lerp(path[segment-1].x,path[segment].x,t),std::lerp(path[segment-1].y,path[segment].y,t)};
                for(int layer=0;layer<2;++layer) {
                    const float radius=screen.stroke*(layer?2.2f:4.f);
                    if(p.x+radius<lo.x || p.x-radius>hi.x || p.y+radius<lo.y || p.y-radius>hi.y) continue;
                    draw.AddImage(texture,{p.x-radius,p.y-radius},{p.x+radius,p.y+radius},{0,0},{1,1},Tint(color,alpha*glow*(layer?.065f:.018f)));
                }
            }
        }
    }
    // Solider Kern mit gewöhnlichem Alpha-Blending nach dem Glow-Callback.
    for(int i=0;i<4;++i) {
        draw.AddPolyline(screen.corners[i].data(),kCubeCornerPoints,Tint(i==3?c.yellow:c.lime,alpha*.95f),0,screen.stroke);
        draw.AddPolyline(screen.corners[i].data(),kCubeCornerPoints,Tint(c.yellow,alpha*.7f),0,screen.stroke*.42f);
        for(auto end:{screen.corners[i].front(),screen.corners[i].back()}) {
            draw.AddCircleFilled(end,screen.stroke*.5f,Tint(i==3?c.yellow:c.lime,alpha*.95f),16);
            draw.AddCircleFilled(end,screen.stroke*.21f,Tint(c.yellow,alpha*.7f),16);
        }
    }
}
} // namespace mcov

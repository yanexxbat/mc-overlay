#pragma once
#include "esp_geometry.hpp"
#include "aura_renderer.hpp"
#include "depth_tags.hpp"
#include <numbers>

namespace mcov::soft {
struct Textures { ImTextureID glow{},ring{},feather{}; };
inline float Gaussian(float value,float sigma) { const float t=value/sigma; return std::exp(-.5f*t*t); }
// Die Dichten werden nur beim Anlegen der unveraenderlichen Texturen ausgewertet.
inline float RingDensity(float x,float y) {
    const float r=std::hypot(x,y),a=std::atan2(y,x);
    const float bend=.008f*std::sin(a*3)+.004f*std::sin(a*7);
    const float mist=.22f*Gaussian(r-.68f,.065f);
    const float main=.85f*Gaussian(r-.68f-bend,.012f)*(.78f+.22f*std::sin(a*2+.6f));
    const float inner=.25f*Gaussian(r-.625f+.008f*std::sin(a*4),.008f);
    const float outer=.20f*Gaussian(r-.75f-.006f*std::cos(a*5),.01f);
    return std::clamp(mist+main+inner+outer,0.f,1.f);
}
inline float FeatherDensity(float x,float y) {
    const float t=(x+1)*.5f,s=std::max(0.f,std::sin(t*std::numbers::pi_v<float>));
    const float center=.13f*std::sin(t*std::numbers::pi_v<float>);
    return std::clamp(std::pow(s,.65f)*std::clamp((1-std::abs(y))/.15f,0.f,1.f)*(.56f*Gaussian(y-center,.16f*s+.025f)+.12f*Gaussian(y-center,.40f*s+.025f)),0.f,1.f);
}
inline ImU32 Ink(ImVec4 c,float alpha=1) { c.w*=std::clamp(alpha,0.f,1.f); return ImGui::ColorConvertFloat4ToU32(c); }
inline ImVec4 Mix(ImVec4 a,ImVec4 b,float t) { return {std::lerp(a.x,b.x,t),std::lerp(a.y,b.y,t),std::lerp(a.z,b.z,t),std::lerp(a.w,b.w,t)}; }
struct TextureScope {
    ImDrawList& draw; bool active;
    TextureScope(ImDrawList& d,ImTextureID texture):draw(d),active(texture!=0) { if(active) draw.PushTextureID(texture); }
    ~TextureScope() { if(active) draw.PopTextureID(); }
};
struct BlendScope {
    ImDrawList& draw; bool active;
    BlendScope(ImDrawList& d,AuraBlend blend):draw(d),active(blend.begin!=nullptr) { if(active) draw.AddCallback(blend.begin,blend.data); }
    ~BlendScope() { if(active) draw.AddCallback(ImDrawCallback_ResetRenderState,nullptr); }
};
inline void Orb(ImDrawList& d,ImTextureID texture,ImVec2 p,float radius,ImVec4 color,float glow=1,float distance=0) {
    if(!texture||!std::isfinite(radius)||radius<=0) return;
    const int first=d.VtxBuffer.Size;
    if(glow>0) d.AddImage(texture,{p.x-radius*3,p.y-radius*3},{p.x+radius*3,p.y+radius*3},{0,0},{1,1},Ink(color,.24f*glow));
    d.AddImage(texture,{p.x-radius,p.y-radius},{p.x+radius,p.y+radius},{0,0},{1,1},Ink(Mix(color,{1,1,1,color.w},.55f),.65f));
    depth::Tag(d,first,distance);
}
inline void Stroke(ImDrawList& d,ImVec2 a,ImVec2 b,float width,ImVec4 color,float glow,ImTextureID texture,float aDepth=0,float bDepth=0) {
    const float length=std::hypot(b.x-a.x,b.y-a.y); if(length<.001f||!std::isfinite(length)) return;
    if(!texture) { d.AddLine(a,b,Ink(color,.4f),width); return; }
    const ImVec2 n{-(b.y-a.y)/length,(b.x-a.x)/length};
    auto layer=[&](float radius,ImVec4 tint,float alpha) {
        const int first=d.VtxBuffer.Size;
        d.AddImageQuad(texture,{a.x+n.x*radius,a.y+n.y*radius},{b.x+n.x*radius,b.y+n.y*radius},
            {b.x-n.x*radius,b.y-n.y*radius},{a.x-n.x*radius,a.y-n.y*radius},
            {.48f,0},{.52f,0},{.52f,1},{.48f,1},Ink(tint,alpha));
        if(aDepth>0&&bDepth>0) for(int i=0;i<4;++i) depth::Vertex(d,first+i,1.f/((i==1||i==2)?bDepth:aDepth));
    };
    if(glow>0) layer(width*3.5f+3,color,.18f*glow);
    layer(std::max(.65f,width*1.1f),Mix(color,{1,1,1,color.w},.32f),.64f);
}
inline void Outline(ImDrawList& d,ImVec2 lo,ImVec2 hi,float rounding,float width,ImVec4 a,ImVec4 b,float glow,ImTextureID texture) {
    const float r=std::min({rounding,(hi.x-lo.x)*.5f,(hi.y-lo.y)*.5f});
    std::array<ImVec2,36> path{};
    const ImVec2 centers[]={{hi.x-r,lo.y+r},{hi.x-r,hi.y-r},{lo.x+r,hi.y-r},{lo.x+r,lo.y+r}};
    for(int corner=0;corner<4;++corner) for(int i=0;i<9;++i) {
        const float angle=(-.5f+corner*.5f+i/16.f)*std::numbers::pi_v<float>;
        path[corner*9+i]={centers[corner].x+r*std::cos(angle),centers[corner].y+r*std::sin(angle)};
    }
    for(int i=0;i<int(path.size());++i) { const auto first=path[i],next=path[(i+1)%path.size()];
        Stroke(d,first,next,width,Mix(a,b,std::clamp((first.y-lo.y)/std::max(1.f,hi.y-lo.y),0.f,1.f)),glow,texture);
    }
}
inline bool WorldQuad(ImDrawList& d,const CameraProjection& p,const std::array<WorldPoint,4>& world,ImTextureID texture,ImVec4 a,ImVec4 b,
                      ImVec2 uvLo={0,0},ImVec2 uvHi={1,1}) {
    if(!texture) return false;
    struct Vertex { WorldPoint world; ImVec2 screen,uv; ImVec4 color; float inverseDepth{}; };
    std::array<Vertex,12> polygon{},buffer{};
    const ImVec2 uv[]={{uvLo.x,uvLo.y},{uvHi.x,uvLo.y},{uvHi.x,uvHi.y},{uvLo.x,uvHi.y}};
    for(int i=0;i<4;++i) polygon[i]={world[i],{},uv[i],i==1||i==2?b:a};
    int count=4;
    auto interpolate=[](const Vertex& v,const Vertex& w,float t) {
        return Vertex{{std::lerp(v.world.x,w.world.x,double(t)),std::lerp(v.world.y,w.world.y,double(t)),std::lerp(v.world.z,w.world.z,double(t))},
            {std::lerp(v.screen.x,w.screen.x,t),std::lerp(v.screen.y,w.screen.y,t)},
            {std::lerp(v.uv.x,w.uv.x,t),std::lerp(v.uv.y,w.uv.y,t)},Mix(v.color,w.color,t),std::lerp(v.inverseDepth,w.inverseDepth,t)};
    };
    auto clip=[&](auto distance) {
        int output=0; auto previous=polygon[count-1]; float dp=distance(previous);
        for(int i=0;i<count;++i) { const auto current=polygon[i]; const float dc=distance(current);
            if(output>=int(buffer.size())-1) { count=0; return; }
            if(!std::isfinite(dc)||!std::isfinite(dp)) { count=0; return; }
            if((dc>=0)!=(dp>=0)) buffer[output++]=interpolate(previous,current,std::clamp(dp/(dp-dc),0.f,1.f));
            if(dc>=0) buffer[output++]=current;
            previous=current; dp=dc;
        }
        polygon=buffer; count=output;
    };
    // UVs an der Near Plane mitnehmen, damit der eigene Ring in F1 sichtbar bleibt.
    const float nearDepth=p.NearPlane()*1.002f;
    clip([&](const Vertex& v){return p.Depth(v.world)-nearDepth;}); if(count<3) return false;
    for(int i=0;i<count;++i) { ScreenPoint point; if(!p.Point(polygon[i].world,point)) return false; polygon[i].screen={point.x,point.y}; polygon[i].inverseDepth=1.f/p.Depth(polygon[i].world); }
    const auto lo=d.GetClipRectMin(),hi=d.GetClipRectMax();
    clip([&](const Vertex& v){return v.screen.x-lo.x;}); if(count<3) return false;
    clip([&](const Vertex& v){return hi.x-v.screen.x;}); if(count<3) return false;
    clip([&](const Vertex& v){return v.screen.y-lo.y;}); if(count<3) return false;
    clip([&](const Vertex& v){return hi.y-v.screen.y;}); if(count<3) return false;
    TextureScope scope(d,texture);
    const int first=d.VtxBuffer.Size;
    d.PrimReserve((count-2)*3,count); const auto base=d._VtxCurrentIdx;
    for(int i=1;i<count-1;++i) { d.PrimWriteIdx(ImDrawIdx(base)); d.PrimWriteIdx(ImDrawIdx(base+i)); d.PrimWriteIdx(ImDrawIdx(base+i+1)); }
    for(int i=0;i<count;++i) {
        d.PrimWriteVtx({std::clamp(polygon[i].screen.x,lo.x,hi.x),std::clamp(polygon[i].screen.y,lo.y,hi.y)},
            {std::clamp(polygon[i].uv.x,uvLo.x,uvHi.x),std::clamp(polygon[i].uv.y,uvLo.y,uvHi.y)},Ink(polygon[i].color));
        depth::Vertex(d,first+i,polygon[i].inverseDepth);
    }
    return true;
}
inline void Ring(ImDrawList& d,const CameraProjection& p,WorldPoint center,double radius,double rotation,
                 ImVec4 a,ImVec4 b,Textures textures,float glow=1) {
    if(!textures.ring) return;
    TextureScope texture(d,textures.ring);
    constexpr double coreRadius=.68;
    for(int layer=0;layer<3;++layer) {
        const double size=radius/coreRadius*(layer==0?1:layer==1?.91:1.10),angle=rotation*(layer==1?-.65:1);
        const double cs=std::cos(angle),sn=std::sin(angle); std::array<WorldPoint,4> corners{};
        constexpr double xs[]={-1,1,1,-1},zs[]={-1,-1,1,1};
        for(int i=0;i<4;++i) corners[i]={center.x+size*(xs[i]*cs-zs[i]*sn),center.y+.008*layer,center.z+size*(xs[i]*sn+zs[i]*cs)};
        auto ca=Mix(a,{1,1,1,a.w},.18f),cb=Mix(b,{1,1,1,b.w},.18f); const float opacity=layer==0?.9f:layer==1?.30f:.16f*glow;
        ca.w*=opacity; cb.w*=opacity;
        // Kleine Textur-Zellen halten die Bodenprojektion auch schraeg betrachtet rund.
        constexpr int grid=10;
        auto point=[&](float u,float v) { return WorldPoint{std::lerp(corners[0].x,corners[1].x,double(u))+(corners[3].x-corners[0].x)*v,
            center.y+.008*layer,std::lerp(corners[0].z,corners[1].z,double(u))+(corners[3].z-corners[0].z)*v}; };
        for(int y=0;y<grid;++y) for(int x=0;x<grid;++x) {
            const float u=float(x)/grid,v=float(y)/grid,uu=float(x+1)/grid,vv=float(y+1)/grid;
            const std::array<WorldPoint,4> cell={point(u,v),point(uu,v),point(uu,vv),point(u,vv)};
            WorldQuad(d,p,cell,textures.ring,Mix(ca,cb,u),Mix(ca,cb,uu),{u,v},{uu,vv});
        }
    }
}
} // namespace mcov::soft

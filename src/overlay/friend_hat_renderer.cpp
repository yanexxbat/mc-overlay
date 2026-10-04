#include "friend_hat_renderer.hpp"
#include <numbers>

namespace mcov {
namespace {
float Safe(float value,float fallback,float low,float high) noexcept {
    return std::clamp(std::isfinite(value)?value:fallback,low,high);
}
ImVec4 Color(ImVec4 color) noexcept {
    return {Safe(color.x,1,0,1),Safe(color.y,1,0,1),Safe(color.z,1,0,1),Safe(color.w,1,0,1)};
}
ImVec4 Mix(ImVec4 a,ImVec4 b,float t) noexcept {
    return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,a.w+(b.w-a.w)*t};
}
ImU32 Tint(ImVec4 color,float alpha) noexcept {
    color.w*=alpha; return ImGui::ColorConvertFloat4ToU32(color);
}
bool Finite(WorldPoint p) noexcept { return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z); }
void Triangle(ImDrawList& draw,ImVec2 a,ImVec2 b,ImVec2 c,ImU32 ca,ImU32 cb,ImU32 cc) {
    // Explizite Vertex-Farben geben dem transparenten Kegel einen ruhigen Verlauf.
    draw.PrimReserve(3,3);
    draw.PrimWriteIdx(static_cast<ImDrawIdx>(draw._VtxCurrentIdx));
    draw.PrimWriteIdx(static_cast<ImDrawIdx>(draw._VtxCurrentIdx+1));
    draw.PrimWriteIdx(static_cast<ImDrawIdx>(draw._VtxCurrentIdx+2));
    const auto uv=ImGui::GetFontTexUvWhitePixel();
    draw.PrimWriteVtx(a,uv,ca); draw.PrimWriteVtx(b,uv,cb); draw.PrimWriteVtx(c,uv,cc);
}
void Sprite(ImDrawList& draw,ImTextureID texture,ImVec2 center,float radius,ImVec4 color,float alpha) {
    const auto lo=draw.GetClipRectMin(),hi=draw.GetClipRectMax();
    if(radius<=0 || center.x+radius<lo.x || center.x-radius>hi.x || center.y+radius<lo.y || center.y-radius>hi.y) return;
    draw.AddImage(texture,{center.x-radius,center.y-radius},{center.x+radius,center.y+radius},
                  {0,0},{1,1},Tint(color,alpha));
}
class GlowScope final {
    ImDrawList& draw_;
public:
    GlowScope(ImDrawList& draw,AuraBlend blend):draw_(draw) {
        if(blend.begin) draw_.AddCallback(blend.begin,blend.data);
    }
    ~GlowScope() { draw_.AddCallback(ImDrawCallback_ResetRenderState,nullptr); }
};
} // namespace
FriendHatRenderer::FriendHatRenderer() noexcept {
    for(int i=0;i<kFriendHatSegments;++i) {
        const double angle=2*std::numbers::pi*i/kFriendHatSegments;
        circle_[i]={static_cast<float>(std::cos(angle)),static_cast<float>(std::sin(angle))};
    }
}
void FriendHatRenderer::Prepare(ImDrawList& draw) {
    constexpr int hats=static_cast<int>(combat::kRelationCapacity);
    draw.VtxBuffer.reserve(draw.VtxBuffer.Size+hats*1536);
    draw.IdxBuffer.reserve(draw.IdxBuffer.Size+hats*4096);
    draw.CmdBuffer.reserve(draw.CmdBuffer.Size+hats*6);
    draw._TextureIdStack.reserve(draw._TextureIdStack.Size+2);
}
bool FriendHatRenderer::IsFriend(const EntitySnapshot& e,const combat::Relations& relations) noexcept {
    if(!(e.flags&kEntityIsPlayer) || (e.flags&kEntityIsSelf) || (!e.uuid.high&&!e.uuid.low)) return false;
    for(const auto& entry:relations)
        if(entry.used && entry.relation==combat::Relation::Friend && entry.uuid==e.uuid) return true;
    return false;
}
bool FriendHatRenderer::BuildWorld(const EntitySnapshot& e,const FriendHatConfig& c,FriendHatWorld& out) const noexcept {
    if(!(e.flags&kEntityIsPlayer) || !(e.flags&kEntityIsLiving) || (e.flags&kEntityIsSelf) ||
       !std::isfinite(e.health) || e.health<=0 || !std::isfinite(e.width) || e.width<=0 ||
       !std::isfinite(e.height) || e.height<=0 || !std::isfinite(e.yaw) || !std::isfinite(e.headPitch)) return false;
    const double model=e.modelHeight>0?e.modelHeight:e.height;
    if(!std::isfinite(model) || model>32 || !Finite({e.x,e.y,e.z})) return false;
    const double radius=std::clamp(double(e.width)*Safe(c.radiusScale,.95f,.5f,2),.15,2.0);
    const double height=radius*Safe(c.heightScale,.55f,.2f,1);
    const double yaw=e.yaw*std::numbers::pi/180, pitch=e.headPitch*std::numbers::pi/180;
    const double cy=std::cos(yaw),sy=std::sin(yaw),cp=std::cos(pitch),sp=std::sin(pitch);
    const bool horizontal=e.pose==kPoseSwimming || e.pose==kPoseGliding || e.pose==kPoseSleeping;
    auto rotate=[&](WorldPoint p) {
        if(horizontal) p={p.x,-p.z,p.y};
        return WorldPoint{p.x*cy-p.z*sy,p.y,p.x*sy+p.z*cy};
    };
    const WorldPoint up=rotate({0,cp,sp}), side=rotate({1,0,0}), forward=rotate({0,-sp,cp});
    WorldPoint head{};
    if(e.rigKind==kRigHumanoid) head=BuildHumanoidRig(e).joints[0];
    else {
        const auto bounds=EntityBounds(e);
        head={(bounds.min.x+bounds.max.x)*.5,bounds.max.y-model*.13,(bounds.min.z+bounds.max.z)*.5};
    }
    if(!Finite(head)) return false;
    const double lift=model*.10+Safe(c.headGap,.025f,0,.2f);
    const WorldPoint base{head.x+up.x*lift,head.y+up.y*lift,head.z+up.z*lift};
    out.tip={base.x+up.x*height,base.y+up.y*height,base.z+up.z*height};
    out.glowRadius=static_cast<float>(radius*.14);
    for(int i=0;i<kFriendHatSegments;++i) {
        const auto v=circle_[i];
        out.brim[i]={base.x+radius*(side.x*v.x+forward.x*v.y),base.y+radius*(side.y*v.x+forward.y*v.y),
                     base.z+radius*(side.z*v.x+forward.z*v.y)};
    }
    return true;
}
bool FriendHatRenderer::Project(const CameraProjection& projection,const FriendHatWorld& world,
                                WorldPoint camera,ImVec2 viewport,FriendHatScreen& out,ImVec2 origin) noexcept {
    if(!Finite(camera) || !std::isfinite(viewport.x) || !std::isfinite(viewport.y) || viewport.x<1 || viewport.y<1) return false;
    ScreenPoint tip{};
    if(!projection.Billboard(world.tip,world.glowRadius,tip,out.tipGlowRadius)) return false;
    out.tip={tip.x+origin.x,tip.y+origin.y};
    float left=tip.x,right=tip.x,top=tip.y,bottom=tip.y;
    for(int i=0;i<kFriendHatSegments;++i) {
        ScreenPoint p{};
        // Ein Kegel an der Near Plane wird vollständig übersprungen statt riesige Dreiecke zu erzeugen.
        if(!projection.Billboard(world.brim[i],world.glowRadius,p,out.glowRadius[i])) return false;
        out.brim[i]={p.x+origin.x,p.y+origin.y};
        left=std::min(left,p.x); right=std::max(right,p.x); top=std::min(top,p.y); bottom=std::max(bottom,p.y);
        const auto& next=world.brim[(i+1)%kFriendHatSegments];
        const auto& a=world.brim[i];
        out.depth[i]=std::hypot((a.x+next.x+world.tip.x)/3-camera.x,(a.y+next.y+world.tip.y)/3-camera.y,
                               (a.z+next.z+world.tip.z)/3-camera.z);
    }
    const float margin=std::max(out.tipGlowRadius,*std::max_element(out.glowRadius.begin(),out.glowRadius.end()))*2;
    return right+margin>=0 && left-margin<=viewport.x && bottom+margin>=0 && top-margin<=viewport.y &&
           right-left<=viewport.x*2 && bottom-top<=viewport.y*2;
}
void FriendHatRenderer::Draw(ImDrawList& draw,const FriendHatScreen& s,const FriendHatConfig& c,
                             ImVec4 start,ImVec4 end,AuraBlend blend,ImTextureID texture) {
    start=Color(start); end=Color(end);
    const float opacity=Safe(c.opacity,.28f,0,.65f), glow=Safe(c.glow,.65f,0,1);
    std::array<int,kFriendHatSegments> order{};
    std::array<ImVec4,kFriendHatSegments> colors{};
    for(int i=0;i<kFriendHatSegments;++i) {
        order[i]=i;
        const float t=.5f+.5f*static_cast<float>(std::sin(2*std::numbers::pi*i/kFriendHatSegments));
        colors[i]=Mix(start,end,t);
    }
    // Transparente Rückseiten zuerst; stabile Reihenfolge ohne Heap-Allokationen.
    std::sort(order.begin(),order.end(),[&](int a,int b) { return s.depth[a]==s.depth[b]?a<b:s.depth[a]>s.depth[b]; });
    const auto apex=Mix(start,ImVec4(1,1,1,1),.55f);
    for(int i:order) {
        const int j=(i+1)%kFriendHatSegments;
        Triangle(draw,s.tip,s.brim[i],s.brim[j],Tint(apex,opacity*.8f),Tint(colors[i],opacity),Tint(colors[j],opacity));
    }
    if(!texture || glow<=0) return;
    GlowScope scope(draw,blend);
    for(int i=0;i<kFriendHatSegments;++i) {
        Sprite(draw,texture,s.brim[i],s.glowRadius[i]*2,colors[i],glow*.025f);
        Sprite(draw,texture,s.brim[i],s.glowRadius[i],colors[i],glow*.11f);
    }
    Sprite(draw,texture,s.tip,s.tipGlowRadius*2,start,glow*.12f);
    Sprite(draw,texture,s.tip,s.tipGlowRadius*.7f,apex,glow*.45f);
    for(int i=0;i<kFriendHatSegments;++i) {
        draw.AddLine(s.brim[i],s.brim[(i+1)%kFriendHatSegments],Tint(Mix(colors[i],ImVec4(1,1,1,1),.35f),glow*.8f),1.25f);
        if(i%8==0) draw.AddLine(s.tip,s.brim[i],Tint(colors[i],glow*.13f),1.f);
    }
}
void FriendHatRenderer::DrawFriends(ImDrawList& draw,const WorldSnapshot& snapshot,const CameraProjection& projection,
                                    const combat::Relations& relations,float maxDistance,const FriendHatConfig& config,
                                    ImVec4 start,ImVec4 end,AuraBlend blend,ImTextureID texture,
                                    ImVec2 viewport,ImVec2 origin) const {
    std::array<const EntitySnapshot*,kMaxEntities> friends{};
    std::size_t count=0;
    for(std::uint32_t i=0;i<std::min(snapshot.entityCount,kMaxEntities) && count<combat::kRelationCapacity;++i) {
        const auto& e=snapshot.entities[i];
        if(IsFriend(e,relations) && std::isfinite(e.distance) && e.distance>=0 && e.distance<=maxDistance) friends[count++]=&e;
    }
    std::sort(friends.begin(),friends.begin()+count,[](auto a,auto b) { return a->distance>b->distance; });
    for(std::size_t i=0;i<count;++i) {
        FriendHatWorld world{}; FriendHatScreen screen{};
        if(BuildWorld(*friends[i],config,world) && Project(projection,world,{snapshot.camX,snapshot.camY,snapshot.camZ},viewport,screen,origin))
            Draw(draw,screen,config,start,end,blend,texture);
    }
}
} // namespace mcov

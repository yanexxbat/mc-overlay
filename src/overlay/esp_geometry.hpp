#pragma once
#include "render_math.hpp"
#include "../common/shared_state.hpp"
#include <algorithm>
#include <array>
#include <limits>

namespace mcov {
struct WorldPoint { double x, y, z; };
struct ScreenPoint { float x, y; };
struct ScreenRect { float left, top, right, bottom; };
struct WorldBounds { WorldPoint min, max; };
inline WorldBounds EntityBounds(const EntitySnapshot& e) {
    if (e.flags & kEntityHasBounds)
        return {{e.x + e.boundsMinX, e.y + e.boundsMinY, e.z + e.boundsMinZ},
                {e.x + e.boundsMaxX, e.y + e.boundsMaxY, e.z + e.boundsMaxZ}};
    const double half = e.width * .5;
    return {{e.x-half,e.y,e.z-half}, {e.x+half,e.y+e.height,e.z+half}};
}

// Origin subtraction happens in double precision BEFORE the float matrix.
// A yaw/pitch basis stays orthonormal even when looking straight up or down.
class CameraProjection {
    struct ClipPoint { float x, y, w; };
    Mat4 matrix_{};
    WorldPoint origin_{};
    float width_{}, height_{}, near_{};
    bool valid_{};
    ClipPoint Clip(WorldPoint p) const {
        const float x = static_cast<float>(p.x-origin_.x);
        const float y = static_cast<float>(p.y-origin_.y);
        const float z = static_cast<float>(p.z-origin_.z);
        const auto& m = matrix_.m;
        return {m[0]*x+m[4]*y+m[8]*z+m[12], m[1]*x+m[5]*y+m[9]*z+m[13],
                m[3]*x+m[7]*y+m[11]*z+m[15]};
    }
    static bool Finite(ClipPoint p) { return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.w); }
    ScreenPoint Pixel(ClipPoint p) const { return {(p.x/p.w*.5f+.5f)*width_, (.5f-p.y/p.w*.5f)*height_}; }
    bool ClipNear(ClipPoint& a, ClipPoint& b) const {
        if (!Finite(a)||!Finite(b)||(a.w<near_ && b.w<near_)) return false;
        if ((a.w<near_) != (b.w<near_)) {
            const float t=(near_-a.w)/(b.w-a.w);
            const ClipPoint p{a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,near_};
            if (a.w<near_) a=p; else b=p;
        }
        return true;
    }
public:
    CameraProjection(const WorldSnapshot& s, float width, float height, float fovTrim=0) :
        origin_{s.camX,s.camY,s.camZ}, width_(width),height_(height),
        near_(s.nearPlane>0 ? s.nearPlane : .05f) {
        if (!std::isfinite(width)||!std::isfinite(height)||width<1||height<1||
            !std::isfinite(s.yawDeg)||!std::isfinite(s.pitchDeg)||!std::isfinite(s.fovDeg+fovTrim)) return;
        constexpr float rad=3.14159265358979323846f/180;
        const float y=s.yawDeg*rad,p=s.pitchDeg*rad;
        const float sy=std::sin(y),cy=std::cos(y),sp=std::sin(p),cp=std::cos(p);
        Mat4 view=Mat4::Identity();
        view.m[0]=-cy; view.m[4]=0; view.m[8]=-sy;
        view.m[1]=-sy*sp; view.m[5]=cp; view.m[9]=cy*sp;
        view.m[2]=sy*cp; view.m[6]=sp; view.m[10]=-cy*cp;
        if (s.viewBobbing && std::isfinite(s.bobPhase) && std::isfinite(s.bobAmplitude)) {
            const float phase=s.bobPhase*3.14159265358979323846f,amp=s.bobAmplitude;
            Mat4 t=Mat4::Identity(),rz=t,rx=t;
            t.m[12]=std::sin(phase)*amp*.5f; t.m[13]=-std::abs(std::cos(phase)*amp);
            const float z=std::sin(phase)*amp*3*rad, x=std::abs(std::cos(phase-.2f)*amp)*5*rad;
            rz.m[0]=rz.m[5]=std::cos(z); rz.m[1]=std::sin(z); rz.m[4]=-std::sin(z);
            rx.m[5]=rx.m[10]=std::cos(x); rx.m[6]=std::sin(x); rx.m[9]=-std::sin(x);
            view=Multiply(Multiply(Multiply(t,rz),rx),view);
        }
        matrix_=Multiply(Perspective(std::clamp(s.fovDeg+fovTrim,1.f,179.f),width/height,near_,
                         std::max(s.farPlane,near_+1)),view);
        valid_=true;
    }
    bool Point(WorldPoint p, ScreenPoint& out) const {
        auto c=Clip(p);
        if (!valid_||!Finite(c)||c.w<near_) return false;
        out=Pixel(c); return std::isfinite(out.x)&&std::isfinite(out.y);
    }
    bool Segment(WorldPoint start,WorldPoint end,ScreenPoint& a,ScreenPoint& b) const {
        auto ca=Clip(start),cb=Clip(end);
        if (!valid_||!ClipNear(ca,cb)) return false;
        a=Pixel(ca); b=Pixel(cb);
        if (!std::isfinite(a.x)||!std::isfinite(a.y)||!std::isfinite(b.x)||!std::isfinite(b.y)) return false;
        // Liang-Barsky viewport clipping; no giant off-screen draw commands.
        const float dx=b.x-a.x,dy=b.y-a.y;
        float low=0,high=1;
        const float ps[4]={-dx,dx,-dy,dy},qs[4]={a.x,width_-a.x,a.y,height_-a.y};
        for (int i=0;i<4;++i) {
            if (std::abs(ps[i])<1e-8f) { if (qs[i]<0) return false; }
            else { const float t=qs[i]/ps[i]; if (ps[i]<0) low=std::max(low,t); else high=std::min(high,t); }
        }
        if (low>high) return false;
        b={a.x+dx*high,a.y+dy*high}; a={a.x+dx*low,a.y+dy*low};
        return true;
    }
    bool Bounds(WorldBounds bounds,ScreenRect& out) const {
        if (!valid_ || bounds.min.x>=bounds.max.x || bounds.min.y>=bounds.max.y || bounds.min.z>=bounds.max.z) return false;
        if (origin_.x>bounds.min.x && origin_.x<bounds.max.x && origin_.y>bounds.min.y &&
            origin_.y<bounds.max.y && origin_.z>bounds.min.z && origin_.z<bounds.max.z) return false;
        std::array<ClipPoint,8> c;
        for (int i=0;i<8;++i) c[i]=Clip({i&1?bounds.max.x:bounds.min.x,i&2?bounds.max.y:bounds.min.y,i&4?bounds.max.z:bounds.min.z});
        out={std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),
             -std::numeric_limits<float>::max(),-std::numeric_limits<float>::max()};
        bool found=false;
        auto include=[&](ClipPoint v) {
            if (!Finite(v)||v.w<near_) return;
            const auto p=Pixel(v);
            if (!std::isfinite(p.x)||!std::isfinite(p.y)) return;
            found=true;
            out.left=std::min(out.left,p.x); out.right=std::max(out.right,p.x);
            out.top=std::min(out.top,p.y); out.bottom=std::max(out.bottom,p.y);
        };
        for (int i=0;i<8;++i) {
            include(c[i]);
            for (int bit : {1,2,4}) if (!(i&bit)) {
                auto a=c[i],b=c[i|bit];
                if (ClipNear(a,b)) { include(a); include(b); }
            }
        }
        if (!found||out.right<0||out.bottom<0||out.left>width_||out.top>height_) return false;
        out.left=std::clamp(out.left,0.f,width_); out.right=std::clamp(out.right,0.f,width_);
        out.top=std::clamp(out.top,0.f,height_); out.bottom=std::clamp(out.bottom,0.f,height_);
        return out.right-out.left>=1 && out.bottom-out.top>=1;
    }
};

struct HumanoidRig {
    // head, neck, pelvis, shoulders, elbows, hands, hips, knees, feet
    std::array<WorldPoint,15> joints;
    inline static constexpr int edges[][2]={{0,1},{1,2},{3,4},{1,3},{1,4},{3,5},{5,7},{4,6},{6,8},
                                            {9,10},{2,9},{2,10},{9,11},{11,13},{10,12},{12,14}};
};
// Vanilla-sized humanoid rig reconstructed from sampled animation. Arbitrary
// renderer/model mods and item-specific arm animations need their own adapters.
inline HumanoidRig BuildHumanoidRig(const EntitySnapshot& e) {
    HumanoidRig rig{};
    constexpr double rad=3.14159265358979323846/180;
    const double h=std::max(.1f,e.modelHeight>0?e.modelHeight:e.height);
    const bool ender=e.rigKind==kRigEnderman,crouch=e.pose==kPoseCrouching;
    // EndermanModel uses a raised head/body and 30-pixel limbs.
    const double leg=h*(ender?.625:.39),torso=h*(ender?.173:.39);
    const double arm=h*(ender?.647:.39),width=h*(ender?.216:.325);
    const double hipY=leg-(crouch?h*.09:0),lean=crouch?.5:0;
    const double legLength=hipY;
    const double shoulderY=hipY+torso*std::cos(lean),shoulderZ=torso*std::sin(lean);
    const double armRootY=shoulderY-h*(ender?.02155:.065),armRootZ=shoulderZ;
    auto local=[&](int i,double x,double y,double z) { rig.joints[i]={x,y,z}; };
    local(2,0,hipY,0); local(1,0,shoulderY,shoulderZ);
    const double hp=e.headPitch*rad;
    const double headRadius=h*(ender?.086:.13);
    local(0,0,shoulderY+headRadius*std::cos(hp),shoulderZ+headRadius*std::sin(hp));
    const double swing=std::clamp(e.walkSpeed,0.f,1.f),phase=e.walkPhase*.6662;
    for (int side=0;side<2;++side) {
        const double sign=side==0?-1:1;
        double armAngle=std::cos(phase+(side==0?3.14159265358979323846:0))*swing;
        const bool riding=(e.rigFlags&kRigRiding)!=0;
        double legAngle=riding?-1.1:std::cos(phase+(side==0?0:3.14159265358979323846))*1.4*swing;
        if (ender) { armAngle=std::clamp(armAngle*.5,-.4,.4); legAngle=std::clamp(legAngle*.5,-.4,.4); }
        const double shoulderX=sign*width*.5,hipX=sign*width*.22;
        local(3+side,shoulderX,armRootY,armRootZ);
        local(5+side,shoulderX,armRootY-arm*.5*std::cos(armAngle),armRootZ-arm*.5*std::sin(armAngle));
        local(7+side,shoulderX,armRootY-arm*std::cos(armAngle),armRootZ-arm*std::sin(armAngle));
        local(9+side,hipX,hipY,0);
        local(11+side,hipX,hipY-legLength*.5*std::cos(legAngle),-legLength*.5*std::sin(legAngle));
        if (riding) local(13+side,hipX,rig.joints[11+side].y-legLength*.5,rig.joints[11+side].z);
        else local(13+side,hipX,hipY-legLength*std::cos(legAngle),-legLength*std::sin(legAngle));
    }
    const double yaw=e.yaw*rad,cy=std::cos(yaw),sy=std::sin(yaw);
    const bool horizontal=e.pose==kPoseSwimming||e.pose==kPoseGliding||e.pose==kPoseSleeping;
    for (auto& p:rig.joints) {
        if (horizontal) {
            const double y=p.y,z=p.z;
            p.y=e.height*.5-z; p.z=y-h*.5;
        }
        p={e.x+p.x*cy-p.z*sy,e.y+p.y,e.z+p.x*sy+p.z*cy};
    }
    return rig;
}
} // namespace mcov

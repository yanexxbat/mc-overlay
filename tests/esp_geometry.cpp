#include "../src/overlay/esp_geometry.hpp"
#include <iostream>
#include <stdexcept>

using namespace mcov;
namespace {
void Require(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
bool Close(double a,double b,double eps=.002) { return std::abs(a-b)<eps; }
WorldSnapshot Camera() { WorldSnapshot s{}; s.fovDeg=90; s.nearPlane=.05f; s.farPlane=1000; return s; }
void ProjectionChecks() {
    auto s=Camera(); CameraProjection p(s,1600,900);
    ScreenPoint point;
    Require(p.Point({0,0,10},point)&&Close(point.x,800)&&Close(point.y,450),"forward point not centered");
    Require(p.Point({1,1,10},point)&&Close(point.x,755)&&Close(point.y,405),"Minecraft camera axis or FOV is wrong");
    Require(!p.Point({0,0,-10},point),"point behind camera was projected");
    Require(!p.Point({std::numeric_limits<double>::quiet_NaN(),0,10},point),"NaN reached renderer");
    ScreenRect r;
    Require(p.Bounds({{-.3,0,9.7},{.3,1.8,10.3}},r),"visible box rejected");
    Require(Close(r.left,800-450*.3/9.7)&&Close(r.right,800+450*.3/9.7)&&
            Close(r.top,450-450*1.8/9.7)&&Close(r.bottom,450),"AABB corners do not enclose true entity geometry");
    ScreenRect offset;
    Require(p.Bounds({{2.7,0,9.7},{3.3,1.8,10.3}},offset),"off-center entity rejected");
    Require(offset.right-offset.left>r.right-r.left+5,"box width still inferred from height");
    ScreenRect far;
    Require(p.Bounds({{-.3,0,19.7},{.3,1.8,20.3}},far)&&far.bottom-far.top<.51*(r.bottom-r.top),"distance scaling incorrect");
    for (float pitch : {-90.f,-60.f,0.f,60.f,90.f}) for (float yaw : {-180.f,-90.f,0.f,90.f,180.f}) {
        s.yawDeg=yaw; s.pitchDeg=pitch;
        float x,y,z; DirectionFromMinecraftAngles(yaw,pitch,x,y,z);
        CameraProjection angled(s,1920,1080);
        Require(angled.Point({x*10,y*10,z*10},point)&&Close(point.x,960)&&Close(point.y,540),"camera basis degenerates at steep pitch");
    }
    s=Camera(); s.pitchDeg=30;
    CameraProjection tilted(s,1600,900);
    ScreenPoint foot{},head{};
    Require(tilted.Point({0,-5,10},foot)&&tilted.Point({0,-3.2,10},head),"tilted entity rejected");
    Require(tilted.Bounds({{-.3,-5,9.7},{.3,-3.2,10.3}},r)&&r.top<head.y&&r.bottom>foot.y,"tilted box does not enclose depth corners");
    s=Camera(); p=CameraProjection(s,1600,900);
    Require(!p.Bounds({{-.3,0,-10},{.3,1.8,-9}},r),"behind-camera AABB visible");
    Require(p.Bounds({{.01,-.1,-.1},{.15,.1,.2}},r)&&r.left>=0&&r.right<=1600&&r.top>=0&&r.bottom<=900,
            "near-plane intersection popped or produced huge coordinates");
    ScreenPoint a,b;
    Require(p.Segment({0,0,-1},{0,0,5},a,b)&&Close(a.x,800)&&Close(b.x,800),"bone near-plane clipping failed");
    Require(p.Segment({-100,0,5},{100,0,5},a,b)&&Close(a.x,1600)&&Close(b.x,0),"bone viewport clipping failed");
    Require(!p.Segment({100,0,5},{110,0,5},a,b),"offscreen bone not rejected");
    Require(p.Bounds({{-.3,0,9.7},{.3,1.8,10.3}},r),"origin box missing");
    s.camX=29999990.125; s.camY=81.375; s.camZ=-29999990.75;
    CameraProjection distant(s,1600,900);
    Require(distant.Bounds({{s.camX-.3,s.camY,s.camZ+9.7},{s.camX+.3,s.camY+1.8,s.camZ+10.3}},far)&&
            Close(r.left,far.left)&&Close(r.top,far.top)&&Close(r.right,far.right)&&Close(r.bottom,far.bottom),
            "world-border precision lost before camera subtraction");
    s=Camera(); s.viewBobbing=1; s.bobPhase=.25f; s.bobAmplitude=.15f;
    CameraProjection bob(s,1600,900);
    const double phase=.25*3.14159265358979323846,amp=.15,rad=3.14159265358979323846/180;
    const double rx=std::abs(std::cos(phase-.2)*amp)*5*rad,rz=std::sin(phase)*amp*3*rad;
    const double rotatedY=10*std::sin(rx),depth=10*std::cos(rx);
    const double bobX=-rotatedY*std::sin(rz)+std::sin(phase)*amp*.5;
    const double bobY=rotatedY*std::cos(rz)-std::abs(std::cos(phase)*amp);
    Require(bob.Point({0,0,10},point)&&Close(point.x,800+450*bobX/depth)&&Close(point.y,450-450*bobY/depth),
            "view bob rotation/translation order is incorrect");
    auto invalid=Camera(); invalid.fovDeg=std::numeric_limits<float>::quiet_NaN();
    Require(!CameraProjection(invalid,1600,900).Point({0,0,10},point),"invalid camera not rejected");
}
void RigChecks() {
    EntitySnapshot e{}; e.x=100; e.y=64; e.z=200; e.width=.6f; e.height=e.modelHeight=1.8f; e.rigKind=kRigHumanoid;
    auto idle=BuildHumanoidRig(e);
    Require(Close(idle.joints[13].y,64)&&Close(idle.joints[14].y,64),"idle feet not at entity anchor");
    Require(Close(idle.joints[2].x,100)&&Close(idle.joints[2].z,200),"pelvis drifted from entity");
    Require(idle.joints[3].x<100&&idle.joints[4].x>100,"humanoid shoulders collapsed");
    Require(Close(idle.joints[3].y,64+1.8*.715),"arm roots are above vanilla shoulders");
    e.headPitch=30; auto lookingDown=BuildHumanoidRig(e);
    Require(lookingDown.joints[0].z>idle.joints[0].z&&lookingDown.joints[0].y<idle.joints[0].y,"head pitch uses the wrong axis");
    e.headPitch=0;
    e.yaw=90; auto turned=BuildHumanoidRig(e);
    Require(Close(turned.joints[3].x,100)&&turned.joints[3].z<200&&turned.joints[4].z>200,"skeleton does not follow body yaw");
    e.yaw=0; e.walkSpeed=.5f; auto walking=BuildHumanoidRig(e);
    Require(walking.joints[13].z<200&&walking.joints[14].z>200&&walking.joints[7].z>200,"limb gait not anchored in world space");
    e.walkSpeed=0; e.pose=kPoseCrouching; e.height=1.5f;
    auto crouching=BuildHumanoidRig(e);
    Require(crouching.joints[0].y<idle.joints[0].y&&crouching.joints[1].z>200&&Close(crouching.joints[13].y,64),
            "crouched rig lost feet anchor or standing model proportions");
    e.pose=kPoseSwimming; e.height=.6f;
    auto swimming=BuildHumanoidRig(e);
    Require(swimming.joints[0].z>swimming.joints[13].z&&Close(swimming.joints[0].y,64.3),"swimming rig not horizontal");
    e.pose=kPoseStanding; e.height=1.8f; e.rigFlags=kRigRiding;
    auto riding=BuildHumanoidRig(e);
    Require(riding.joints[11].z>200&&Close(riding.joints[11].z,riding.joints[13].z),"riding knees did not bend");
    e.rigFlags=0; e.rigKind=kRigEnderman; e.modelHeight=e.height=2.9f;
    auto ender=BuildHumanoidRig(e);
    Require(Close(ender.joints[2].y,64+2.9*.625)&&Close(ender.joints[0].y,64+2.9*.884)&&
            Close(ender.joints[3].y,64+2.9*.77645)&&Close(ender.joints[13].y,64),"enderman proportions are wrong");
    e.flags=kEntityHasBounds; e.boundsMinX=-.4f; e.boundsMinY=.1f; e.boundsMinZ=-.2f;
    e.boundsMaxX=.4f; e.boundsMaxY=2; e.boundsMaxZ=.2f;
    auto bounds=EntityBounds(e);
    Require(Close(bounds.min.x,99.6)&&Close(bounds.min.y,64.1)&&Close(bounds.max.z,200.2),"actual AABB offsets ignored");
}
}
int main() {
    try { ProjectionChecks(); RigChecks(); std::cout<<"PASS ESP projection, clipping, precision and humanoid rig\n"; return 0; }
    catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}

#include "overlay/rockstar_features.hpp"
#include <iostream>
#include <stdexcept>
#include <limits>
#include <memory>
#include <windows.h>
using namespace mcov;
using namespace mcov::rockstar;
void Require(bool v,const char* why) { if(!v) throw std::runtime_error(why); }
int main() {
    try {
        Config c; for(auto& o:c.modules) Require(!o.enabled,"new modules must default off");
        Require(soft::RingDensity(0,0)<.0001f&&soft::RingDensity(.68f,0)>.4f&&soft::RingDensity(1,0)<.001f,"soft ring lost its clear center or transparent edge");
        Require(soft::FeatherDensity(0,0)>.3f&&soft::FeatherDensity(0,1)==0,"feather mask has a hard rectangular edge");
        c[Feature::JumpCircles].enabled=c[Feature::DeathCoordinates].enabled=c[Feature::DamageMarkers].enabled=c[Feature::HitSounds].enabled=true;
        c[Feature::GlowEsp].size=std::numeric_limits<float>::quiet_NaN(); c[Feature::Wings].opacity=99; c.Clamp();
        Require(std::isfinite(c[Feature::GlowEsp].size)&&c[Feature::Wings].opacity==1,"invalid config was not bounded");
        auto s=WorldSnapshot{}; CopyField(s.dimension,"Overworld"); CopyField(s.serverAddress,"test");
        s.frameIndex=1; s.health=s.maxHealth=20; s.nearPlane=.05f; s.farPlane=1024; s.fovDeg=70; s.camY=1.62;
        s.entityCount=1; auto& e=s.entities[0]; e.entityId=42; e.uuid={1,2}; e.flags=kEntityIsPlayer|kEntityIsLiving;
        e.health=e.maxHealth=20; e.height=e.modelHeight=1.8f; e.width=.6f; e.z=4; e.distance=4; e.rigKind=kRigHumanoid; CopyField(e.name,"Test player");
        Pack pack; pack.Update(s,10,c,42); Require(pack.DamageCount(10,2)==0,"initial observation generated damage");
        s.frameIndex++; e.health=16; s.posY=.2f; pack.Update(s,10.05,c,42);
        Require(pack.DamageCount(10.05,2)==1&&pack.TakeHitSound(),"sampled target damage missing");
        Require(pack.RingCount(10.05,2)==1,"jump did not generate a ring");
        pack.Update(s,10.06,c,42); Require(pack.DamageCount(10.06,2)==1&&!pack.TakeHitSound(),"same frame generated duplicate events");
        s.frameIndex++; s.health=0; pack.Update(s,10.1,c,42);
        Require(pack.Markers()[0].used&&pack.Markers()[0].death,"death position not recorded");
        s.frameIndex++; s.health=20; pack.Update(s,10.15,c,42); Require(pack.Markers()[0].used,"respawn erased death position");
        keys::Input input{}; input[1]=true; pack.Input(input,true,10.16); pack.Input(input,true,10.17);
        Require(pack.Metrics(10.2).leftCps==1,"held mouse button counted multiple clicks");
        input[1]=false; pack.Input(input,true,10.3); input[1]=true; pack.Input(input,true,10.4);
        Require(pack.Metrics(10.5).leftCps==2&&pack.Metrics(11.5).leftCps==0,"CPS sliding window failed");
        s.frameIndex++; s.posX=1000; pack.Update(s,10.5,c,42); Require(pack.Metrics(10.5).speed==0,"teleport reported extreme speed");
        CopyField(s.dimension,"Nether"); s.frameIndex++; e.health=1; pack.Update(s,10.6,c,42);
        Require(pack.DamageCount(10.6,2)==0&&pack.RingCount(10.6,2)==0&&pack.Markers()[0].used,"world transition leaked effects or erased saved marker");
        Require(std::string(Pack::DisplayName("Secret",true,true))=="Player","privacy alias failed");
        Require(kBlockKindCount<=32&&std::string(BlockKindKey(kBlockBeacon))=="beacon","scanner mask/schema mismatch");
        // HUD-Position bleibt 100 ms alt, die eigene Entity bewegt sich trotzdem jedes Frame.
        auto moving=std::make_unique<WorldSnapshot>(); CopyField(moving->dimension,"Overworld");
        moving->entityCount=1; moving->health=moving->maxHealth=20;
        auto& self=moving->entities[0]; self.flags=kEntityIsSelf|kEntityIsLiving|kEntityIsPlayer;
        self.x=1000; self.y=64; self.health=20; moving->frameIndex=1;
        LARGE_INTEGER qpcFrequency{}; QueryPerformanceFrequency(&qpcFrequency);
        const auto step=qpcFrequency.QuadPart/100;
        moving->captureQpc=qpcFrequency.QuadPart*5000;
        Pack meter; double delivered=100; meter.Update(*moving,delivered,c,0);
        for(int i=0;i<30;++i) {
            ++moving->frameIndex; moving->captureQpc+=step;
            self.x+=4.*double(step)/double(qpcFrequency.QuadPart);
            delivered+=(i&1)?.004:.025;
            meter.Update(*moving,delivered,c,0);
            Require(std::abs(meter.Metrics(delivered).speed-4.f)<.001f,"speedometer used stale HUD positions or IPC delivery time");
        }
        --moving->frameIndex; self.x=0; meter.Update(*moving,delivered+.01,c,0);
        Require(std::abs(meter.Metrics(delivered+.01).speed-4.f)<.001f,"older GPU snapshot overwrote live speed");
        ImGui::CreateContext(); auto& io=ImGui::GetIO(); io.DisplaySize={640,480}; io.DeltaTime=1.f/60;
        unsigned char* pixels=nullptr; int width=0,height=0; io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
        for(auto& o:c.modules) o.enabled=true; c.Clamp();
        s.posX=0; CopyField(s.dimension,"Overworld"); s.frameIndex++; e.health=20; pack.Update(s,11,c,42);
        ImGui::NewFrame(); auto* draw=ImGui::GetBackgroundDrawList(); const CameraProjection projection(s,640,480);
        pack.DrawWorld(*draw,s,projection,c,{},42,{640,480},{.66f,.42f,1,1},{.05f,1,.88f,1},{},{},true);
        pack.DrawScreen(*draw,s,projection,c,{640,480},{.66f,.42f,1,1},{.05f,1,.88f,1},11);
        for(int i=int(HudModule::Radar);i<kHudModules;++i) pack.DrawHud(*draw,static_cast<HudModule>(i),s,c,{}, {232,200},11);
        ImGui::Render(); Require(ImGui::GetDrawData()->TotalVtxCount>0,"feature pack rendered nothing");
        for(auto* list:ImGui::GetDrawData()->CmdLists) for(const auto& vertex:list->VtxBuffer) Require(std::isfinite(vertex.pos.x)&&std::isfinite(vertex.pos.y),"nonfinite feature geometry");
        ImGui::NewFrame(); draw=ImGui::GetBackgroundDrawList();
        auto clipScene=std::make_unique<WorldSnapshot>(s); clipScene->camY=.2; clipScene->yawDeg=clipScene->pitchDeg=0;
        const CameraProjection nearProjection(*clipScene,640,480);
        const std::array<WorldPoint,4> ground={WorldPoint{-.5,0,-1},{.5,0,-1},{.5,0,1},{-.5,0,1}};
        const auto textureDepth=draw->_TextureIdStack.Size,clipDepth=draw->_ClipRectStack.Size;
        Require(soft::WorldQuad(*draw,nearProjection,ground,ImTextureID(1),{.4f,1,.6f,1},{.4f,1,.6f,1}),"near-plane clipping removed the visible part of a ground ring");
        for(const auto& v:draw->VtxBuffer) Require(v.pos.x>=0&&v.pos.x<=640.01f&&v.pos.y>=0&&v.pos.y<=480.01f&&v.uv.x>=0&&v.uv.x<=1&&v.uv.y>=0&&v.uv.y<=1,"clipped texture geometry escaped viewport or UV bounds");
        Require(draw->_TextureIdStack.Size==textureDepth&&draw->_ClipRectStack.Size==clipDepth,"soft texture clipping leaked drawing state"); ImGui::Render();
        // Ein bereits erfasster Spieler muss auch beim spaeteren Eintritt in die Reichweite melden.
        Pack nearby; e.distance=50; s.frameIndex=90; nearby.Update(s,14,c,42);
        const auto noticeVertices=[&](double time) {
            ImGui::NewFrame(); auto* list=ImGui::GetBackgroundDrawList(); nearby.DrawHud(*list,HudModule::Notifications,s,c,{}, {232,145},time);
            const int count=list->VtxBuffer.Size; ImGui::Render(); return count;
        };
        const int empty=noticeVertices(14); e.distance=16; s.frameIndex++; nearby.Update(s,14.1,c,42);
        // The empty state now contains text too; a real notice replaces it rather than necessarily adding vertices.
        const int entered=noticeVertices(14.1); Require(entered!=empty,"existing player entering detection range generated no notification");
        s.frameIndex++; nearby.Update(s,14.2,c,42); Require(noticeVertices(14.2)==entered,"stationary nearby player generated repeated notifications");
        ImGui::DestroyContext(); std::cout<<"PASS Rockstar feature state, projection, persistence boundaries and HUD\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}

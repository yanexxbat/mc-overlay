#include "overlay/rockstar_features.hpp"
#include <iostream>
#include <stdexcept>
#include <limits>
using namespace mcov;
using namespace mcov::rockstar;
void Require(bool v,const char* why) { if(!v) throw std::runtime_error(why); }
int main() {
    try {
        Config c; for(auto& o:c.modules) Require(!o.enabled,"new modules must default off");
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
        ImGui::CreateContext(); auto& io=ImGui::GetIO(); io.DisplaySize={640,480}; io.DeltaTime=1.f/60;
        unsigned char* pixels=nullptr; int width=0,height=0; io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
        for(auto& o:c.modules) o.enabled=true; c.Clamp();
        s.posX=0; CopyField(s.dimension,"Overworld"); s.frameIndex++; e.health=20; pack.Update(s,11,c,42);
        ImGui::NewFrame(); auto* draw=ImGui::GetBackgroundDrawList(); const CameraProjection projection(s,640,480);
        pack.DrawWorld(*draw,s,projection,c,{},42,{640,480},{.66f,.42f,1,1},{.05f,1,.88f,1},{},0,true);
        pack.DrawScreen(*draw,s,projection,c,{640,480},{.66f,.42f,1,1},{.05f,1,.88f,1},11);
        for(int i=int(HudModule::Radar);i<kHudModules;++i) pack.DrawHud(*draw,static_cast<HudModule>(i),s,c,{}, {232,200},11);
        ImGui::Render(); Require(ImGui::GetDrawData()->TotalVtxCount>0,"feature pack rendered nothing");
        for(auto* list:ImGui::GetDrawData()->CmdLists) for(const auto& vertex:list->VtxBuffer) Require(std::isfinite(vertex.pos.x)&&std::isfinite(vertex.pos.y),"nonfinite feature geometry");
        ImGui::DestroyContext(); std::cout<<"PASS Rockstar feature state, projection, persistence boundaries and HUD\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}

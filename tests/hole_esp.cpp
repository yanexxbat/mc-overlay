#include "overlay/client_config.hpp"
#include "imgui_internal.h"
#include <iostream>
#include <stdexcept>

using namespace mcov;
namespace {
void Require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
void Detection() {
    using namespace holes;
    auto wall=kBedrock;
    auto read=[&](int,int y,int)->std::int8_t { return y>0?kAir:wall; };
    Require(Detect(read)==Type::Bedrock,"Bedrock hole not recognised");
    wall=kObsidian; Require(Detect(read)==Type::Obsidian,"Obsidian hole not recognised");
    Require(Detect([&](int x,int y,int z) {return x==1?kBedrock:read(x,y,z);})==Type::Mixed,"Mixed hole not recognised");
    for(int missing=0;missing<7;++missing) {
        int at=0;
        Require(Detect([&](int x,int y,int z) {return at++==missing?std::int8_t(-1):read(x,y,z);})==Type::None,"Missing floor, wall or headroom accepted");
    }
    Require(Detect([&](int x,int y,int z) {return x==-1?kAir:read(x,y,z);})==Type::None,"Open or double hole accepted as 1x1");
    Require(Material("block.minecraft.water")==-1&&Material("block.minecraft.obsidian_slab")==-1&&
        Material("block.minecraft.cave_air")==kAir,"Unsafe material accepted");
}
void Profiles() {
    using namespace client_configs;
    Snapshot source; source.holes.enabled=true; source.holes.radius=16; source.holes.mixed=false; source.holes.labels=true;
    source.holes.bedrockColour.w=.6f; source.binds[keys::Index(keys::Action::HoleEsp)]={0x48,keys::Ctrl,keys::Mode::Hold};
    std::string text; Snapshot result; Require(Encode(source,text)&&Decode(text,result)&&result.holes.enabled&&result.holes.radius==16&&
        !result.holes.mixed&&result.holes.labels&&result.holes.bedrockColour.w==.6f&&
        result.binds[keys::Index(keys::Action::HoleEsp)].mode==keys::Mode::Hold,"Hole settings did not round-trip");
    std::istringstream in(text); std::ostringstream old; std::string line;
    const auto bind="bind."+std::to_string(keys::Index(keys::Action::HoleEsp))+".";
    while(std::getline(in,line)) if(!line.starts_with("holes.")&&!line.starts_with(bind)) old<<line<<'\n';
    Require(Decode(old.str(),result)&&!result.holes.enabled&&result.holes.radius==8&&!result.binds[keys::Index(keys::Action::HoleEsp)].key,
        "Old configs did not get disabled Hole ESP defaults");
    Require(!Decode(old.str()+"holes.enabled 1\n",result),"Partial hole settings accepted");
    source.holes.radius=17; Require(!Encode(source,text),"Oversized short-range scan accepted");
}
void Rendering() {
    WorldSnapshot s{}; s.camY=66; s.camZ=-4; s.posY=64; s.fovDeg=70; s.pitchDeg=15; s.nearPlane=.05f; s.farPlane=128;
    s.holeCount=3; for(unsigned i=0;i<3;++i) {s.holes[i]={}; s.holes[i].x=float(i*2)-2; s.holes[i].y=64; s.holes[i].z=4; s.holes[i].kind=std::uint8_t(i+1);}
    holes::Settings c; c.enabled=true;
    const CameraProjection projection(s,640,480); auto& draw=*ImGui::GetBackgroundDrawList();
    holes::Draw(draw,s,projection,c,{640,480}); Require(draw.VtxBuffer.Size>0,"Hole markers are invisible");
    for(const auto& v:draw.VtxBuffer) Require(std::isfinite(v.pos.x)&&std::isfinite(v.pos.y),"Hole marker emitted invalid geometry");
    const int before=draw.VtxBuffer.Size; c.enabled=false; holes::Draw(draw,s,projection,c,{640,480});
    Require(draw.VtxBuffer.Size==before,"Disabled hole markers still rendered");
    c.enabled=true; c.bedrock=c.obsidian=c.mixed=false; holes::Draw(draw,s,projection,c,{640,480});
    Require(draw.VtxBuffer.Size==before,"Hole type filters were ignored");
    c.bedrock=c.obsidian=c.mixed=true; c.distance=1; holes::Draw(draw,s,projection,c,{640,480});
    Require(draw.VtxBuffer.Size==before,"Hole distance limit was ignored");
}
}
int main() {
    try {
        Detection(); Profiles(); ImGui::CreateContext(); auto& io=ImGui::GetIO(); io.IniFilename=nullptr; io.DisplaySize={640,480};
        io.Fonts->AddFontDefault(); io.Fonts->Build(); ImGui::NewFrame(); Rendering(); ImGui::Render(); ImGui::DestroyContext();
        std::cout<<"PASS Hole ESP: exact materials, all safety neighbours, types, profiles/migration, bounded 3D rendering, filters\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}

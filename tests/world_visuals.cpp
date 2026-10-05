#include "overlay/world_visuals.hpp"
#include "imgui_internal.h"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace mcov;
namespace {
bool countAllocations=false;
int allocations=0;
void* Allocate(std::size_t size,void*) { if(countAllocations) ++allocations; return std::malloc(size); }
void Free(void* memory,void*) { std::free(memory); }
void Require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
WorldSnapshot Scene() {
    WorldSnapshot s{}; s.frameIndex=1; s.health=s.maxHealth=20; s.camY=1.2; s.fovDeg=70;
    s.nearPlane=.05f; s.farPlane=1024; s.entityCount=1;
    CopyField(s.dimension,"overworld"); auto& e=s.entities[0];
    e.entityId=42; e.uuid={11,22}; e.z=4; e.width=.6f; e.height=1.8f;
    e.flags=kEntityIsPlayer|kEntityIsLiving; e.health=e.maxHealth=20;
    return s;
}
void Checks() {
    WorldVisualsRenderer renderer; auto s=Scene(); WorldVisualsConfig config;
    config.trails.enabled=config.hits.enabled=config.ambient.enabled=true;
    config.trails.subjects=TrailSubjects::Target;
    renderer.Update(s,0,config,42);
    Require(renderer.Stats().hitParticles==0,"Initial HP spawned damage particles");
    Require(renderer.Stats().ambientParticles==48,"Ambient population is wrong");
    ++s.frameIndex; s.entities[0].x=.15; renderer.Update(s,.04,config,42);
    Require(renderer.Stats().trails==1,"Moving target did not produce a trail");
    ++s.frameIndex; s.entities[0].health=18; renderer.Update(s,.08,config,42);
    Require(renderer.Stats().hitParticles==12,"HP loss did not produce one burst");
    renderer.Update(s,.09,config,42);
    Require(renderer.Stats().hitParticles==12,"Repeated render of one snapshot duplicated damage");
    ++s.frameIndex; s.entityCount=0; renderer.Update(s,.12,config,42);
    ++s.frameIndex; s.entityCount=1; s.entities[0].health=4; renderer.Update(s,.16,config,42);
    Require(renderer.Stats().hitParticles==12,"Reappearing entity caused fake damage");
    ++s.frameIndex; s.entities[0].x=12; s.entities[0].health=2; renderer.Update(s,.2,config,42);
    Require(renderer.Stats().trails==0&&renderer.Stats().hitParticles==12,"Teleport connected a trail or spawned fake damage");
    ++s.frameIndex; s.entities[0].uuid={33,44}; s.entities[0].health=1; renderer.Update(s,.24,config,42);
    Require(renderer.Stats().hitParticles==12,"Reused entity ID inherited another UUID's HP");
    renderer.Update(s,.61,config,42);
    Require(renderer.Stats().hitParticles==0&&renderer.Stats().ambientParticles==0,"Stale snapshot kept world effects alive");
    renderer.Update(s,.7,config,42);
    Require(renderer.Stats().ambientParticles==0,"Stale snapshot restarted the ambient field");
    ++s.frameIndex; renderer.Update(s,.74,config,42);
    Require(renderer.Stats().ambientParticles==48&&renderer.Stats().hitParticles==0,"Fresh frame failed to establish a clean baseline");
    ++s.frameIndex; s.entities[0].health=.5f; renderer.Update(s,.78,config,42);
    Require(renderer.Stats().hitParticles==12,"Damage after reconnect was not recognized");
    ++s.frameIndex; CopyField(s.dimension,"nether"); renderer.Update(s,.82,config,42);
    Require(renderer.Stats().hitParticles==0,"World transition retained old particles");
    config.hits.targetOnly=true; ++s.frameIndex; s.entities[0].health=.25f; renderer.Update(s,.86,config,7);
    Require(renderer.Stats().hitParticles==0,"Target-only mode emitted particles for another entity");
    config.hits.targetOnly=false; config.hits.amount=10000;
    for(int i=0;i<30;++i) { ++s.frameIndex; s.entities[0].health=20-i*.02f; renderer.Update(s,.9+i*.01,config,42); }
    Require(renderer.Stats().hitParticles==WorldVisualsRenderer::kHitParticles,"Particle pool did not enforce its capacity");
    s.posX=std::numeric_limits<float>::quiet_NaN(); ++s.frameIndex; renderer.Update(s,1.3,config,42);
    Require(renderer.Stats().hitParticles==0&&renderer.Stats().ambientParticles==0,"Invalid positions retained effects");
    // Langsame Bewegungen akkumulieren Distanz zum letzten Sample, nicht pro Frame.
    renderer.Reset(); s=Scene(); config={}; config.trails.enabled=true; config.trails.subjects=TrailSubjects::Target;
    for(int i=0;i<100;++i) { ++s.frameIndex; s.entities[0].x=i*.005; renderer.Update(s,2+i*.01,config,42); }
    Require(renderer.Stats().trails==1,"Slow movement never accumulated trail samples");
    // Der vorbereitete und aufgewärmte DrawList-Pfad bleibt allokationsfrei.
    config.hits.enabled=config.ambient.enabled=true;
    ++s.frameIndex; s.entities[0].health=18; renderer.Update(s,3,config,42);
    auto& draw=*ImGui::GetBackgroundDrawList(); WorldVisualsRenderer::Prepare(draw);
    const auto projection=CameraProjection(s,1280,720);
    const auto texture=static_cast<ImTextureID>(123);
    for(auto shape:{TrailShape::Ribbon,TrailShape::Line,TrailShape::Dots}) {
        config.trails.shape=shape; config.hits.shape=ParticleShape::Stars; config.ambient.shape=AmbientShape::Stars;
        renderer.Draw(draw,projection,{1280,720},config,{.6f,.4f,1,1},{0,1,.8f,1},{},texture);
    }
    const int clips=draw._ClipRectStack.Size;
    allocations=0; countAllocations=true;
    renderer.Update(s,3.01,config,42);
    renderer.Draw(draw,projection,{1280,720},config,{.6f,.4f,1,1},{0,1,.8f,1},{},texture,{},true);
    countAllocations=false;
    Require(allocations==0,"Warmed renderer allocated ImGui memory");
    Require(draw._ClipRectStack.Size==clips,"World effects leaked a clipping rectangle");
    Require(draw.VtxBuffer.Size>0,"World effects generated no geometry");
    for(const auto& vertex:draw.VtxBuffer) Require(std::isfinite(vertex.pos.x)&&std::isfinite(vertex.pos.y),"Invalid effect vertex");
}
void ExtraChecks() {
    WorldVisualsRenderer renderer; auto s=Scene(); WorldVisualsConfig config;
    config.bubbles.enabled=config.kills.enabled=config.crumbs.enabled=true;
    config.kills.targetOnly=true;
    renderer.Update(s,0,config,42);
    Require(renderer.Stats().bubbles==0&&renderer.Stats().kills==0,"First observation emitted rings");
    ++s.frameIndex; s.entities[0].health=10; s.posZ=.3f; renderer.Update(s,.04,config,42);
    Require(renderer.Stats().bubbles==1&&renderer.Stats().crumbs==2,"Damage bubble or moving breadcrumb missing");
    // Das Combat-Ziel kann im Todesframe bereits freigegeben sein.
    ++s.frameIndex; s.entities[0].health=0; renderer.Update(s,.08,config,0);
    Require(renderer.Stats().kills==1&&renderer.Stats().killParticles==24,"Death of previous target was missed");
    renderer.Update(s,.09,config,0);
    ++s.frameIndex; renderer.Update(s,.12,config,0);
    Require(renderer.Stats().kills==1&&renderer.Stats().killParticles==24,"Dead entity emitted duplicate kill effects");
    auto& draw=*ImGui::GetBackgroundDrawList(); WorldVisualsRenderer::Prepare(draw);
    renderer.Draw(draw,CameraProjection(s,1280,720),{1280,720},config,{.6f,.4f,1,1},{0,1,.8f,1},{},123);
    const auto clips=draw._ClipRectStack.Size; const auto flags=draw.Flags;
    allocations=0; countAllocations=true;
    renderer.Update(s,.16,config,0);
    renderer.Draw(draw,CameraProjection(s,1280,720),{1280,720},config,{.6f,.4f,1,1},{0,1,.8f,1},{},123,{},true);
    countAllocations=false;
    Require(allocations==0&&draw._ClipRectStack.Size==clips&&draw.Flags==flags,"New effects allocated or leaked draw state");
    ++s.frameIndex; s.entities[0].health=20; renderer.Update(s,.2,config,7);
    ++s.frameIndex; s.entities[0].health=0; renderer.Update(s,.24,config,7);
    Require(renderer.Stats().kills==1,"Target-only kills triggered for a different entity");
    // Poolgrenzen und Despawn: Das reine Verschwinden erzeugt keinen Todes-Burst.
    renderer.Reset(); s=Scene(); config.kills.targetOnly=false; config.kills.amount=10000;
    renderer.Update(s,1,config);
    for(int i=0;i<100;++i) {
        ++s.frameIndex; s.entities[0].health=(i%2)?0.f:20.f; s.posX=i*.2f;
        renderer.Update(s,1.01+i*.002,config);
    }
    Require(renderer.Stats().killParticles==WorldVisualsRenderer::kKillParticles,"Death particle pool exceeded its capacity");
    Require(renderer.Stats().bubbles+renderer.Stats().kills==WorldVisualsRenderer::kRings,"Ring pool exceeded its capacity");
    Require(renderer.Stats().crumbs<=WorldVisualsRenderer::kCrumbs,"Breadcrumb pool exceeded its capacity");
    renderer.Reset(); s=Scene(); renderer.Update(s,2,config);
    ++s.frameIndex; s.entityCount=0; renderer.Update(s,2.04,config);
    Require(renderer.Stats().kills==0,"Despawn was treated as a kill");
    ++s.frameIndex; s.entityCount=1; s.entities[0].health=0; renderer.Update(s,2.08,config);
    Require(renderer.Stats().kills==0,"Reappearance created a fake kill");
    ++s.frameIndex; s.entities[0].health=20; renderer.Update(s,2.12,config);
    ++s.frameIndex; s.entities[0].x=20; s.entities[0].health=0; renderer.Update(s,2.16,config);
    Require(renderer.Stats().kills==0,"Teleport created a fake death");
    renderer.Update(s,2.6,config);
    Require(renderer.Stats().kills==0&&renderer.Stats().crumbs==0,"Stale snapshot retained rings or breadcrumbs");
    // Der neue Self-Record liefert präzise, interpolierte Fußpositionen; kein Selbsttreffer.
    renderer.Reset(); s=Scene(); s.entities[0].flags|=kEntityIsSelf;
    renderer.Update(s,3,config);
    ++s.frameIndex; s.entities[0].x=.3; s.entities[0].health=0; renderer.Update(s,3.04,config);
    Require(renderer.Stats().bubbles==0&&renderer.Stats().kills==0,"Own body emitted target damage effects");
}
}
int main() {
    try {
        ImGui::SetAllocatorFunctions(Allocate,Free); ImGui::CreateContext();
        auto& io=ImGui::GetIO(); io.IniFilename=nullptr; io.DisplaySize={1280,720}; io.DeltaTime=1.f/60;
        io.Fonts->AddFontDefault(); unsigned char* pixels; int width,height;
        io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height); ImGui::NewFrame();
        Checks(); ExtraChecks(); ImGui::Render(); ImGui::DestroyContext();
        std::cout<<"World visual checks passed\n"; return 0;
    } catch(const std::exception& e) { countAllocations=false; std::cerr<<e.what()<<'\n'; return 1; }
}

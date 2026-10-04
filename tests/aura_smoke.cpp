// Prüft reale GPU-Pixel, Projektionsskalierung und Speicherverhalten ohne Spiel/JNI.
#include "overlay/aura_projection.hpp"
#include "overlay/aura_blend_dx11.hpp"
#include "overlay/friend_hat_renderer.hpp"
#include "imgui_impl_dx11.h"
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <crtdbg.h>

using namespace mcov;
using Microsoft::WRL::ComPtr;
namespace {
bool recording=false;
int allocations=0;
void* Alloc(std::size_t size,void*) { if (recording) ++allocations; return std::malloc(size); }
void Free(void* memory,void*) { std::free(memory); }
void Require(bool good,const char* message) { if (!good) throw std::runtime_error(message); }
bool Close(float a,float b) { return std::abs(a-b)<.002f; }
void Hr(HRESULT result) { Require(SUCCEEDED(result),"D3D11 WARP operation failed"); }
WorldSnapshot Camera() {
    WorldSnapshot snapshot{};
    snapshot.camY=.9; snapshot.fovDeg=90; snapshot.nearPlane=.05f; snapshot.farPlane=1024;
    return snapshot;
}
void ProjectionChecks() {
    auto s=Camera();
    EntitySnapshot entity{};
    entity.entityId=42; entity.width=.6f; entity.height=1.8f; entity.z=10;
    AuraAnchor anchor;
    Require(MakeAuraAnchor(entity,anchor),"Valid world anchor rejected");
    AuraScreen normal,farther,large,edge;
    Require(ProjectAura(CameraProjection(s,1280,720),anchor,1280,720,normal),"Visible aura rejected");
    Require(Close(normal.center.x,640)&&Close(normal.center.y,360)&&Close(normal.radius,32.4f),
            "Aura is not centered/scaled on the real entity");
    anchor.z=20;
    Require(ProjectAura(CameraProjection(s,1280,720),anchor,1280,720,farther)&&Close(farther.radius,normal.radius*.5f),
            "Aura does not shrink with distance");
    anchor.z=10;
    Require(ProjectAura(CameraProjection(s,2560,1440),anchor,2560,1440,large)&&
            Close(large.radius,normal.radius*2)&&Close(large.center.x,normal.center.x*2),"Resolution scaling failed");
    anchor.x=17;
    Require(ProjectAura(CameraProjection(s,1280,720),anchor,1280,720,edge)&&
            Close(edge.radius,normal.radius)&&!Close(edge.center.x,normal.center.x),"Clipping changed the anchor or radius");
    s.camX=29999990.125; s.camZ=-29999990.75;
    anchor.x=s.camX; anchor.z=s.camZ+10;
    Require(ProjectAura(CameraProjection(s,1280,720),anchor,1280,720,farther)&&Close(farther.center.x,normal.center.x)&&
            Close(farther.radius,normal.radius),"World-border precision lost");
    anchor.z=s.camZ-10;
    Require(!ProjectAura(CameraProjection(s,1280,720),anchor,1280,720,farther),"Aura behind the camera was accepted");
    anchor.z=s.camZ+.01;
    Require(!ProjectAura(CameraProjection(s,1280,720),anchor,1280,720,farther),"Aura behind near plane was accepted");
    entity.width=std::numeric_limits<float>::quiet_NaN();
    Require(!MakeAuraAnchor(entity,anchor),"Invalid bounds were accepted");
}
void FadeChecks() {
    AuraRenderer renderer;
    AuraConfig config; config.enabled=true; config.animate=false; config.fadeSeconds=.2f;
    AuraAnchor a{1,0,1,5,.9f},b{2,2,1,6,.9f};
    renderer.UpdateTarget(&a,.1f,config);
    const float before=renderer.Targets()[1].alpha;
    renderer.UpdateTarget(&b,0,config);
    Require(Close(renderer.Targets()[0].alpha,before)&&renderer.Targets()[1].alpha==0,"Target change popped opacity");
    renderer.UpdateTarget(&b,.02f,config);
    const float returning=renderer.Targets()[0].alpha;
    renderer.UpdateTarget(&a,0,config);
    Require(Close(renderer.Targets()[1].alpha,returning),"Quick target reversal restarted the fade");
    a.x=3;
    renderer.RefreshAnchor(a);
    Require(renderer.Targets()[1].anchor.x==3,"Target stopped following world movement");
    renderer.UpdateTarget(nullptr,.2f,config);
    Require(!renderer.Targets()[0].valid&&!renderer.Targets()[1].valid,"Target loss never finished fading");
    config.fadeSeconds=0;
    renderer.UpdateTarget(&a,0,config);
    Require(renderer.Targets()[1].alpha==1,"Zero-duration fade was not immediate");
    config.enabled=false;
    renderer.UpdateTarget(&a,0,config);
    Require(!renderer.Targets()[0].valid&&!renderer.Targets()[1].valid,"Disabled aura did not clear");
}
void OrbitChecks() {
    AuraRenderer renderer;
    AuraConfig config; config.enabled=true; config.pulseAmplitude=0;
    AuraAnchor anchor{42,0,.9,5,.9f};
    AuraWorldFog initial,moved;
    renderer.BuildWorldFog(anchor,config,initial);
    Require(initial.orbCount==4,"Default fog did not produce four orbs");
    const auto original=anchor;
    anchor.x+=17.25; anchor.y+=3.5; anchor.z-=2;
    renderer.BuildWorldFog(anchor,config,moved);
    for (int i=0;i<initial.orbCount;++i) for (int j=0;j<AuraRenderer::kTrailSamples;++j) {
        const auto& a=initial.trails[i][j]; const auto& b=moved.trails[i][j];
        Require(std::abs((b.x-a.x)-17.25)<.00001&&std::abs((b.y-a.y)-3.5)<.00001&&
                std::abs((b.z-a.z)+2)<.00001,"Fog trail detached from moving entity");
        Require(std::abs(std::sqrt((a.x-original.x)*(a.x-original.x)+(a.y-original.y)*(a.y-original.y)+
                (a.z-original.z)*(a.z-original.z))-original.worldRadius*config.orbitRadius)<.00001,
                "Orb path was not a world-space orbit");
    }
    Require(std::abs(initial.trails[0][0].x-initial.trails[0].back().x)>.1,"Trail collapsed into its head");
    AuraRenderer fast,slow;
    for (int i=0;i<60;++i) fast.UpdateTarget(&anchor,1.f/60,config);
    for (int i=0;i<120;++i) slow.UpdateTarget(&anchor,1.f/120,config);
    fast.BuildWorldFog(anchor,config,initial); slow.BuildWorldFog(anchor,config,moved);
    Require(std::abs(initial.trails[0][0].x-moved.trails[0][0].x)<.00001,"Orbit depended on frame rate");
    auto camera=Camera(); camera.yawDeg=35;
    AuraScreenFog screen;
    fast.BuildWorldFog(original,config,initial);
    Require(ProjectAuraFog(CameraProjection(camera,128,128),initial,128,128,screen),"3D orbit projection failed");
    ScreenPoint expected; float radius;
    const auto& head=initial.trails[0][0];
    Require(CameraProjection(camera,128,128).Billboard({head.x,head.y,head.z},head.radius,expected,radius)&&
            Close(expected.x,screen.trails[0][0].center.x)&&Close(radius,screen.trails[0][0].radius),
            "Fog did not follow the current camera projection");
}
void RotationChecks() {
    AuraRenderer renderer;
    AuraConfig config; config.enabled=true; config.pulseAmplitude=0;
    config.rotationDegreesPerSecond=90;
    const AuraAnchor anchor{42,0,.9,5,.9f};
    AuraWorldFog start,rotated,paused,resumed;
    renderer.BuildWorldFog(anchor,config,start);
    for (int i=0;i<60;++i) renderer.UpdateTarget(&anchor,1.f/60,config);
    renderer.BuildWorldFog(anchor,config,rotated);
    const double orbit=anchor.worldRadius*config.orbitRadius;
    const auto& head=rotated.trails[0][0];
    Require(std::abs(head.x-anchor.x)<.00001 && head.y>anchor.y+.5*orbit && head.z>anchor.z,
            "Positive rotation did not complete a quarter orbit in one second");
    config.animate=false;
    for (int i=0;i<60;++i) renderer.UpdateTarget(&anchor,1.f/60,config);
    renderer.BuildWorldFog(anchor,config,paused);
    Require(head.x==paused.trails[0][0].x && head.y==paused.trails[0][0].y && head.z==paused.trails[0][0].z,
            "Disabling aura animation did not freeze the orbit");
    config.animate=true; config.rotationDegreesPerSecond=-90;
    for (int i=0;i<60;++i) renderer.UpdateTarget(&anchor,1.f/60,config);
    renderer.BuildWorldFog(anchor,config,resumed);
    const auto& origin=start.trails[0][0]; const auto& back=resumed.trails[0][0];
    Require(std::abs(back.x-origin.x)<.00001 && std::abs(back.y-origin.y)<.00001 &&
            std::abs(back.z-origin.z)<.00001,"Reverse rotation did not resume from the paused phase");
    config.rotationDegreesPerSecond=0;
    for (int i=0;i<60;++i) renderer.UpdateTarget(&anchor,1.f/60,config);
    renderer.BuildWorldFog(anchor,config,paused);
    Require(back.x==paused.trails[0][0].x && back.y==paused.trails[0][0].y && back.z==paused.trails[0][0].z,
            "Zero rotation speed moved the orbit");
}
struct Pixels {
    std::array<unsigned char,128*128*4> data{};
    const unsigned char* At(int x,int y) const { return data.data()+(y*128+x)*4; }
};
Pixels Capture(ID3D11Device* device,ID3D11DeviceContext* context) {
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width=desc.Height=128; desc.MipLevels=desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count=1; desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> texture; Hr(device->CreateTexture2D(&desc,nullptr,&texture));
    ComPtr<ID3D11RenderTargetView> target; Hr(device->CreateRenderTargetView(texture.Get(),nullptr,&target));
    auto* renderTarget=target.Get(); context->OMSetRenderTargets(1,&renderTarget,nullptr);
    const float clear[4]={}; context->ClearRenderTargetView(target.Get(),clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    desc.BindFlags=0; desc.Usage=D3D11_USAGE_STAGING; desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging; Hr(device->CreateTexture2D(&desc,nullptr,&staging));
    context->CopyResource(staging.Get(),texture.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{}; Hr(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped));
    Pixels pixels;
    for (int y=0;y<128;++y) std::memcpy(pixels.data.data()+y*128*4,
        static_cast<const unsigned char*>(mapped.pData)+y*mapped.RowPitch,128*4);
    context->Unmap(staging.Get(),0);
    return pixels;
}
ImDrawList& Frame() {
    auto& io=ImGui::GetIO(); io.DisplaySize={128,128}; io.DeltaTime=1.f/60;
    ImGui_ImplDX11_NewFrame(); ImGui::NewFrame();
    return *ImGui::GetBackgroundDrawList();
}
void DrawingChecks(ID3D11Device* device,ID3D11DeviceContext* context,AuraBlendDX11& blend) {
    AuraRenderer renderer;
    AuraConfig config; config.enabled=true; config.animate=false; config.orbCount=AuraRenderer::kMaxOrbs;
    AuraWorldFog world;
    AuraScreenFog first,second;
    AuraAnchor anchor{42,0,.9,5,.9f};
    const CameraProjection projection(Camera(),128,128);
    auto& initial=Frame(); AuraRenderer::Prepare(initial); ImGui::Render();
    for (int frame=0;frame<40;++frame) {
        auto& draw=Frame(); const auto flags=draw.Flags;
        recording=true;
        renderer.BuildWorldFog(anchor,config,world);
        ProjectAuraFog(projection,world,128,128,first);
        ProjectAuraFog(projection,world,128,128,second,{10,0});
        renderer.Draw(draw,first,1,config,blend.Callbacks(),blend.GlowTexture());
        renderer.Draw(draw,second,1,config,blend.Callbacks(),blend.GlowTexture());
        recording=false;
        Require(draw.Flags==flags,"Aura leaked draw flags");
        Require(draw.VtxBuffer.Size<=AuraRenderer::kMaxVertices&&draw.IdxBuffer.Size<=AuraRenderer::kMaxIndices,
                "Aura exceeded the reserved worst-case budget");
        for (const auto& vertex:draw.VtxBuffer)
            Require(std::isfinite(vertex.pos.x)&&std::isfinite(vertex.pos.y),"Aura generated non-finite vertices");
        ImGui::Render();
    }
    Require(allocations==0,"Aura allocated during warmed-up rendering");
    auto& draw=Frame();
    draw.AddRectFilled({0,0},{64,64},IM_COL32(51,51,51,255));
    draw.AddCallback(blend.Callbacks().begin,blend.Callbacks().data);
    draw.AddRectFilled({0,0},{16,16},IM_COL32(128,0,0,128));
    draw.AddCallback(ImDrawCallback_ResetRenderState,nullptr);
    draw.AddRectFilled({24,0},{40,16},IM_COL32(128,0,0,128));
    ImGui::Render();
    const auto blendPixels=Capture(device,context);
    Require(blendPixels.At(8,8)[0]>110&&blendPixels.At(8,8)[1]==51,"RGB did not use additive blending");
    Require(blendPixels.At(32,8)[0]<95&&blendPixels.At(32,8)[1]<30,"Normal alpha blending was not restored");

    auto& glow=Frame();
    // Auch nach dem Überschreiten der 16-Bit-Indexgrenze muss der Verlauf stimmen.
    for (int i=0;i<17000;++i) glow.AddRectFilled({-20,-20},{-10,-10},IM_COL32_WHITE);
    glow.PushClipRect({32,32},{96,96},true);
    AuraScreenFog probe; probe.orbCount=1; probe.trails[0][0]={{32,64},12,true};
    renderer.Draw(glow,probe,1,config,blend.Callbacks(),blend.GlowTexture());
    glow.PopClipRect(); ImGui::Render();
    const auto glowPixels=Capture(device,context);
    Require(glowPixels.At(34,64)[3]>0&&glowPixels.At(34,64)[2]>0,"Glow disappeared on a transparent target");
    Require(glowPixels.At(28,64)[3]==0&&glowPixels.At(0,0)[3]==0,"Aura escaped clipping or darkened empty pixels");
    auto& invalid=Frame(); const int count=invalid.VtxBuffer.Size;
    probe.trails[0][0].radius=std::numeric_limits<float>::quiet_NaN();
    renderer.Draw(invalid,probe,1,config,blend.Callbacks(),blend.GlowTexture());
    Require(invalid.VtxBuffer.Size==count,"Invalid projection produced geometry");
    ImGui::Render();
}
void FriendHatChecks(ID3D11Device* device,ID3D11DeviceContext* context,AuraBlendDX11& blend) {
    FriendHatRenderer renderer;
    FriendHatConfig config;
    AuraConfig colors;
    WorldSnapshot scene=Camera(); scene.entityCount=1;
    auto& entity=scene.entities[0];
    entity.entityId=42; entity.uuid={1,2}; entity.z=5; entity.distance=5;
    entity.width=.6f; entity.height=entity.modelHeight=1.8f; entity.health=20;
    entity.flags=kEntityIsPlayer|kEntityIsLiving; entity.rigKind=kRigHumanoid;
    combat::Relations relations{}; relations[0]={entity.uuid,combat::Relation::Friend,true};
    Require(FriendHatRenderer::IsFriend(entity,relations),"UUID friend was not marked");
    auto stranger=entity; stranger.uuid={7,8};
    Require(!FriendHatRenderer::IsFriend(stranger,relations),"reused entity ID inherited a friend hat");
    relations[0].relation=combat::Relation::Enemy;
    Require(!FriendHatRenderer::IsFriend(entity,relations),"enemy received a friend hat");
    relations[0].relation=combat::Relation::Friend;
    FriendHatWorld initial,moved,crouched; FriendHatScreen normal,farther,large;
    Require(renderer.BuildWorld(entity,config,initial),"valid friend hat rejected");
    const auto camera=Camera();
    Require(FriendHatRenderer::Project(CameraProjection(camera,1280,720),initial,{0,.9,0},{1280,720},normal),
            "friend hat projection failed");
    entity.z=10;
    Require(renderer.BuildWorld(entity,config,moved) &&
            FriendHatRenderer::Project(CameraProjection(camera,1280,720),moved,{0,.9,0},{1280,720},farther) &&
            Close(farther.tipGlowRadius,normal.tipGlowRadius*.5f),"friend hat did not scale with distance");
    Require(FriendHatRenderer::Project(CameraProjection(camera,2560,1440),initial,{0,.9,0},{2560,1440},large) &&
            Close(large.tip.x,normal.tip.x*2) && Close(large.tipGlowRadius,normal.tipGlowRadius*2),
            "friend hat did not scale with resolution");
    entity.z=5; entity.x=123456.25; entity.y=3;
    Require(renderer.BuildWorld(entity,config,moved) && std::abs(moved.tip.x-initial.tip.x-123456.25)<1e-7 &&
            std::abs(moved.tip.y-initial.tip.y-3)<1e-7,"friend hat detached from moving head");
    entity.x=entity.y=0; entity.pose=kPoseCrouching;
    Require(renderer.BuildWorld(entity,config,crouched) && crouched.tip.y<initial.tip.y,"friend hat ignored crouching");
    entity.pose=kPoseStanding; entity.z=-5;
    Require(renderer.BuildWorld(entity,config,moved) &&
            !FriendHatRenderer::Project(CameraProjection(camera,1280,720),moved,{0,.9,0},{1280,720},farther),
            "friend hat behind camera was accepted");
    entity.z=5;
    auto& warm=Frame(); FriendHatRenderer::Prepare(warm);
    renderer.DrawFriends(warm,scene,CameraProjection(scene,128,128),relations,64,config,colors.startColor,colors.endColor,
                         blend.Callbacks(),blend.GlowTexture(),{128,128});
    ImGui::Render();
    const int before=allocations;
    for(int i=0;i<10;++i) {
        auto& draw=Frame(); const auto flags=draw.Flags; const auto clips=draw._ClipRectStack.Size;
        recording=true;
        renderer.DrawFriends(draw,scene,CameraProjection(scene,128,128),relations,64,config,colors.startColor,colors.endColor,
                             blend.Callbacks(),blend.GlowTexture(),{128,128});
        recording=false;
        Require(draw.VtxBuffer.Size>0 && draw.Flags==flags && draw._ClipRectStack.Size==clips,
                "friend hat was empty or leaked draw state");
        for(const auto& vertex:draw.VtxBuffer)
            Require(std::isfinite(vertex.pos.x)&&std::isfinite(vertex.pos.y),"friend hat produced non-finite vertices");
        ImGui::Render();
    }
    Require(allocations==before,"friend hat allocated after warm-up");
    const auto pixels=Capture(device,context);
    bool visible=false,transparent=false;
    for(std::size_t i=3;i<pixels.data.size();i+=4) {
        visible|=pixels.data[i]>0; transparent|=pixels.data[i]>0 && pixels.data[i]<200;
    }
    Require(visible&&transparent,"friend hat did not render transparent GPU pixels");
    auto& empty=Frame(); relations[0]={};
    renderer.DrawFriends(empty,scene,CameraProjection(scene,128,128),relations,64,config,colors.startColor,colors.endColor,
                         blend.Callbacks(),blend.GlowTexture(),{128,128});
    Require(empty.VtxBuffer.Size==0,"removed friend kept its hat"); ImGui::Render();
    auto& restore=Frame(); relations[0]={entity.uuid,combat::Relation::Friend,true};
    // Eigene Dreiecke müssen nach einem 16-Bit-Vertex-Offset weiter korrekt indizieren.
    for(int i=0;i<17000;++i) restore.AddRectFilled({-20,-20},{-10,-10},IM_COL32_WHITE);
    restore.AddRectFilled({8,104},{24,120},IM_COL32(51,51,51,255));
    renderer.DrawFriends(restore,scene,CameraProjection(scene,128,128),relations,64,config,colors.startColor,colors.endColor,
                         blend.Callbacks(),blend.GlowTexture(),{128,128});
    restore.AddRectFilled({8,104},{24,120},IM_COL32(128,0,0,128));
    ImGui::Render();
    const auto restored=Capture(device,context);
    Require(restored.At(12,108)[0]<95 && restored.At(12,108)[1]<30,"friend glow leaked additive blending into later UI");
    bool offsetVisible=false;
    for(int y=0;y<90;++y) for(int x=0;x<128;++x) offsetVisible|=restored.At(x,y)[3]>0;
    Require(offsetVisible,"friend hat mesh failed after the 16-bit vertex offset");
}
} // namespace
int main() {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    _set_error_mode(_OUT_TO_STDERR); _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#ifdef _DEBUG
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE); _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
#endif
    try {
        ProjectionChecks(); FadeChecks(); OrbitChecks(); RotationChecks();
        ComPtr<ID3D11Device> device; ComPtr<ID3D11DeviceContext> context;
        Hr(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context));
        ImGui::SetAllocatorFunctions(Alloc,Free); ImGui::CreateContext(); ImGui::GetIO().IniFilename=nullptr;
        Require(ImGui_ImplDX11_Init(device.Get(),context.Get()),"ImGui D3D11 initialization failed");
        AuraBlendDX11 blend; Require(blend.Initialize(device.Get()),"Aura blend initialization failed");
        DrawingChecks(device.Get(),context.Get(),blend);
        FriendHatChecks(device.Get(),context.Get(),blend);
        ImGui_ImplDX11_Shutdown(); ImGui::DestroyContext();
        std::cout<<"PASS aura fog: world-space orbits, rotation direction/speed/pause/resume, curved trails, entity/camera movement, frame-rate independence, interrupted fades, no warmed-up allocations, additive RGB, alpha coverage, blend reset, clipping and vertex offsets\n";
        return 0;
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}

#include "overlay/esp_renderer.hpp"
#include "overlay/visual_draw.hpp"
#include "overlay/visual_geometry.hpp"
#include "imgui_internal.h"
#include "imgui_impl_dx11.h"

#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace mcov;
using Microsoft::WRL::ComPtr;
namespace {
bool recordAllocations = false;
int allocations = 0;
void* Allocate(std::size_t size, void*) {
    if (recordAllocations) ++allocations;
    return std::malloc(size);
}
void Free(void* pointer, void*) { std::free(pointer); }
void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void Hr(HRESULT value) { Require(SUCCEEDED(value),"D3D11 WARP operation failed"); }
struct Pixels {
    int width{},height{};
    std::vector<unsigned char> data;
    const unsigned char* At(int x,int y) const {
        Require(x>=0 && y>=0 && x<width && y<height,"Pixel probe outside render target");
        return data.data()+(y*width+x)*4;
    }
};
Pixels Capture(ID3D11Device* device,ID3D11DeviceContext* context) {
    const auto size=ImGui::GetIO().DisplaySize;
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width=UINT(size.x); desc.Height=UINT(size.y); desc.MipLevels=desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count=1;
    desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> texture; Hr(device->CreateTexture2D(&desc,nullptr,&texture));
    ComPtr<ID3D11RenderTargetView> target; Hr(device->CreateRenderTargetView(texture.Get(),nullptr,&target));
    auto* view=target.Get(); context->OMSetRenderTargets(1,&view,nullptr);
    const float clear[4]{}; context->ClearRenderTargetView(view,clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    desc.BindFlags=0; desc.Usage=D3D11_USAGE_STAGING; desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging; Hr(device->CreateTexture2D(&desc,nullptr,&staging));
    context->CopyResource(staging.Get(),texture.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{}; Hr(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped));
    Pixels result{int(desc.Width),int(desc.Height),std::vector<unsigned char>(desc.Width*desc.Height*4)};
    for (UINT y=0;y<desc.Height;++y)
        std::memcpy(result.data.data()+y*desc.Width*4,
                    static_cast<const unsigned char*>(mapped.pData)+y*mapped.RowPitch,desc.Width*4);
    context->Unmap(staging.Get(),0);
    return result;
}
ImDrawList& Frame(ImVec2 size={384,256}) {
    auto& io=ImGui::GetIO(); io.DisplaySize=size; io.DeltaTime=1.f/60;
    ImGui_ImplDX11_NewFrame(); ImGui::NewFrame();
    return *ImGui::GetBackgroundDrawList();
}
WorldSnapshot Scene() {
    WorldSnapshot scene{}; scene.camY=.9; scene.fovDeg=90;
    scene.nearPlane=.05f; scene.farPlane=1024; scene.entityCount=1;
    auto& e=scene.entities[0];
    e.z=2; e.width=.6f; e.height=e.modelHeight=1.8f; e.health=e.maxHealth=20;
    e.distance=2; e.entityId=42; e.uuid={1,2}; e.rigKind=kRigHumanoid;
    e.flags=kEntityIsPlayer|kEntityIsLiving; e.details.armorValue=10;
    CopyField(e.name,"Render target");
    return scene;
}
VisualSettings Empty() {
    VisualSettings c;
    c.boxEsp=c.skeleton=c.tracers=c.nameTags=c.healthBars=c.distanceText=c.targetInfo=false;
    c.targetHighlight=EspHighlight::Off; c.targetGlow=false;
    return c;
}
Pixels Render(ID3D11Device* device,ID3D11DeviceContext* context,const WorldSnapshot& scene,
              const VisualSettings& config,ImVec2 origin={}) {
    auto& draw=Frame();
    EspRenderer::Draw(draw,scene,CameraProjection(scene,384,256),config,{384,256},origin);
    ImGui::Render(); return Capture(device,context);
}
void GeometryChecks() {
    const auto scene=Scene(); const CameraProjection projection(scene,384,256);
    const auto cube=visual::BoundsCuboid(EntityBounds(scene.entities[0]));
    Require(visual::FacingCamera(cube,4,{0,.9,0}),"Front face was culled");
    Require(!visual::FacingCamera(cube,5,{0,.9,0}),"Rear face was not culled");
    int faces=0;
    for (int face=0;face<6;++face) {
        visual::ProjectedFace result;
        if (!visual::ProjectFace(projection,cube,face,384,256,result)) continue;
        ++faces;
        for (int i=0;i<result.count;++i) {
            const auto p=result.points[i];
            Require(std::isfinite(p.x)&&std::isfinite(p.y)&&p.x>=-.01f&&p.x<=384.01f&&p.y>=-.01f&&p.y<=256.01f,
                    "Face escaped the viewport");
        }
    }
    Require(faces==6,"Visible cuboid faces failed projection");
    const auto nearCube=visual::BoundsCuboid({{-.1,.6,-.1},{.1,1.2,.2}});
    bool clipped=false;
    for (int face=0;face<6;++face) {
        visual::ProjectedFace result;
        if (visual::ProjectFace(projection,nearCube,face,384,256,result)) {
            clipped=true;
            for (int i=0;i<result.count;++i)
                Require(result.points[i].x>=-.01f&&result.points[i].x<=384.01f&&
                        result.points[i].y>=-.01f&&result.points[i].y<=256.01f,"Near clipping produced giant geometry");
        }
    }
    Require(clipped,"Near-plane cuboid disappeared");
    const auto rig=BuildHumanoidRig(scene.entities[0]);
    const auto model=visual::BuildHighlight(scene.entities[0],rig);
    Require(model.count==10,"Humanoid highlight did not contain ten body parts");
    for (const auto& part:model.parts) for (const auto point:part.points)
        Require(visual::Finite(point),"Humanoid highlight produced non-finite world coordinates");
    ScreenPoint a{192,128},b{192,512}; float first=0,last=1;
    Require(visual::ClipLine(a,b,384,256,first,last)&&std::abs(b.y-256)<.01f&&last<1,
            "Offscreen tracer did not preserve its gradient parameter");
}
void HelperPixels(ID3D11Device* device,ID3D11DeviceContext* context) {
    auto& draw=Frame();
    visual::DrawGradientRect(draw,{10,10},{74,74},IM_COL32(255,0,0,255),IM_COL32(0,0,255,51),6);
    visual::DrawGradientLine(draw,{10,92},{74,92},IM_COL32(255,0,0,26),IM_COL32(0,0,255,230),4);
    visual::DrawBoxOutline(draw,{100,10},{180,82},IM_COL32_WHITE,1.6f,6,true,.28f);
    ImGui::Render(); const auto pixels=Capture(device,context);
    const auto* top=pixels.At(40,16); const auto* bottom=pixels.At(40,68);
    Require(top[0]>top[2]&&bottom[2]>bottom[0]&&top[3]>bottom[3]+100,"Rectangle RGB/alpha gradient failed");
    Require(pixels.At(15,92)[3]<pixels.At(69,92)[3],"Line alpha gradient failed");
    Require(pixels.At(110,10)[3]>0&&pixels.At(140,10)[3]==0,"Corner brackets became a full box");
    Require(pixels.At(0,0)[3]==0,"Helpers darkened untouched transparent pixels");
    auto& offset=Frame();
    for(int i=0;i<17000;++i) offset.AddRectFilled({-20,-20},{-10,-10},IM_COL32_WHITE);
    visual::DrawGradientRect(offset,{10,10},{74,74},IM_COL32(255,0,0,255),IM_COL32(0,0,255,51),6);
    ImGui::Render(); const auto after=Capture(device,context);
    Require(after.At(40,16)[0]>after.At(40,16)[2]&&after.At(40,68)[2]>after.At(40,68)[0],
            "Gradient failed after the 16-bit vertex offset");
}
void MeterPixels(ID3D11Device* device,ID3D11DeviceContext* context) {
    auto scene=Scene(); auto config=Empty(); config.healthBars=true;
    ScreenRect rect; Require(CameraProjection(scene,384,256).Bounds(EntityBounds(scene.entities[0]),rect),"Meter bounds missing");
    const int healthX=int(rect.left-7.5f),armorX=int(rect.right+7.5f);
    auto pixels=Render(device,context,scene,config);
    const auto* healthy=pixels.At(healthX,int((rect.top+rect.bottom)*.5f));
    Require(healthy[1]>healthy[0],"Full health was not green");
    Require(pixels.At(armorX,int(rect.bottom-(rect.bottom-rect.top)*.25f))[2]>100,"Armor fill missing");
    Require(pixels.At(armorX,int(rect.top+5))[2]<100,"Armor fill ignored its fraction");
    scene.entities[0].health=2;
    pixels=Render(device,context,scene,config);
    const auto* low=pixels.At(healthX,int(rect.bottom-3));
    Require(low[0]>low[1]*2,"Low health retained a healthy color");
    scene.entities[0].health=10;
    pixels=Render(device,context,scene,config);
    const auto* half=pixels.At(healthX,int(rect.bottom-4));
    Require(half[0]>150&&half[1]>100&&half[2]<100,"Half health was not yellow");
    config.barPlacement=EspBarPlacement::Horizontal;
    pixels=Render(device,context,scene,config);
    Require(pixels.At(int(rect.left+5),int(rect.bottom+6))[0]>100,"Horizontal health bar missing");
    Require(pixels.At(int(rect.left+5),int(rect.bottom+12))[2]>100,"Horizontal armor bar missing");
}
void RendererChecks(ID3D11Device* device,ID3D11DeviceContext* context) {
    auto scene=Scene(); auto config=Empty();
    auto pixels=Render(device,context,scene,config);
    Require(std::all_of(pixels.data.begin(),pixels.data.end(),[](unsigned char b){return b==0;}),"Disabled modules still rendered");
    config.boxEsp=true; config.filledBox=true;
    for(const auto style:{EspBoxStyle::Corners2D,EspBoxStyle::Full2D,EspBoxStyle::Wire3D}) {
        config.boxStyle=style; pixels=Render(device,context,scene,config);
        Require(pixels.At(192,128)[3]>0&&pixels.At(0,0)[3]==0,"Box fill or clipping failed");
    }
    config=Empty(); config.boxEsp=true; config.targetHighlight=EspHighlight::Humanoid; config.targetGlow=true;
    scene.hasTarget=1; scene.targetEntityId=42;
    pixels=Render(device,context,scene,config);
    Require(pixels.At(192,128)[3]>0,"Humanoid target highlight missing");
    scene.hasTarget=0; pixels=Render(device,context,scene,config);
    Require(pixels.At(192,128)[3]==0,"Unselected entity inherited target highlight");
    config=Empty(); config.tracers=true;
    pixels=Render(device,context,scene,config);
    Require(pixels.At(192,220)[3]>0,"Tracer missing");
    config.tracerColor[3]=0; pixels=Render(device,context,scene,config);
    Require(std::all_of(pixels.data.begin(),pixels.data.end(),[](unsigned char b){return b==0;}),"Zero tracer alpha still drew");
    auto& invalid=Frame(); const int before=invalid.VtxBuffer.Size;
    scene.entities[0].x=std::numeric_limits<double>::quiet_NaN();
    EspRenderer::Draw(invalid,scene,CameraProjection(scene,384,256),VisualSettings{},{384,256});
    Require(invalid.VtxBuffer.Size==before,"Invalid entity generated vertices");
    ImGui::Render();
}
void LoadChecks() {
    auto scene=Scene(); scene.entityCount=kMaxEntities; scene.hasTarget=1; scene.targetEntityId=42;
    for(std::uint32_t i=1;i<kMaxEntities;++i) {
        scene.entities[i]=scene.entities[0]; scene.entities[i].entityId=42+i;
        scene.entities[i].x=(int(i%16)-8)*.15; scene.entities[i].z=2+(i/16)*.1;
        std::memset(scene.entities[i].name,'W',kNameLen); // bounded, unterminated input
    }
    VisualSettings config; config.tracers=config.filledBox=true; config.targetInfo=false;
    config.nameTagEquipment=false;
    const ImVec2 viewport{1280,720};
    const CameraProjection projection(scene,viewport.x,viewport.y);
    for(int frame=0;frame<3;++frame) {
        auto& draw=Frame(viewport); EspRenderer::Draw(draw,scene,projection,config,viewport); ImGui::Render();
    }
    allocations=0; double totalMs=0; int maxVertices=0,maxCommands=0;
    for(int frame=0;frame<30;++frame) {
        auto& draw=Frame(viewport); const auto flags=draw.Flags; const auto clips=draw._ClipRectStack.Size;
        const auto textures=draw._TextureIdStack.Size;
        const auto start=std::chrono::steady_clock::now(); recordAllocations=true;
        EspRenderer::Draw(draw,scene,projection,config,viewport);
        recordAllocations=false;
        totalMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        Require(draw.Flags==flags&&draw._ClipRectStack.Size==clips&&draw._TextureIdStack.Size==textures,
                "ESP leaked draw-list state");
        for(const auto& vertex:draw.VtxBuffer)
            Require(std::isfinite(vertex.pos.x)&&std::isfinite(vertex.pos.y),"ESP emitted non-finite screen positions");
        maxVertices=std::max(maxVertices,draw.VtxBuffer.Size); maxCommands=std::max(maxCommands,draw.CmdBuffer.Size);
        ImGui::Render();
    }
    Require(allocations==0,"ESP allocated after warm-up");
    Require(maxCommands<=12,"Normal name tags fragmented draw commands");
    Require(maxVertices<1000000,"ESP exceeded the existing depth-buffer vertex capacity");
    std::cout<<"256 entities: "<<totalMs/30<<" ms CPU/frame, "<<maxVertices<<" vertices, "
             <<maxCommands<<" commands, zero warmed-up ImGui allocations\n";
}
void SaveBmp(const Pixels& pixels,const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
    BITMAPFILEHEADER file{}; file.bfType=0x4d42; file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);
    file.bfSize=file.bfOffBits+DWORD(pixels.data.size());
    BITMAPINFOHEADER info{}; info.biSize=sizeof(info); info.biWidth=pixels.width; info.biHeight=-pixels.height;
    info.biPlanes=1; info.biBitCount=32; info.biCompression=BI_RGB;
    auto bgra=pixels.data;
    for(std::size_t i=0;i<bgra.size();i+=4) std::swap(bgra[i],bgra[i+2]);
    std::ofstream output(path,std::ios::binary);
    output.write(reinterpret_cast<const char*>(&file),sizeof(file));
    output.write(reinterpret_cast<const char*>(&info),sizeof(info));
    output.write(reinterpret_cast<const char*>(bgra.data()),std::streamsize(bgra.size()));
    Require(bool(output),"Could not save the GPU preview");
}
void Preview(ID3D11Device* device,ID3D11DeviceContext* context,const std::filesystem::path& path) {
    auto& draw=Frame({1200,640});
    draw.AddRectFilled({0,0},{1200,640},IM_COL32(18,23,32,255));
    auto& font=*ImGui::GetFont();
    visual::RenderTextWithOutline(draw,font,24,{32,24},IM_COL32_WHITE,"ESP / external renderer");
    const char* titles[3]={"CORNER / gradient + armor","FULL / target silhouette","3D / clipped world bounds"};
    for(int column=0;column<3;++column) {
        const ImVec2 origin{float(column*400+18),94}; const ImVec2 viewport{364,510};
        visual::RenderTextWithOutline(draw,font,14,{origin.x+12,68},IM_COL32(177,193,212,255),titles[column]);
        draw.AddRectFilled(origin,{origin.x+viewport.x,origin.y+viewport.y},IM_COL32(24,31,42,255),6);
        for(int line=0;line<12;++line)
            draw.AddLine({origin.x,origin.y+line*46.f},{origin.x+viewport.x,origin.y+line*46.f},IM_COL32(42,50,62,100));
        auto scene=Scene(); scene.entities[0].z=3; scene.entities[0].distance=3;
        scene.entities[0].health=column==0 ? 18.f : column==1 ? 10.f : 3.f;
        scene.entities[0].walkPhase=1.2f; scene.entities[0].walkSpeed=.32f;
        scene.hasTarget=column>0; scene.targetEntityId=42;
        VisualSettings config; config.targetInfo=false; config.tracers=config.filledBox=true;
        config.nameTagEquipment=config.nameTagPing=false;
        config.boxStyle=column==0 ? EspBoxStyle::Corners2D : column==1 ? EspBoxStyle::Full2D : EspBoxStyle::Wire3D;
        config.barPlacement=column==1 ? EspBarPlacement::Horizontal : EspBarPlacement::Vertical;
        EspRenderer::Draw(draw,scene,CameraProjection(scene,viewport.x,viewport.y),config,viewport,origin);
    }
    ImGui::Render(); SaveBmp(Capture(device,context),path);
}
} // namespace

int main(int argc,char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    try {
        GeometryChecks();
        ComPtr<ID3D11Device> device; ComPtr<ID3D11DeviceContext> context;
        Hr(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,
                             &device,nullptr,&context));
        ImGui::SetAllocatorFunctions(Allocate,Free); ImGui::CreateContext();
        ImGui::GetIO().IniFilename=nullptr;
        Require(ImGui_ImplDX11_Init(device.Get(),context.Get()),"ImGui DX11 initialization failed");
        HelperPixels(device.Get(),context.Get()); MeterPixels(device.Get(),context.Get());
        RendererChecks(device.Get(),context.Get()); LoadChecks();
        if(argc>1) Preview(device.Get(),context.Get(),std::filesystem::path(argv[1]));
        ImGui_ImplDX11_Shutdown(); ImGui::DestroyContext();
        std::cout<<"PASS ESP geometry, GPU gradients/alpha/meters/highlights, clipping, vertex offsets and draw-state ownership\n";
        return 0;
    } catch(const std::exception& error) {
        recordAllocations=false; std::cerr<<error.what()<<'\n'; return 1;
    }
}

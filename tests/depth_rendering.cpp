// GPU-Pruefung mit echtem ImGui/D3D11, ohne Minecraft oder JVM-Attach.
#include "overlay/depth_renderer.hpp"
#include "imgui_internal.h"
#include "imgui_impl_dx11.h"
#include <iostream>
#include <stdexcept>

using namespace mcov;
using Microsoft::WRL::ComPtr;
void Require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
void Hr(HRESULT result) { Require(SUCCEEDED(result),"D3D11 operation failed"); }
int main() {
    try {
        ComPtr<ID3D11Device> device; ComPtr<ID3D11DeviceContext> context;
        Hr(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context));
        ImGui::CreateContext(); auto& io=ImGui::GetIO(); io.DisplaySize={320,180}; io.DeltaTime=1.f/60; io.IniFilename=nullptr;
        Require(ImGui_ImplDX11_Init(device.Get(),context.Get()),"ImGui DX11 initialization failed");
        DepthRenderer renderer; Require(renderer.Initialize(device.Get()),"Depth shader initialization failed");
        auto frame=std::make_unique<DepthFrame>();
        const auto raw=[](float distance) { constexpr float n=.05f,f=1000; return f*(distance-n)/(distance*(f-n)); };
        for(int y=0;y<kDepthHeight;++y) for(int x=0;x<kDepthWidth;++x) frame->pixels[y*kDepthWidth+x]=raw(x<kDepthWidth/2?3.f:8.f);
        D3D11_TEXTURE2D_DESC desc{}; desc.Width=320; desc.Height=180; desc.MipLevels=desc.ArraySize=1;
        desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count=1; desc.BindFlags=D3D11_BIND_RENDER_TARGET;
        ComPtr<ID3D11Texture2D> target; Hr(device->CreateTexture2D(&desc,nullptr,&target));
        ComPtr<ID3D11RenderTargetView> view; Hr(device->CreateRenderTargetView(target.Get(),nullptr,&view));
        desc.Usage=D3D11_USAGE_STAGING; desc.BindFlags=0; desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        ComPtr<ID3D11Texture2D> staging; Hr(device->CreateTexture2D(&desc,nullptr,&staging));
        for(bool missing:{false,true}) {
            ImGui_ImplDX11_NewFrame(); ImGui::NewFrame(); auto* draw=ImGui::GetBackgroundDrawList();
            renderer.Begin(*draw);
            {
                depth::Scope scope(renderer.Tags());
                // Mehr als 64K Vertices erzwingen ImGui VtxOffset; unsichtbare Dummy-Geometrie.
                for(int i=0;i<17000;++i) draw->AddRectFilled({0,0},{1,1},IM_COL32(0,0,0,1));
                Require(draw->VtxBuffer.Size>65536,"VtxOffset fixture did not generate enough vertices");
                int first=draw->VtxBuffer.Size; draw->AddRectFilled({20,20},{300,70},IM_COL32(0,255,0,255)); depth::Tag(*draw,first,7);
                first=draw->VtxBuffer.Size; draw->AddRectFilled({20,90},{300,130},IM_COL32(255,0,0,255)); depth::Tag(*draw,first,2);
                draw->AddRectFilled({20,145},{80,165},IM_COL32(0,0,255,255)); // Bildschirm/HUD bleibt ungeclippt.
            }
            ImGui::GetForegroundDrawList()->AddRectFilled({240,145},{300,165},IM_COL32(255,255,255,255));
            ImGui::Render(); auto* data=ImGui::GetDrawData();
            // Ein vorgeschalteter DrawList testet zusaetzlich den globalen Vertex-Offset.
            auto prefix=std::make_unique<ImDrawList>(ImGui::GetDrawListSharedData());
            prefix->_ResetForNewFrame();
            prefix->PushClipRect({0,0},{320,180}); prefix->PushTextureID(io.Fonts->TexID);
            prefix->AddRectFilled({0,0},{2,2},IM_COL32(255,255,255,255));
            data->CmdLists.resize(data->CmdListsCount+1);
            for(int i=data->CmdListsCount;i>0;--i) data->CmdLists[i]=data->CmdLists[i-1];
            data->CmdLists[0]=prefix.get(); ++data->CmdListsCount;
            data->TotalVtxCount+=prefix->VtxBuffer.Size; data->TotalIdxCount+=prefix->IdxBuffer.Size;
            Require(renderer.Finish(context.Get(),*data,missing?nullptr:frame.get()),"Depth upload failed");
            auto* output=view.Get(); context->OMSetRenderTargets(1,&output,nullptr); const float clear[]={0,0,0,0}; context->ClearRenderTargetView(output,clear);
            ImGui_ImplDX11_RenderDrawData(data); context->CopyResource(staging.Get(),target.Get());
            D3D11_MAPPED_SUBRESOURCE pixels{}; Hr(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&pixels));
            const auto channel=[&](int x,int y,int c){return static_cast<const unsigned char*>(pixels.pData)[y*pixels.RowPitch+x*4+c];};
            Require(channel(40,40,1)==0,"effect behind the left occluder leaked through");
            Require(missing?channel(250,40,1)==0:channel(250,40,1)>200,"visible portion incorrectly clipped / missing depth failed open");
            Require(missing?channel(40,110,0)==0:channel(40,110,0)>200,"front effect was incorrectly hidden");
            Require(channel(40,155,2)>200&&channel(270,155,0)>200,"depth shader leaked into HUD or foreground state");
            context->Unmap(staging.Get(),0);
        }
        renderer.Shutdown(); ImGui_ImplDX11_Shutdown(); ImGui::DestroyContext();
        std::cout<<"PASS depth shader: partial occlusion, visible front, missing data, local/global vertex offsets and HUD state\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}

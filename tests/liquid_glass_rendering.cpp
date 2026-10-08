// Echter DX11/WARP-Pixeltest fuer Glas, Alpha, Ebenenfolge und Resize.
#include "common/liquid_glass.hpp"
#include "backdrop.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <algorithm>
using Microsoft::WRL::ComPtr;
void Require(bool value,const char* error) { if(!value) throw std::runtime_error(error); }
int main() {
    try {
        ComPtr<ID3D11Device> device; ComPtr<ID3D11DeviceContext> context;
        Require(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,
            D3D11_SDK_VERSION,&device,nullptr,&context)),"WARP unavailable");
        ImGui::CreateContext(); ImGui::GetIO().IniFilename=nullptr;
        ImGui::GetIO().Fonts->AddFontDefault();
        Require(ImGui_ImplDX11_Init(device.Get(),context.Get()),"ImGui DX11 unavailable");
        mcov::LiquidGlass glass;
        Require(glass.Initialize(device.Get(),context.Get())&&glass.ready(),"Glass shader initialization failed");
        // Blur-Shaders laufen mit synthetischem Muster. Dieser Test nimmt keinen
        // Desktop auf und benoetigt weder ein Fenster noch eine Minecraft-JVM.
        {
            Glass::Backdrop blur;
            struct Cleanup { Glass::Backdrop& blur; ~Cleanup() { blur.Shutdown(); } } cleanup{blur};
            Require(blur.Init(device.Get(),context.Get(),nullptr),"Kawase shaders failed to compile");
            std::uint32_t checker[64*64];
            for(int y=0;y<64;++y) for(int x=0;x<64;++x)
                checker[y*64+x]=(x+y)%2?IM_COL32(255,0,0,255):IM_COL32(0,0,255,255);
            D3D11_TEXTURE2D_DESC d{}; d.Width=d.Height=64; d.MipLevels=d.ArraySize=1;
            d.Format=DXGI_FORMAT_R8G8B8A8_UNORM; d.SampleDesc.Count=1; d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
            D3D11_SUBRESOURCE_DATA source{checker,64*sizeof(std::uint32_t),0};
            ComPtr<ID3D11Texture2D> texture; Require(SUCCEEDED(device->CreateTexture2D(&d,&source,&texture)),"Blur input failed");
            blur.BlurUI(texture.Get(),64,64); Require(blur.uiSRV()!=nullptr,"Kawase blur output is missing");
            ComPtr<ID3D11Resource> resource; blur.uiSRV()->GetResource(&resource);
            d.BindFlags=0; d.Usage=D3D11_USAGE_STAGING; d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
            ComPtr<ID3D11Texture2D> staging; Require(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&staging)),"Blur readback allocation failed");
            context->CopyResource(staging.Get(),resource.Get()); D3D11_MAPPED_SUBRESOURCE map{};
            Require(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&map)),"Blur readback failed");
            const auto* pixel=static_cast<const std::uint8_t*>(map.pData)+32*map.RowPitch+32*4;
            const bool softened=pixel[0]>80&&pixel[0]<180&&pixel[2]>80&&pixel[2]<180;
            context->Unmap(staging.Get(),0); Require(softened,"Kawase passes left the checkerboard unblurred");
            Require(!blur.ok(),"Synthetic blur incorrectly reported desktop capture");
        }
        auto frame=[&](UINT w,UINT h,bool visible,bool second=false,ID3D11Texture2D* scene=nullptr,bool red=false) {
            auto& io=ImGui::GetIO(); io.DisplaySize={float(w),float(h)}; io.DeltaTime=1.f/60;
            ImGui_ImplDX11_NewFrame(); ImGui::NewFrame();
            glass.BeginFrame(nullptr,io.DisplaySize,visible,false,scene);
            if(visible) {
                auto& draw=*ImGui::GetBackgroundDrawList();
                draw.AddRectFilled({140,140},{160,160},IM_COL32(0,255,0,255));
                mcov::LiquidGlass::Panel(draw,{40,30},{float(w)-40,float(h)-30},22);
                // HUD liegt hinter dem Glas, Bedienelemente liegen davor.
                draw.AddRectFilled({90,90},{110,110},IM_COL32(255,255,255,255));
                if(second) {
                    mcov::LiquidGlass::Panel(draw,{180,40},{float(w)-20,170},22);
                    draw.AddRectFilled({250,90},{270,110},IM_COL32(255,0,255,255));
                    ImTextureID previous=0; int images=0;
                    for(const auto& cmd:draw.CmdBuffer)
                        if(cmd.ElemCount&&cmd.TextureId!=io.Fonts->TexID&&cmd.TextureId!=previous) { previous=cmd.TextureId; ++images; }
                    Require(images==2,"Second glass window did not receive an independent material texture");
                }
            }
            ImGui::Render(); glass.RenderTexture();
            D3D11_TEXTURE2D_DESC d{}; d.Width=w; d.Height=h; d.MipLevels=d.ArraySize=1;
            d.Format=DXGI_FORMAT_R8G8B8A8_UNORM; d.SampleDesc.Count=1; d.BindFlags=D3D11_BIND_RENDER_TARGET;
            ComPtr<ID3D11Texture2D> output,staging; ComPtr<ID3D11RenderTargetView> target;
            Require(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&output)),"Output texture failed");
            Require(SUCCEEDED(device->CreateRenderTargetView(output.Get(),nullptr,&target)),"RTV failed");
            auto* rtv=target.Get(); context->OMSetRenderTargets(1,&rtv,nullptr);
            const float clear[4]{}; context->ClearRenderTargetView(rtv,clear);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            d.BindFlags=0; d.Usage=D3D11_USAGE_STAGING; d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
            Require(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&staging)),"Readback texture failed");
            context->CopyResource(staging.Get(),output.Get());
            D3D11_MAPPED_SUBRESOURCE map{};
            Require(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&map)),"Readback failed");
            const auto pixel=[&](int x,int y,int channel) {
                return static_cast<const std::uint8_t*>(map.pData)[y*map.RowPitch+x*4+channel];
            };
            Require(pixel(0,0,3)==0,"Glass filled transparent desktop outside the panel");
            if(visible) {
                Require(pixel(w/2,h/2,3)>250,"Glass shader did not fill its material");
                if(!scene) Require(pixel(w/2,h/2,2)>pixel(w/2,h/2,0),"Fallback glass lost its cool tint");
                else Require(red?pixel(220,200,0)>pixel(220,200,1)+30:pixel(220,200,1)>pixel(220,200,0)+30,
                    "Glass did not react to the changed scene texture");
                Require(pixel(100,100,0)>250,"Glass overpainted foreground controls");
                if(second) Require(pixel(260,100,0)>250&&pixel(260,100,2)>250,"Second pane overpainted its foreground controls");
                Require(scene?std::abs(pixel(150,150,1)-pixel(220,200,1))<20:pixel(150,150,1)<100,
                    "Background HUD overpainted the glass");
                Require(pixel(42,32,3)<50,"Rounded glass corner became opaque");
                Require(!glass.liveBackdrop(),"Offline frame reported live desktop capture");
            } else Require(pixel(w/2,h/2,3)==0,"Hidden menu left a stale glass image");
            context->Unmap(staging.Get(),0);
        };
        frame(400,300,true); frame(520,380,true,true); frame(400,300,true,true); frame(400,300,false);
        {
            D3D11_TEXTURE2D_DESC d{}; d.Width=400; d.Height=300; d.MipLevels=d.ArraySize=1;
            d.Format=DXGI_FORMAT_R8G8B8A8_UNORM; d.SampleDesc.Count=1;
            d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
            ComPtr<ID3D11Texture2D> scene;
            Require(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&scene)),"Scene texture allocation failed");
            std::vector<std::uint32_t> pixels(400*300,IM_COL32(240,30,30,255));
            context->UpdateSubresource(scene.Get(),0,nullptr,pixels.data(),400*4,0);
            frame(400,300,true,true,scene.Get(),true);
            std::fill(pixels.begin(),pixels.end(),IM_COL32(30,240,30,255));
            context->UpdateSubresource(scene.Get(),0,nullptr,pixels.data(),400*4,0);
            frame(400,300,true,true,scene.Get(),false);
            frame(400,300,true); // Kein alter Test-Frame darf als Desktop gelten.
        }
        glass.Shutdown(); Require(!glass.ready(),"Shutdown retained the renderer");
        Require(glass.Initialize(device.Get(),context.Get()),"Renderer could not be reinitialized");
        frame(400,300,true); glass.Shutdown(); ImGui_ImplDX11_Shutdown(); ImGui::DestroyContext();
        std::cout<<"PASS liquid glass: shaders, Kawase blur pixels, alpha, tint, rounded clipping, independent panes, foreground, resize, idle, reinitialization\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}

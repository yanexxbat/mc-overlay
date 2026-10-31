// Echtes HWND und Swapchain; keine Desktop-Aufnahme, kein sichtbares Fenster.
#include "common/flat_app.hpp"
#include "injector/launcher_ui.hpp"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include <d3d11.h>
#include <dxgi.h>
#include <dwmapi.h>
#include <wrl/client.h>
#include <wincodec.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
void Require(bool ok,const char* reason) { if(!ok) throw std::runtime_error(reason); }
void Hr(HRESULT value) { Require(SUCCEEDED(value),"Native frame PNG encoding failed"); }
void Save(const std::filesystem::path& path,int width,int height,const D3D11_MAPPED_SUBRESOURCE& mapped) {
    if(path.empty()) return;
    ComPtr<IWICImagingFactory> wic;
    Hr(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&wic)));
    ComPtr<IWICStream> stream; Hr(wic->CreateStream(&stream));
    Hr(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE));
    ComPtr<IWICBitmapEncoder> encoder; Hr(wic->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder));
    Hr(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache));
    ComPtr<IWICBitmapFrameEncode> frame; Hr(encoder->CreateNewFrame(&frame,nullptr));
    Hr(frame->Initialize(nullptr)); Hr(frame->SetSize(width,height));
    auto format=GUID_WICPixelFormat32bppRGBA; Hr(frame->SetPixelFormat(&format));
    ComPtr<IWICBitmap> bitmap;
    Hr(wic->CreateBitmapFromMemory(width,height,GUID_WICPixelFormat32bppRGBA,
        mapped.RowPitch,mapped.RowPitch*height,static_cast<BYTE*>(mapped.pData),&bitmap));
    ComPtr<IWICFormatConverter> converted; Hr(wic->CreateFormatConverter(&converted));
    Hr(converted->Initialize(bitmap.Get(),format,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
    Hr(frame->WriteSource(converted.Get(),nullptr)); Hr(frame->Commit()); Hr(encoder->Commit());
}
namespace mcov {
struct FlatAppRenderingCheck {
    static void Frame(FlatApp& app,launcher::View& view,const std::filesystem::path& png={}, float dt=1.f/60) {
        app.ResizeIfNeeded();
        ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
        ImGui::GetIO().DeltaTime=dt; launcher::Render(view,{float(app.width_),float(app.height_)});
        app.DrawFrame();
        ComPtr<ID3D11Resource> resource; app.backBuffer_->GetResource(&resource);
        ComPtr<ID3D11Texture2D> back; Require(SUCCEEDED(resource.As(&back)),"Missing swapchain texture");
        D3D11_TEXTURE2D_DESC desc{}; back->GetDesc(&desc);
        desc.BindFlags=desc.MiscFlags=0; desc.Usage=D3D11_USAGE_STAGING; desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        ComPtr<ID3D11Texture2D> readback;
        Require(SUCCEEDED(app.device_->CreateTexture2D(&desc,nullptr,&readback)),"Readback allocation failed");
        app.context_->CopyResource(readback.Get(),back.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        Require(SUCCEEDED(app.context_->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped)),"Readback failed");
        int opaque=0,graphite=0,transparent=0,ink=0,mascot=0;
        const bool compact=app.width_<620||app.height_<560;
        const int mascotX=compact?20:28,mascotY=compact?21:31;
        for(int y=0;y<app.height_;++y) for(int x=0;x<app.width_;++x) {
            const auto* pixel=static_cast<const unsigned char*>(mapped.pData)+y*mapped.RowPitch+x*4;
            if(pixel[3]==255) ++opaque;
            if(pixel[0]==25&&pixel[1]==28&&pixel[2]==27&&pixel[3]==255) ++graphite;
            if(pixel[3]==0) ++transparent;
            if(pixel[0]>150&&pixel[1]>150&&pixel[2]>150) ++ink;
            if(x>=mascotX&&x<mascotX+30&&y>=mascotY&&y<mascotY+30&&pixel[1]>pixel[0]*1.2f&&pixel[1]>pixel[2]*1.4f&&pixel[3]>128) ++mascot;
        }
        Save(png,app.width_,app.height_,mapped);
        app.context_->Unmap(readback.Get(),0);
        Require(view.introReady||opaque>app.width_*app.height_*.75,"Native launcher lost its opaque workspace panel");
        Require(view.introReady||graphite>app.width_*app.height_*.3,"Native launcher lost its Graphite / Sage palette");
        Require(transparent>app.width_*20,"Native launcher lost its transparent margins");
        Require(view.introReady||ink>500,"Native launcher controls are invisible");
        Require(view.introReady||mascot>40,"Keim mascot lost its colour or is missing from the launcher");
        Require(SUCCEEDED(app.swapChain_->Present(0,0)),"Native launcher presentation failed");
        Require(!IsWindowVisible(static_cast<HWND>(app.hwnd_)),"Fixture became visible");
        auto region=CreateRectRgn(0,0,0,0);
        const int kind=GetWindowRgn(static_cast<HWND>(app.hwnd_),region);
        const bool rounded=kind!=ERROR&&!PtInRegion(region,0,0)&&PtInRegion(region,app.width_/2,app.height_/2);
        DeleteObject(region);
        Require(rounded,"Native launcher lost its rounded region");
        std::cout<<"Native frame "<<app.width_<<'x'<<app.height_<<": opaque="<<opaque<<" graphite="<<graphite<<" transparent="<<transparent<<" visible ink="<<ink<<'\n';
    }
};
}
int main(int argc,char** argv) {
    const auto apartment=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    struct Apartment { HRESULT result; ~Apartment() { if(SUCCEEDED(result)) CoUninitialize(); } } cleanup{apartment};
    try {
        const auto output=argc==2?std::filesystem::path(argv[1]):std::filesystem::path{};
        if(!output.empty()) std::filesystem::create_directories(output);
        mcov::FlatApp app; std::string failure;
        if(!app.Initialize(L"Keim hidden launcher check",640,640,&failure)) throw std::runtime_error(failure);
        Require(GetClassLongPtrW(static_cast<HWND>(app.hwnd()),GCLP_HICON)!=0,"Keim Windows icon is missing");
        Require(!IsWindowVisible(static_cast<HWND>(app.hwnd())),"Launcher shown before first frame");
        using namespace mcov::launcher;
        for(float hz:{30.f,60.f,144.f}) {
            anim::Tween tween;
            for(int i=0;i<int(hz*.1f);++i) tween.step(1,1/hz,true);
            const float elapsed=int(hz*.1f)/hz;
            Require(std::abs(tween.value-anim::easeOutCubic(elapsed/anim::Transition))<.0001f,"Tween is frame dependent");
            tween.step(0,0,false); Require(tween.value==0,"Reduced motion did not snap");
        }
        auto frame=[&](View& v,const char* name,float dt=1.f/60) {
            mcov::FlatAppRenderingCheck::Frame(app,v,output/name,dt);
        };
        View view; view.introReady=true;
        frame(view,"intro.png",.35f);
        Require(!view.motion.introDone,"Intro ended early");
        frame(view,"intro-exit.png",1.9f);
        frame(view,"start.png",.3f);
        Require(view.motion.introDone,"Intro did not complete");
        view.tab=1; view.log="first\nsecond"; frame(view,"logs-start.png",.25f);
        frame(view,"logs.png",.25f);
        const auto birth=view.motion.lineBirth.front();
        view.log+="\nthird"; frame(view,"logs-append.png",.1f);
        Require(view.motion.lineBirth.front()==birth,"Existing logs restarted fade");
        View off; off.introReady=true; off.animations=false;
        frame(off,"animations-off.png"); Require(off.motion.introDone,"Disabled intro appeared");
        View skip; skip.introReady=true;
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Space,true);
        frame(skip,"skip.png"); Require(skip.motion.introDone,"Any-key skip failed");
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Space,false);
        std::cout<<"PASS motion: timestep independence, reduced motion, intro/skip, log continuity\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}


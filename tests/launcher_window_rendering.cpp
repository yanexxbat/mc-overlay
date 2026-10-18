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
    static void Frame(FlatApp& app,launcher::View& view,const std::filesystem::path& png={}) {
        app.ResizeIfNeeded();
        ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
        launcher::Render(view,{float(app.width_),float(app.height_)});
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
        Require(opaque>app.width_*app.height_*.75,"Native launcher lost its opaque workspace panel");
        Require(graphite>app.width_*app.height_*.3,"Native launcher lost its Graphite / Sage palette");
        Require(transparent>app.width_*20,"Native launcher lost its transparent margins");
        Require(ink>500,"Native launcher controls are invisible");
        Require(mascot>40,"Keim mascot lost its colour or is missing from the launcher");
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
        mcov::launcher::View view;
        view.licensed=true;
        view.hardwareId="S-1-5-21-1234567890-1234567890-1234567890-1001";
        mcov::FlatAppRenderingCheck::Frame(app,view,output.empty()?output:output/"launcher-native-start.png");
        view.instances.push_back({123,"Minecraft fixture","Java 21",false,false}); view.selected=0;
        mcov::FlatAppRenderingCheck::Frame(app,view);
        view.tab=1; view.diagnostic="Connection details appear here.";
        SetWindowPos(static_cast<HWND>(app.hwnd()),nullptr,0,0,480,380,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        mcov::FlatAppRenderingCheck::Frame(app,view,output.empty()?output:output/"launcher-native-diagnose.png");
        std::cout<<"PASS native launcher: hidden initialization, graphite workspace, controls, resize, rounded clipping\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}

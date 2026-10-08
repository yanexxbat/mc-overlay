#include "windows_backdrop_capture.hpp"
#include "backdrop.h"
#include <d3d11.h>
#include <d3d11_4.h>
#include <roapi.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

namespace mcov {
namespace {
using namespace winrt::Windows::Graphics;
using namespace winrt::Windows::Graphics::Capture;
using namespace winrt::Windows::Graphics::DirectX;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
}
struct WindowsBackdropCapture::Impl {
    GraphicsCaptureItem item{nullptr};
    Direct3D11CaptureFramePool pool{nullptr};
    GraphicsCaptureSession session{nullptr};
    IDirect3DDevice device{nullptr};
    SizeInt32 size{};
    int x=0,y=0;
    bool apartment=false;
    bool apartmentReady=false;
    HRESULT error=S_OK;
    winrt::com_ptr<ID3D11Multithread> threading;
    BOOL wasProtected=FALSE;
};
WindowsBackdropCapture::WindowsBackdropCapture():impl_(std::make_unique<Impl>()) {}
WindowsBackdropCapture::~WindowsBackdropCapture() {
    Stop();
    // WinRT-Factory-Caches erst beim Ende des Wrappers abbauen. Ein Uninitialize
    // bei jedem Schliessen des Menues hinterliesse ungueltige Factory-Zeiger.
    if(impl_->apartment) { winrt::clear_factory_cache(); RoUninitialize(); }
}
bool WindowsBackdropCapture::running() const { return bool(impl_->session); }
HRESULT WindowsBackdropCapture::error() const { return impl_->error; }
void WindowsBackdropCapture::Stop() {
    auto& p=*impl_;
    try { if(p.session) p.session.Close(); } catch(...) {}
    try { if(p.pool) p.pool.Close(); } catch(...) {}
    p.session=nullptr; p.pool=nullptr; p.item=nullptr; p.device=nullptr;
    if(p.threading) p.threading->SetMultithreadProtected(p.wasProtected);
    p.threading=nullptr;
}
bool WindowsBackdropCapture::Start(ID3D11Device* device,HMONITOR monitor) {
    Stop(); auto& p=*impl_; p.error=S_OK;
    if(!device||!monitor) { p.error=E_INVALIDARG; return false; }
    if(!p.apartmentReady) {
        const auto hr=RoInitialize(RO_INIT_MULTITHREADED);
        if(FAILED(hr)&&hr!=RPC_E_CHANGED_MODE) { p.error=hr; return false; }
        p.apartment=SUCCEEDED(hr); p.apartmentReady=true;
    }
    try {
        if(!GraphicsCaptureSession::IsSupported()) { p.error=E_NOTIMPL; Stop(); return false; }
        MONITORINFO info{sizeof(info)};
        if(!GetMonitorInfoW(monitor,&info)) winrt::throw_last_error();
        p.x=info.rcMonitor.left; p.y=info.rcMonitor.top;
        auto interop=winrt::get_activation_factory<GraphicsCaptureItem,IGraphicsCaptureItemInterop>();
        winrt::check_hresult(interop->CreateForMonitor(monitor,winrt::guid_of<GraphicsCaptureItem>(),winrt::put_abi(p.item)));
        auto dxgi=winrt::com_ptr<ID3D11Device>{}; dxgi.copy_from(device);
        winrt::com_ptr<ID3D11DeviceContext> context; device->GetImmediateContext(context.put());
        p.threading=context.try_as<ID3D11Multithread>();
        if(p.threading) p.wasProtected=p.threading->SetMultithreadProtected(TRUE);
        auto dxgiDevice=dxgi.as<IDXGIDevice>();
        winrt::com_ptr<IInspectable> inspectable;
        winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.get(),inspectable.put()));
        p.device=inspectable.as<IDirect3DDevice>(); p.size=p.item.Size();
        // Kein Dispatcher, kein eigener Callback: neueste Frames werden gepollt.
        p.pool=Direct3D11CaptureFramePool::CreateFreeThreaded(p.device,DirectXPixelFormat::B8G8R8A8UIntNormalized,2,p.size);
        p.session=p.pool.CreateCaptureSession(p.item);
        // Auf aelteren Windows-Versionen bleiben nicht vorhandene Optionen aus.
        try { p.session.IsCursorCaptureEnabled(false); } catch(const winrt::hresult_error&) {}
        try { p.session.IsBorderRequired(false); } catch(const winrt::hresult_error&) {}
        p.session.StartCapture(); return true;
    } catch(const winrt::hresult_error& e) { p.error=e.code(); Stop(); return false; }
}
bool WindowsBackdropCapture::Poll(Glass::Backdrop& backdrop) {
    auto& p=*impl_; if(!p.pool) return false;
    try {
        Direct3D11CaptureFrame latest{nullptr};
        // Hoechstens die zwei Pool-Frames abholen; nie auf ein Frame warten.
        for(int i=0;i<2;++i) {
            auto frame=p.pool.TryGetNextFrame(); if(!frame) break;
            if(latest) latest.Close(); latest=std::move(frame);
        }
        if(!latest) return false;
        const auto size=latest.ContentSize();
        const auto frameSize=p.size;
        bool uploaded=false;
        if(size.Width==frameSize.Width&&size.Height==frameSize.Height) {
            auto access=latest.Surface().as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
            winrt::com_ptr<ID3D11Texture2D> texture;
            winrt::check_hresult(access->GetInterface(__uuidof(ID3D11Texture2D),texture.put_void()));
            // Kopieren/Blur vor Close: danach darf Windows den Pool-Slot wiederverwenden.
            uploaded=backdrop.UpdateTexture(texture.get(),p.x,p.y);
        }
        latest.Close();
        if(size.Width>0&&size.Height>0&&(size.Width!=frameSize.Width||size.Height!=frameSize.Height)) {
            p.size=size; p.pool.Recreate(p.device,DirectXPixelFormat::B8G8R8A8UIntNormalized,2,p.size);
        }
        return uploaded;
    } catch(const winrt::hresult_error& e) { p.error=e.code(); Stop(); return false; }
}
}

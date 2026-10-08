#include "liquid_glass.hpp"
#include "glass.h"
#include "backdrop.h"
#include "windows_backdrop_capture.hpp"
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <windows.h>

namespace mcov {
namespace {
using Microsoft::WRL::ComPtr;
thread_local LiquidGlass* activeGlass=nullptr;
constexpr DWORD kExcluded=0x11;
constexpr ULONGLONG kCapturePeriodMs=33;
}
struct LiquidGlass::Impl {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    // Ein Renderziel je Fenster vermeidet doppelte Komposition und falsche Z-Order.
    struct Pane {
        ComPtr<ID3D11Texture2D> texture;
        ComPtr<ID3D11RenderTargetView> target;
        ComPtr<ID3D11ShaderResourceView> image;
        int primitive=-1;
    };
    std::array<Pane,4> panes{};
    int paneCount=0;
    ComPtr<ID3D11ShaderResourceView> fallback;
    Glass::Renderer renderer;
    Glass::Backdrop backdrop;
    WindowsBackdropCapture windowsCapture;
    HWND window=nullptr;
    HMONITOR monitor=nullptr;
    DWORD previousAffinity=0;
    bool changedAffinity=false, captureInitialized=false, usable=false, visible=false;
    bool sceneProvided=false;
    int width=0,height=0;
    ULONGLONG captureAt=0;
    ULONGLONG captureStarted=0,windowsRetryAt=0;
    char captureStatus[224]="Menu closed";

    void RestoreAffinity() {
        if(changedAffinity&&IsWindow(window)) SetWindowDisplayAffinity(window,previousAffinity);
        changedAffinity=false;
    }
    bool Resize(int w,int h) {
        if(w==width&&h==height) return true;
        // ImGui darf beim Resize keine alte SRV als Renderziel gebunden halten.
        ID3D11ShaderResourceView* none=nullptr; context->PSSetShaderResources(0,1,&none);
        context->OMSetRenderTargets(0,nullptr,nullptr);
        for(auto& pane:panes) pane={};
        width=w; height=h; return true;
    }
    bool EnsurePane(int index) {
        auto& pane=panes[index];
        if(pane.image) return true;
        pane={};
        D3D11_TEXTURE2D_DESC d{};
        d.Width=width; d.Height=height; d.MipLevels=d.ArraySize=1; d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        d.SampleDesc.Count=1; d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        if(FAILED(device->CreateTexture2D(&d,nullptr,&pane.texture))||
           FAILED(device->CreateRenderTargetView(pane.texture.Get(),nullptr,&pane.target))||
           FAILED(device->CreateShaderResourceView(pane.texture.Get(),nullptr,&pane.image))) { pane={}; return false; }
        return true;
    }
};
LiquidGlass::LiquidGlass():impl_(std::make_unique<Impl>()) {}
LiquidGlass::~LiquidGlass() { Shutdown(); }
bool LiquidGlass::Initialize(ID3D11Device* device,ID3D11DeviceContext* context) {
    Shutdown(); auto& p=*impl_; p.device=device; p.context=context;
    if(!device||!context||!p.renderer.Init(device,context)) { Shutdown(); return false; }
    // Offline/WARP/gesperrter Desktop: ein ruhiges Material statt undefinierter Texturen.
    constexpr int side=64;
    std::array<ImU32,side*side> pixels{};
    for(int y=0;y<side;++y) for(int x=0;x<side;++x) {
        const float blue=std::exp(-((x-10.f)*(x-10.f)+(y-5.f)*(y-5.f))/900.f);
        const float violet=std::exp(-((x-60.f)*(x-60.f)+(y-48.f)*(y-48.f))/1000.f);
        pixels[y*side+x]=IM_COL32(int(19+16*violet+7*blue),int(25+17*blue),int(41+27*blue+24*violet),255);
    }
    D3D11_TEXTURE2D_DESC d{}; d.Width=d.Height=side; d.MipLevels=d.ArraySize=1;
    d.Format=DXGI_FORMAT_R8G8B8A8_UNORM; d.SampleDesc.Count=1;
    d.Usage=D3D11_USAGE_IMMUTABLE; d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{pixels.data(),side*sizeof(ImU32),0}; ComPtr<ID3D11Texture2D> fallback;
    if(FAILED(device->CreateTexture2D(&d,&data,&fallback))||FAILED(device->CreateShaderResourceView(fallback.Get(),nullptr,&p.fallback))) {
        Shutdown(); return false;
    }
    auto& m=Glass::EditParams(Glass::Material::Thick);
    // brightness ist ein additiver Offset; refr_band wird in Pixeln angegeben.
    m.blur_mix=.78f; m.saturation=.9f; m.brightness=-.02f; m.contrast=.94f;
    m.tint_rgb[0]=.045f; m.tint_rgb[1]=.050f; m.tint_rgb[2]=.063f; m.tint_opacity=.56f;
    m.grain=.012f; m.refr_strength=9; m.refr_band=18.f; m.highlight=.40f;
    m.inner_shadow=.12f; m.border_intensity=.20f; m.border_width=.65f;
    m.shadow_strength=.22f; m.shadow_radius=20; m.shadow_off_x=0; m.shadow_off_y=9;
    m.sheen=.08f; m.chroma=.10f;
    Glass::GlassEdgeConfig edge; edge.cursor_glow=.15f; edge.grain_amt=.3f;
    edge.specular_amt=.50f; edge.ambient_rim=.14f; edge.edge_shadow=.10f;
    p.renderer.SetEdgeConfig(edge); p.renderer.SetLights(-35,.8f,145,.35f);
    p.usable=true; return true;
}
void LiquidGlass::Shutdown() {
    if(activeGlass==this) activeGlass=nullptr;
    auto& p=*impl_; p.windowsCapture.Stop(); p.RestoreAffinity(); p.backdrop.Shutdown(); p.renderer.Shutdown();
    for(auto& pane:p.panes) pane={}; p.fallback.Reset(); p.paneCount=0;
    p.context.Reset(); p.device.Reset(); p.window=nullptr; p.monitor=nullptr; p.captureInitialized=false;
    p.usable=p.visible=false; p.width=p.height=0;
    p.captureAt=p.captureStarted=p.windowsRetryAt=0;
}
bool LiquidGlass::ready() const { return impl_->usable; }
bool LiquidGlass::liveBackdrop() const { return impl_->visible&&impl_->changedAffinity&&impl_->backdrop.ok(); }
const char* LiquidGlass::captureStatus() const { return impl_->captureStatus; }
void LiquidGlass::BeginFrame(void* hwnd,ImVec2 size,bool visible,bool allowCapture,ID3D11Texture2D* sceneTexture) {
    if(impl_->sceneProvided&&!sceneTexture) impl_->backdrop.Invalidate();
    auto& p=*impl_; p.paneCount=0; p.visible=visible&&p.usable; p.sceneProvided=false;
    activeGlass=p.visible?this:nullptr;
    if(!p.visible) {
        p.windowsCapture.Stop(); p.backdrop.StopCapture();
        p.RestoreAffinity(); p.captureAt=p.captureStarted=p.windowsRetryAt=0;
        std::snprintf(p.captureStatus,sizeof(p.captureStatus),"Menu closed"); return;
    }
    if(!p.Resize(std::max(1,int(size.x)),std::max(1,int(size.y)))) { p.visible=false; activeGlass=nullptr; p.RestoreAffinity(); return; }
    const auto window=static_cast<HWND>(hwnd);
    const HMONITOR monitor=window?MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST):nullptr;
    if(window!=p.window||monitor!=p.monitor) {
        p.windowsCapture.Stop(); p.RestoreAffinity(); p.backdrop.Shutdown(); p.captureInitialized=false; p.window=window;
        p.captureAt=p.captureStarted=p.windowsRetryAt=0;
        p.monitor=monitor;
    }
    if(sceneTexture) {
        p.windowsCapture.Stop(); p.backdrop.StopCapture(); p.RestoreAffinity();
        p.captureAt=p.captureStarted=p.windowsRetryAt=0;
        if(!p.captureInitialized) p.captureInitialized=p.backdrop.Init(p.device.Get(),p.context.Get(),nullptr);
        p.sceneProvided=p.captureInitialized&&p.backdrop.UpdateTexture(sceneTexture,0,0);
        std::snprintf(p.captureStatus,sizeof(p.captureStatus),p.sceneProvided?"Provided texture / no desktop capture":"Provided texture upload failed");
        p.renderer.BeginFrame(p.width,p.height,0,0,p.sceneProvided?p.backdrop.width():p.width,
            p.sceneProvided?p.backdrop.height():p.height,ImGui::GetIO().MousePos);
        return;
    }
    bool captureSafe=false;
    if(allowCapture&&IsWindow(window)) {
        if(!p.changedAffinity) {
            const bool read=GetWindowDisplayAffinity(window,&p.previousAffinity)!=FALSE;
            if(read&&SetWindowDisplayAffinity(window,kExcluded))
                p.changedAffinity=true;
            else std::snprintf(p.captureStatus,sizeof(p.captureStatus),"Capture exclusion failed (Windows error %lu)",GetLastError());
        }
        captureSafe=p.changedAffinity;
    } else {
        p.windowsCapture.Stop(); p.backdrop.StopCapture();
        p.captureAt=p.captureStarted=p.windowsRetryAt=0;
        p.RestoreAffinity();
        std::snprintf(p.captureStatus,sizeof(p.captureStatus),"Desktop capture disabled");
    }
    const auto now=GetTickCount64();
    if(captureSafe&&now>=p.captureAt) {
        if(!p.captureStarted) p.captureStarted=now;
        if(!p.captureInitialized) p.captureInitialized=p.backdrop.Init(p.device.Get(),p.context.Get(),window);
        if(p.captureInitialized) {
            if(p.windowsCapture.running()) {
                p.windowsCapture.Poll(p.backdrop);
                if(!p.windowsCapture.running()) { p.backdrop.Invalidate(); p.windowsRetryAt=now+2000; }
            } else p.backdrop.Capture();
            // DXGI haengt an der Monitor-GPU. WGC kann den Monitor auf das
            // bestehende Rendergeraet liefern, auch bei Hybrid-/Multi-GPU-Systemen.
            if(!p.backdrop.ok()&&!p.windowsCapture.running()&&now-p.captureStarted>=750&&now>=p.windowsRetryAt) {
                p.windowsCapture.Start(p.device.Get(),monitor); p.windowsRetryAt=now+2000;
                if(p.windowsCapture.running()) {
                    p.backdrop.StopCapture();
                    p.windowsCapture.Poll(p.backdrop);
                }
            }
        }
        if(p.backdrop.ok()) std::snprintf(p.captureStatus,sizeof(p.captureStatus),"Live / %s",p.windowsCapture.running()?"Windows Graphics Capture":"DXGI Desktop Duplication");
        else if(p.windowsCapture.running()) std::snprintf(p.captureStatus,sizeof(p.captureStatus),"Waiting for Windows Graphics Capture frame");
        else std::snprintf(p.captureStatus,sizeof(p.captureStatus),"%s (0x%08lX); WGC 0x%08lX",p.backdrop.status(),unsigned long(p.backdrop.lastError()),unsigned long(p.windowsCapture.error()));
        p.captureAt=now+(p.captureInitialized?kCapturePeriodMs:1000);
    }
    POINT origin{}; if(window) ClientToScreen(window,&origin);
    const bool live=captureSafe&&p.backdrop.ok();
    p.renderer.BeginFrame(p.width,p.height,live?origin.x-p.backdrop.originX():0,
        live?origin.y-p.backdrop.originY():0,live?p.backdrop.width():p.width,
        live?p.backdrop.height():p.height,ImGui::GetIO().MousePos);
}
void LiquidGlass::RenderTexture() {
    auto& p=*impl_; if(!p.visible||!p.paneCount) return;
    ID3D11ShaderResourceView* none[2]{}; p.context->PSSetShaderResources(0,2,none);
    D3D11_VIEWPORT viewport{0,0,float(p.width),float(p.height),0,1}; p.context->RSSetViewports(1,&viewport);
    const bool live=(p.sceneProvided||p.changedAffinity)&&p.backdrop.ok();
    for(int i=0;i<p.paneCount;++i) {
        auto& pane=p.panes[i]; auto* target=pane.target.Get();
        p.context->OMSetRenderTargets(1,&target,nullptr);
        const float clear[4]{}; p.context->ClearRenderTargetView(target,clear);
        p.renderer.Render(live?p.backdrop.heavySRV():p.fallback.Get(),live?p.backdrop.softSRV():p.fallback.Get(),pane.primitive);
    }
    p.context->OMSetRenderTargets(0,nullptr,nullptr);
}
void LiquidGlass::Panel(ImDrawList& draw,ImVec2 min,ImVec2 max,float radius) {
    auto* active=activeGlass;
    const auto* viewport=ImGui::GetMainViewport();
    draw.PushClipRect(viewport->Pos,{viewport->Pos.x+viewport->Size.x,viewport->Pos.y+viewport->Size.y},false);
    if(active&&active->impl_->paneCount<int(active->impl_->panes.size())&&active->impl_->EnsurePane(active->impl_->paneCount)) {
        auto& p=*active->impl_; auto& slot=p.panes[p.paneCount++];
        Glass::Primitive pane; pane.cx=(min.x+max.x)*.5f; pane.cy=(min.y+max.y)*.5f;
        pane.hw=(max.x-min.x)*.5f; pane.hh=(max.y-min.y)*.5f; pane.corner_radius=radius;
        pane.fade=1; pane.material=Glass::Material::Thick; slot.primitive=int(p.renderer.Submit(pane));
        draw.PushClipRect({0,0},{float(p.width),float(p.height)},false);
        // Das Material wird nach ImGui::Render in dieselbe Textur geschrieben.
        // AddImage bewahrt die normale Reihenfolge: Welt/HUD, Glas, Bedienelemente.
        draw.AddImage(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(slot.image.Get())),{0,0},{float(p.width),float(p.height)});
        draw.PopClipRect();
    } else {
        for(int i=8;i>0;--i) {
            const float spread=float(i)*2;
            draw.AddRectFilled({min.x-spread,min.y-spread+7},{max.x+spread,max.y+spread+7},IM_COL32(0,0,0,3),radius+spread);
        }
        draw.AddRectFilled(min,max,IM_COL32(25,32,51,250),radius);
        draw.AddRect(min,max,IM_COL32(210,222,255,45),radius);
    }
    draw.PopClipRect();
}
} // namespace mcov

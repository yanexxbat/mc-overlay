// Opt-in Diagnose: keine JVM, keine gespeicherten Desktop-Bilder.
#include "common/liquid_glass.hpp"
#include "common/windows_backdrop_capture.hpp"
#include "backdrop.h"
#include "imgui_impl_dx11.h"
#include <wrl/client.h>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <cstring>
using Microsoft::WRL::ComPtr;
int main(int argc,char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
    std::cout<<std::unitbuf;
    SetUnhandledExceptionFilter([](EXCEPTION_POINTERS* e)->LONG {
        HMODULE module=nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(e->ExceptionRecord->ExceptionAddress),&module);
        wchar_t name[MAX_PATH]{}; GetModuleFileNameW(module,name,MAX_PATH);
        std::cerr<<"Unhandled exception 0x"<<std::hex<<e->ExceptionRecord->ExceptionCode<<" offset=0x"<<
            (reinterpret_cast<uintptr_t>(e->ExceptionRecord->ExceptionAddress)-reinterpret_cast<uintptr_t>(module))<<'\n';
        std::wcerr<<name<<'\n';
        return EXCEPTION_EXECUTE_HANDLER;
    });
    const bool sta=argc==2&&!std::strcmp(argv[1],"--live-sta");
    if(argc!=2||(!sta&&std::strcmp(argv[1],"--live"))) { std::cout<<"Use --live or --live-sta to check the current Windows capture APIs.\n"; return 0; }
    const HRESULT apartment=sta?CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED):E_FAIL;
    struct Apartment { HRESULT result; ~Apartment() { if(SUCCEEDED(result)) CoUninitialize(); } } apartmentCleanup{apartment};
    HWND window=nullptr;
    try {
        WNDCLASSW wc{}; wc.hInstance=GetModuleHandleW(nullptr); wc.lpszClassName=L"mcov_capture_probe"; wc.lpfnWndProc=DefWindowProcW;
        RegisterClassW(&wc);
        window=CreateWindowExW(WS_EX_LAYERED|WS_EX_TOOLWINDOW,wc.lpszClassName,L"mcov capture probe",WS_POPUP,0,0,64,64,nullptr,nullptr,wc.hInstance,nullptr);
        if(!window) throw std::runtime_error("Probe window creation failed");
        SetLayeredWindowAttributes(window,0,255,LWA_ALPHA);
        DWORD affinity=0;
        const BOOL read=GetWindowDisplayAffinity(window,&affinity); const auto readError=GetLastError();
        const BOOL write=SetWindowDisplayAffinity(window,0x11); const auto writeError=GetLastError();
        std::cout<<"Affinity read="<<read<<" error="<<(read?0:readError)<<" write="<<write<<" error="<<(write?0:writeError)<<'\n';
        ComPtr<ID3D11Device> device; ComPtr<ID3D11DeviceContext> context;
        if(FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)))
            throw std::runtime_error("Hardware D3D11 unavailable");
        Glass::Backdrop backdrop;
        struct Cleanup { Glass::Backdrop& b; ~Cleanup() { b.Shutdown(); } } cleanup{backdrop};
        if(!backdrop.Init(device.Get(),context.Get(),window)) throw std::runtime_error("Backdrop initialization failed");
        const auto start=GetTickCount64();
        while(!backdrop.ok()&&GetTickCount64()-start<1000) { backdrop.Capture(); Sleep(16); }
        std::cout<<"DXGI live="<<backdrop.ok()<<" status="<<backdrop.status()<<" error=0x"<<std::hex<<backdrop.lastError()<<std::dec<<'\n';
        backdrop.StopCapture();
        mcov::WindowsBackdropCapture wgc;
        const bool started=wgc.Start(device.Get(),MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST));
        const auto wgcStart=GetTickCount64();
        while(started&&wgc.running()&&!backdrop.ok()&&GetTickCount64()-wgcStart<2000) { wgc.Poll(backdrop); Sleep(16); }
        std::cout<<"WGC start="<<started<<" live="<<backdrop.ok()<<" error=0x"<<std::hex<<wgc.error()<<std::dec<<'\n';
        wgc.Stop();
        backdrop.StopCapture();
        SetWindowDisplayAffinity(window,0);
        ImGui::CreateContext(); ImGui::GetIO().Fonts->AddFontDefault(); unsigned char* pixels; int w,h;
        ImGui::GetIO().IniFilename=nullptr;
        ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
        if(!ImGui_ImplDX11_Init(device.Get(),context.Get())) throw std::runtime_error("ImGui DX11 failed");
        mcov::LiquidGlass glass;
        if(!glass.Initialize(device.Get(),context.Get())) throw std::runtime_error("Glass initialization failed");
        const auto runUntilLive=[&] {
        const auto runtimeStart=GetTickCount64();
        while(!glass.liveBackdrop()&&GetTickCount64()-runtimeStart<3000) {
            ImGui::GetIO().DisplaySize={640,480}; ImGui::GetIO().DeltaTime=1.f/60;
            ImGui_ImplDX11_NewFrame(); ImGui::NewFrame();
            glass.BeginFrame(window,{640,480},true);
            ImGui::Begin("Capture probe"); ImGui::TextUnformatted("Capture probe");
            mcov::LiquidGlass::Panel(*ImGui::GetWindowDrawList(),{40,30},{600,450}); ImGui::End();
            ImGui::Render(); glass.RenderTexture();
            Sleep(16);
        }
        return glass.liveBackdrop();
        };
        const bool live=runUntilLive(); std::cout<<"Runtime live="<<live<<" status="<<glass.captureStatus()<<'\n';
        glass.BeginFrame(window,{640,480},false);
        DWORD restored=~DWORD(0);
        const bool paused=!glass.liveBackdrop()&&GetWindowDisplayAffinity(window,&restored)&&restored==0;
        const bool reopened=runUntilLive();
        std::cout<<"Lifecycle paused="<<paused<<" reopened="<<reopened<<" status="<<glass.captureStatus()<<'\n';
        glass.Shutdown(); ImGui_ImplDX11_Shutdown(); ImGui::DestroyContext(); DestroyWindow(window); window=nullptr;
        return live&&paused&&reopened?0:1;
    } catch(const std::exception& e) { if(window) DestroyWindow(window); std::cerr<<e.what()<<'\n'; return 1; }
}

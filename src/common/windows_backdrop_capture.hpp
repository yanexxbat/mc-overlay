#pragma once
#include <memory>
#include <windows.h>

struct ID3D11Device;
namespace Glass { class Backdrop; }
namespace mcov {
// Zweiter Monitor-Capture fuer Hybrid-GPUs und fehlgeschlagene DXGI-Duplizierung.
// Alle Aufrufe erfolgen auf dem UI-Thread; kein Zugriff auf Spiel/JNI.
class WindowsBackdropCapture final {
public:
    WindowsBackdropCapture();
    ~WindowsBackdropCapture();
    WindowsBackdropCapture(const WindowsBackdropCapture&)=delete;
    WindowsBackdropCapture& operator=(const WindowsBackdropCapture&)=delete;
    bool Start(ID3D11Device*,HMONITOR);
    void Stop();
    bool Poll(Glass::Backdrop&);
    bool running() const;
    HRESULT error() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}

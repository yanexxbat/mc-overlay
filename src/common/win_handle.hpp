#pragma once
#include <windows.h>
#include <utility>
#include <string>
#include <cstdint>
namespace mcov {
class WinHandle final {
    HANDLE value_=nullptr;
public:
    explicit WinHandle(HANDLE value=nullptr):value_(value) {}
    ~WinHandle() { if(*this) CloseHandle(value_); }
    WinHandle(const WinHandle&)=delete;
    WinHandle& operator=(const WinHandle&)=delete;
    WinHandle(WinHandle&& other) noexcept:value_(std::exchange(other.value_,nullptr)) {}
    WinHandle& operator=(WinHandle&& other) noexcept { if(this!=&other) { if(*this) CloseHandle(value_); value_=std::exchange(other.value_,nullptr); } return *this; }
    HANDLE get() const { return value_; }
    explicit operator bool() const { return value_&&value_!=INVALID_HANDLE_VALUE; }
};
inline std::wstring OverlayMutexName(std::uint32_t pid) { return L"Local\\mcov_overlay_"+std::to_wstring(pid); }
} // namespace mcov

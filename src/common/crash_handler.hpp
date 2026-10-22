#pragma once
#include <windows.h>
#include <cwchar>

namespace mcov {
inline LONG WINAPI HandleUnhandledException(EXCEPTION_POINTERS* exception) noexcept {
    static volatile LONG handling=0;
    if(InterlockedCompareExchange(&handling,1,0)==0) {
        wchar_t message[256]{};
        const DWORD code=exception&&exception->ExceptionRecord?exception->ExceptionRecord->ExceptionCode:0;
        std::swprintf(message,sizeof(message)/sizeof(message[0]),
            L"Keim encountered an unexpected error and must close.\n\nError code: 0x%08lX\nPlease restart Keim.",
            static_cast<unsigned long>(code));
        MessageBoxW(nullptr,message,L"Keim - Unexpected error",MB_OK|MB_ICONERROR|MB_SETFOREGROUND|MB_TASKMODAL);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}
inline void InstallCrashHandler() noexcept {
    SetUnhandledExceptionFilter(HandleUnhandledException);
}
}

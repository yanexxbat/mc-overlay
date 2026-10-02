#include "log.hpp"

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <mutex>

namespace mcov::log {
namespace {

std::mutex  g_mutex;
std::string g_tag       = "mcov";
HANDLE      g_file      = INVALID_HANDLE_VALUE;
bool        g_console   = false;

const char* LevelName(Level level) {
    switch (level) {
        case Level::Trace: return "TRACE";
        case Level::Info:  return "INFO ";
        case Level::Warn:  return "WARN ";
        case Level::Error: return "ERROR";
    }
    return "?????";
}

} // namespace

void Init(const char* tag, const std::wstring& logFile, bool alsoConsole) {
    std::lock_guard<std::mutex> guard(g_mutex);
    if (tag) g_tag = tag;
    g_console = alsoConsole;

    if (!logFile.empty()) {
        // FILE_SHARE_READ so the file can be tailed while we hold it open.
        g_file = ::CreateFileW(logFile.c_str(),
                               FILE_APPEND_DATA,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               nullptr,
                               OPEN_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL,
                               nullptr);
    }
}

void Shutdown() {
    std::lock_guard<std::mutex> guard(g_mutex);
    if (g_file != INVALID_HANDLE_VALUE) {
        ::CloseHandle(g_file);
        g_file = INVALID_HANDLE_VALUE;
    }
}

void Write(Level level, const char* fmt, ...) {
    char body[2048];
    va_list args;
    va_start(args, fmt);
    ::vsnprintf(body, sizeof(body), fmt, args);
    va_end(args);

    SYSTEMTIME now{};
    ::GetLocalTime(&now);

    char line[2400];
    const int written = ::snprintf(line, sizeof(line),
                                   "[%02u:%02u:%02u.%03u][%s][%s] %s\r\n",
                                   now.wHour, now.wMinute, now.wSecond, now.wMilliseconds,
                                   g_tag.c_str(), LevelName(level), body);
    if (written <= 0) return;

    std::lock_guard<std::mutex> guard(g_mutex);

    ::OutputDebugStringA(line);

    if (g_console) {
        ::fputs(line, level == Level::Error ? stderr : stdout);
    }
    if (g_file != INVALID_HANDLE_VALUE) {
        DWORD ignored = 0;
        ::WriteFile(g_file, line, static_cast<DWORD>(written), &ignored, nullptr);
    }
}

} // namespace mcov::log

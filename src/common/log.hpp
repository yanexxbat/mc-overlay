// log.hpp - tiny logger usable from both binaries.
//
// The agent half runs inside javaw.exe, which has no console attached, so
// printf-style logging silently vanishes there. Every message therefore also
// goes to OutputDebugStringA (visible in DebugView / a debugger) and to a log
// file, which in practice is the only way to diagnose a failed JNI lookup.
#pragma once

#include <string>

namespace mcov::log {

enum class Level { Trace, Info, Warn, Error };

// tag prefixes every line; logFile may be empty to disable file output.
void Init(const char* tag, const std::wstring& logFile, bool alsoConsole);
void Shutdown();

void Write(Level level, const char* fmt, ...);

} // namespace mcov::log

#define MCOV_LOG_TRACE(...) ::mcov::log::Write(::mcov::log::Level::Trace, __VA_ARGS__)
#define MCOV_LOG_INFO(...)  ::mcov::log::Write(::mcov::log::Level::Info,  __VA_ARGS__)
#define MCOV_LOG_WARN(...)  ::mcov::log::Write(::mcov::log::Level::Warn,  __VA_ARGS__)
#define MCOV_LOG_ERROR(...) ::mcov::log::Write(::mcov::log::Level::Error, __VA_ARGS__)

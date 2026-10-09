#pragma once
#include <windows.h>
#include <filesystem>
#include <string>
#include "../overlay/process.hpp"

namespace mcov {
// The portable launcher passes its own folder to the extracted overlay process.
// Runtime/log cache location is deliberately independent from user configs.
inline std::filesystem::path ClientDirectory() {
    const auto size=GetEnvironmentVariableW(L"MCOV_CLIENT_DIR",nullptr,0);
    if(size) { std::wstring value(size,L'\0'); GetEnvironmentVariableW(L"MCOV_CLIENT_DIR",value.data(),size); value.resize(size-1); return value; }
    return ExecutableDirectory();
}
inline std::filesystem::path ClientDataDirectory() {
    const auto size=GetEnvironmentVariableW(L"MCOV_DATA_DIR",nullptr,0);
    if(size) { std::wstring value(size,L'\0'); GetEnvironmentVariableW(L"MCOV_DATA_DIR",value.data(),size); value.resize(size-1); return value; }
    return ExecutableDirectory();
}
} // namespace mcov

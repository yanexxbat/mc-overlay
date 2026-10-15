#pragma once
#include "keyauth.hpp"
#include <windows.h>
#include <filesystem>
#include <string>

namespace mcov::auth {
// The session travels only through an explicitly inherited, single-use pipe.
// Command-line arguments contain a handle number, never a key or session token.
Session ReadHandoff(HANDLE pipe);
bool LaunchOverlay(const std::filesystem::path& executable,const std::filesystem::path& directory,
                   std::wstring command,const Session& session,PROCESS_INFORMATION& process,std::string& error);
}

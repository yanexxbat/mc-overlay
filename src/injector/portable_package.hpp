#pragma once
#include <filesystem>
namespace mcov::launcher {
// Payloads bleiben in einem versionspezifischen Cache, solange die JVM sie lädt.
std::filesystem::path PreparePackage(const std::filesystem::path& storage);
// A live IPC connection can belong to an older build with the same wire format.
// Reuse only when the loaded DLL and both bootstrap/frame helpers match.
bool MatchingAgentRuntime(const std::filesystem::path& expected,const std::filesystem::path& loadedAgent);
}

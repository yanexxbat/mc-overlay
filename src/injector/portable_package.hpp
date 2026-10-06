#pragma once
#include <filesystem>
namespace mcov::launcher {
// Payloads bleiben in einem versionspezifischen Cache, solange die JVM sie lädt.
std::filesystem::path PreparePackage(const std::filesystem::path& storage);
}

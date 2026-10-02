#pragma once
#include <filesystem>
#include <fstream>
#include <string>

namespace mcov {
inline bool IsX64Java(const std::filesystem::path& javaHome) {
    std::ifstream release(javaHome / "release");
    std::string line;
    while (std::getline(release, line)) {
        if (line.starts_with("OS_ARCH="))
            return line.find("\"amd64\"") != std::string::npos ||
                   line.find("\"x86_64\"") != std::string::npos;
    }
    return false;
}
// Read the installed runtime's manifest without starting another JVM.
inline int ReadJavaMajor(const std::filesystem::path& javaHome) {
    std::ifstream release(javaHome / "release");
    std::string line;
    while (std::getline(release, line)) {
        if (!line.starts_with("JAVA_VERSION=")) continue;
        const auto first = line.find_first_of("0123456789", 13);
        if (first == std::string::npos) return 0;
        try {
            int major = std::stoi(line.substr(first));
            if (major == 1) { // legacy manifests: 1.8.0...
                const auto dot = line.find('.', first);
                if (dot == std::string::npos) return 0;
                major = std::stoi(line.substr(dot + 1));
            }
            return major;
        } catch (...) { return 0; }
    }
    return 0;
}
} // namespace mcov

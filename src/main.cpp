// main.cpp - entry point for the external overlay process.
//
// Lifecycle:
//   1. find the running Minecraft JVM and its window
//   2. derive the JDK paths (used to load the agent on the game's own runtime)
//   3. optionally load the agent, if it is not already resident
//   4. run the overlay until the panic key or the game window goes away
//
// The overlay never touches JNI itself - see jvm_interface.hpp for why that
// work has to live inside javaw.exe.
#include <windows.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <thread>

#include "common/log.hpp"
#include "overlay/overlay.hpp"
#include "overlay/process.hpp"

namespace {

bool ParseFlag(int argc, char** argv, const char* flag) {
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == flag) return true;
    }
    return false;
}

void PrintBanner() {
    std::printf(
        "\n"
        "  mcov - external Minecraft overlay\n"
        "  ---------------------------------\n"
        "  INSERT  toggle menu        END  quit\n"
        "\n");
}

} // namespace

int main(int argc, char** argv) {
    const bool skipAutoLoad = ParseFlag(argc, argv, "--no-agent");

    const std::wstring executableDirectory = mcov::ExecutableDirectory();
    mcov::log::Init("overlay", executableDirectory + L"\\mcov_overlay.log", /*alsoConsole=*/true);
    PrintBanner();

    // --- 1. locate the target -------------------------------------------------
    mcov::TargetProcess target;
    std::string         error;

    for (int attempt = 0; attempt < 30; ++attempt) {
        if (mcov::FindMinecraftProcess(target, &error)) break;
        if (attempt == 0) std::printf("  waiting for Minecraft... (%s)\n", error.c_str());
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (!target.valid()) {
        std::fprintf(stderr, "  [!] %s\n", error.c_str());
        MCOV_LOG_ERROR("giving up: %s", error.c_str());
        return 1;
    }

    std::printf("  target : pid %u  (%ls)\n", target.pid, target.windowTitle.c_str());
    std::printf("  runtime: %ls\n", target.javaHome.empty() ? L"<unknown>" : target.javaHome.c_str());

    if (target.jvmDllPath.empty()) {
        // Not fatal: the agent path does not actually need jvm.dll resolved, but
        // it is a strong hint that we mis-identified the process.
        std::printf("  [!] jvm.dll not found under the target's runtime - continuing anyway\n");
    }

    // --- 2. make sure the in-JVM agent is loaded ------------------------------
    // Loading the agent is a visible, one-way action against another process, so
    // it is opt-out here and also available as an explicit button in the menu.
    if (!skipAutoLoad) {
        const std::wstring agentDll = executableDirectory + L"\\mcov_agent.dll";
        const std::wstring toolsDir = executableDirectory + L"\\tools";

        std::uint32_t exitCode = 0;
        std::string   loadError;
        std::printf("  loading agent into the JVM...\n");

        if (mcov::LaunchAgentLoader(target, toolsDir, agentDll, &exitCode, &loadError)) {
            std::printf("  agent loaded\n");
            // Give Agent_OnAttach time to create the shared section before the
            // overlay tries to open it.
            std::this_thread::sleep_for(std::chrono::milliseconds(600));
        } else {
            std::printf("  [!] %s\n", loadError.c_str());
            std::printf("      the overlay will still start; use the Settings tab to retry\n");
            MCOV_LOG_WARN("auto-load failed: %s", loadError.c_str());
        }
    }

    // --- 3. run ---------------------------------------------------------------
    mcov::Overlay overlay;
    if (!overlay.Initialize(target, &error)) {
        std::fprintf(stderr, "  [!] overlay init failed: %s\n", error.c_str());
        MCOV_LOG_ERROR("overlay init failed: %s", error.c_str());
        return 2;
    }

    std::printf("  overlay running - press INSERT for the menu\n\n");

    while (overlay.RunFrame()) {
        // RunFrame blocks on vsync when enabled; this keeps a busy loop from
        // burning a core when it is not.
    }

    overlay.Shutdown();

    MCOV_LOG_INFO("overlay exited cleanly");
    mcov::log::Shutdown();
    std::printf("  bye\n");
    return 0;
}

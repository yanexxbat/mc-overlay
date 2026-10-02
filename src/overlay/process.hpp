// process.hpp - external process discovery and window plumbing.
//
// This is the part of the tool that genuinely works from outside the target:
// finding javaw.exe, locating its window, and deriving the JDK's jvm.dll path so
// the agent can be loaded into it. It does not read Java object state - that is
// not something OpenProcess can meaningfully do (see jvm_interface.hpp).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mcov {

struct TargetProcess {
    std::uint32_t pid    = 0;
    void*         window = nullptr; // HWND of the game's GLFW window

    std::wstring imagePath;   // ...\bin\javaw.exe
    std::wstring javaHome;    // ...\  (parent of bin)
    std::wstring javaExe;     // ...\bin\java.exe - used to run the agent loader
    std::wstring jvmDllPath;  // ...\bin\server\jvm.dll
    std::wstring windowTitle;

    bool valid() const noexcept { return pid != 0 && window != nullptr; }
};

// A running JVM, as listed by the injector.
struct JvmProcess {
    std::uint32_t pid = 0;
    void*         window = nullptr;      // HWND, null for headless JVMs
    std::wstring  imagePath;             // ...\bin\javaw.exe
    std::wstring  javaHome;
    std::wstring  windowTitle;
    bool          looksLikeMinecraft = false;
    bool          alreadyInjected    = false;  // our shared section already exists
};

// Every java.exe / javaw.exe the current user can open, Minecraft first.
std::vector<JvmProcess> EnumerateJvmProcesses();

// RAII wrapper for an OpenProcess handle.
class ProcessHandle {
public:
    ProcessHandle() = default;
    explicit ProcessHandle(std::uint32_t pid, std::uint32_t access);
    ~ProcessHandle();

    ProcessHandle(const ProcessHandle&)            = delete;
    ProcessHandle& operator=(const ProcessHandle&) = delete;
    ProcessHandle(ProcessHandle&& other) noexcept;
    ProcessHandle& operator=(ProcessHandle&& other) noexcept;

    void* get() const noexcept { return handle_; }
    bool  valid() const noexcept { return handle_ != nullptr; }
    void  reset();

private:
    void* handle_ = nullptr;
};

// Scans top-level windows for a visible one owned by javaw.exe/java.exe whose
// title looks like Minecraft. Window-first (rather than process-first) because
// it is the only approach that reliably picks the *game* out of a machine that
// may also be running Gradle daemons, launchers and IDEs on the same JVM binary.
bool FindMinecraftProcess(TargetProcess& out, std::string* error);

// Re-derives javaHome / jvmDllPath from an image path. Exposed for testing and
// for the case where the user points at a JDK by hand.
bool DeriveJavaPaths(TargetProcess& target);

// Client-area rectangle in screen coordinates. This is what the overlay tracks,
// so ESP lines up with the rendered frame rather than the window frame.
bool GetClientRectOnScreen(void* hwnd, int& x, int& y, int& width, int& height);

bool IsWindowAlive(void* hwnd);
bool IsForegroundWindowOwnedBy(std::uint32_t pid);

// ---------------------------------------------------------------------------
// How hard to try when taking the foreground.
//
// Background: Windows refuses SetForegroundWindow from a process that does not
// already own the foreground, unless the per-user foreground lock has expired.
// The lock defaults to 200000 ms, so for an overlay it is effectively always on.
//
// Minecraft only releases the mouse when it loses focus, so the menu needs the
// foreground - but HOW we take it matters enormously:
//
//   AttachInput merges our input queue with Minecraft's render thread. That
//   satisfies the lock, but it drives GLFW's WM_KILLFOCUS -> enableCursor()
//   path across a shared queue, and GLFW is not reentrant there. In practice
//   this crashes the game with an access violation inside glfw.dll whenever the
//   cursor is captured (i.e. in-game, but NOT in menus, which is why it can
//   look like it works). Left available, off by default.
//
//   UnlockForeground instead clears the foreground lock timeout for the moment
//   it takes to call SetForegroundWindow, then restores it. Minecraft then sees
//   an ordinary deactivation - the same thing alt-tab does - and never has its
//   input queue touched. This changes a per-user system preference briefly, so
//   it is opt-in rather than the default.
// ---------------------------------------------------------------------------
enum class FocusMode {
    None = 0,          // never touch the foreground
    Simple,            // plain SetForegroundWindow; safe, often refused
    UnlockForeground,  // briefly clear the foreground lock, then activate normally
    AttachInput,       // legacy AttachThreadInput path - can crash GLFW targets
};

bool ForceForeground(void* hwnd, FocusMode mode);

// Locates a java.exe that can actually perform a JVM attach.
//
// The runtime Minecraft launchers ship is frequently a *JRE* (Modrinth ships
// Zulu 21 JRE), which has neither javac nor the jdk.attach module, so running
// the loader on the game's own runtime fails with NoClassDefFoundError before it
// does anything. Presence of bin/attach.dll is the precise capability signal.
//
// Search order: MCOV_JDK_HOME, JAVA_HOME, the JDK this binary was built against,
// then the target's runtime and installed JDKs. Every candidate must match the
// target's Java major, obtained from its release manifest.
std::wstring FindAttachCapableJava(const TargetProcess& target, std::string* diagnostic);

// Runs the JVM attach helper: java -cp <classDir> AgentLoader <pid> <dll>
// Blocks until the helper exits; returns its exit code through exitCode.
bool LaunchAgentLoader(const TargetProcess& target,
                       const std::wstring&  classDir,
                       const std::wstring&  agentDll,
                       std::uint32_t*       exitCode,
                       std::string*         error);

std::wstring ExecutableDirectory();

} // namespace mcov

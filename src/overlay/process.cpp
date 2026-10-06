#include "process.hpp"
#include "java_runtime.hpp"

#include <windows.h>

#include <tlhelp32.h>

#include <algorithm>
#include <cwctype>
#include <utility>
#include <vector>

#include "../common/ipc.hpp"
#include "../common/log.hpp"

namespace mcov {
namespace {

std::string LastErrorText(const char* what) {
    const DWORD code = ::GetLastError();
    char        buffer[512] = {};
    ::FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                     nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                     buffer, static_cast<DWORD>(sizeof(buffer)), nullptr);
    std::string out = what;
    out += ": ";
    out += buffer;
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
    return out;
}

std::wstring ToLower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });
    return value;
}

std::wstring FileNameOf(const std::wstring& path) {
    const std::size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? path : path.substr(slash + 1);
}

std::wstring DirectoryOf(const std::wstring& path) {
    const std::size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring() : path.substr(0, slash);
}

bool FileExists(const std::wstring& path) {
    const DWORD attributes = ::GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

struct ScanContext {
    TargetProcess best;
    int           bestScore = 0;
    std::uint32_t requiredPid=0;
};

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM param) {
    auto* context = reinterpret_cast<ScanContext*>(param);

    if (!::IsWindowVisible(hwnd)) return TRUE;
    if (::GetWindow(hwnd, GW_OWNER) != nullptr) return TRUE; // skip tool/child windows

    RECT rect{};
    if (!::GetWindowRect(hwnd, &rect)) return TRUE;
    if ((rect.right - rect.left) < 320 || (rect.bottom - rect.top) < 240) return TRUE;

    DWORD pid = 0;
    ::GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0 || pid == ::GetCurrentProcessId()) return TRUE;
    if(context->requiredPid&&pid!=context->requiredPid) return TRUE;

    // PROCESS_QUERY_LIMITED_INFORMATION is the minimum right that still allows
    // QueryFullProcessImageNameW, and crucially it is granted between processes
    // of the same user without elevation - which is exactly our situation.
    ProcessHandle handle(pid, PROCESS_QUERY_LIMITED_INFORMATION);
    if (!handle.valid()) return TRUE;

    wchar_t imagePath[MAX_PATH] = {};
    DWORD   length              = MAX_PATH;
    if (!::QueryFullProcessImageNameW(handle.get(), 0, imagePath, &length)) return TRUE;

    const std::wstring exeName = ToLower(FileNameOf(imagePath));
    if (exeName != L"javaw.exe" && exeName != L"java.exe") return TRUE;

    wchar_t title[512] = {};
    ::GetWindowTextW(hwnd, title, 512);
    const std::wstring titleText  = title;
    const std::wstring titleLower = ToLower(titleText);

    wchar_t className[256] = {};
    ::GetClassNameW(hwnd, className, 256);
    const std::wstring classText = className;

    // Score candidates rather than taking the first hit: a dev machine can
    // easily have several JVMs with windows open.
    int score = 0;
    if (titleLower.find(L"minecraft") != std::wstring::npos) score += 100;
    if (classText.rfind(L"GLFW", 0) == 0) score += 50;   // LWJGL3 registers "GLFW30"
    if (classText == L"LWJGL") score += 40;              // LWJGL2, older versions
    if (titleLower.find(L"launcher") != std::wstring::npos) score -= 80;
    if (score <= 0) return TRUE;

    if (score > context->bestScore) {
        context->bestScore          = score;
        context->best.pid           = pid;
        context->best.window        = hwnd;
        context->best.imagePath     = imagePath;
        context->best.windowTitle   = titleText;
    }
    return TRUE;
}

} // namespace

// ---------------------------------------------------------------------------
// ProcessHandle
// ---------------------------------------------------------------------------
ProcessHandle::ProcessHandle(std::uint32_t pid, std::uint32_t access) {
    handle_ = ::OpenProcess(access, FALSE, pid);
}

ProcessHandle::~ProcessHandle() { reset(); }

ProcessHandle::ProcessHandle(ProcessHandle&& other) noexcept : handle_(other.handle_) {
    other.handle_ = nullptr;
}

ProcessHandle& ProcessHandle::operator=(ProcessHandle&& other) noexcept {
    if (this != &other) {
        reset();
        handle_       = other.handle_;
        other.handle_ = nullptr;
    }
    return *this;
}

void ProcessHandle::reset() {
    if (handle_) {
        ::CloseHandle(handle_);
        handle_ = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Discovery
// ---------------------------------------------------------------------------
namespace {

// Maps PID -> main window title, so the injector can label each JVM with
// something a human recognises instead of a bare number.
struct TitleScan {
    std::vector<std::pair<DWORD, std::pair<HWND, std::wstring>>> found;
};

BOOL CALLBACK CollectTitlesProc(HWND hwnd, LPARAM param) {
    if (!::IsWindowVisible(hwnd)) return TRUE;
    if (::GetWindow(hwnd, GW_OWNER) != nullptr) return TRUE;

    wchar_t title[512] = {};
    if (::GetWindowTextW(hwnd, title, 512) <= 0) return TRUE;

    DWORD pid = 0;
    ::GetWindowThreadProcessId(hwnd, &pid);
    if (!pid) return TRUE;

    reinterpret_cast<TitleScan*>(param)->found.push_back({pid, {hwnd, title}});
    return TRUE;
}

} // namespace

std::vector<JvmProcess> EnumerateJvmProcesses() {
    std::vector<JvmProcess> results;

    TitleScan titles;
    ::EnumWindows(&CollectTitlesProc, reinterpret_cast<LPARAM>(&titles));

    HANDLE snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return results;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    if (::Process32FirstW(snapshot, &entry)) {
        do {
            const std::wstring exeName = ToLower(entry.szExeFile);
            if (exeName != L"javaw.exe" && exeName != L"java.exe") continue;

            // PROCESS_QUERY_LIMITED_INFORMATION succeeds between same-user
            // processes without elevation; a failure here means the JVM is
            // running elevated and we could not attach to it anyway.
            ProcessHandle handle(entry.th32ProcessID, PROCESS_QUERY_LIMITED_INFORMATION);
            if (!handle.valid()) continue;

            wchar_t imagePath[MAX_PATH] = {};
            DWORD   length              = MAX_PATH;
            if (!::QueryFullProcessImageNameW(handle.get(), 0, imagePath, &length)) continue;

            JvmProcess record;
            record.pid       = entry.th32ProcessID;
            record.imagePath = imagePath;
            record.javaHome  = DirectoryOf(DirectoryOf(imagePath));

            for (const auto& [pid, window] : titles.found) {
                if (pid == record.pid) {
                    record.window      = window.first;
                    record.windowTitle = window.second;
                    break;
                }
            }

            const std::wstring lowerTitle = ToLower(record.windowTitle);
            record.looksLikeMinecraft =
                lowerTitle.find(L"minecraft") != std::wstring::npos &&
                lowerTitle.find(L"launcher") == std::wstring::npos;

            // If our shared section already exists for this PID, the agent is in.
            if (HANDLE existing = ::OpenFileMappingW(FILE_MAP_READ, FALSE,
                                                     SharedChannel::MakeName(record.pid).c_str())) {
                record.alreadyInjected = true;
                ::CloseHandle(existing);
            }

            results.push_back(std::move(record));
        } while (::Process32NextW(snapshot, &entry));
    }
    ::CloseHandle(snapshot);

    // Minecraft first, then anything with a window, then the rest.
    std::stable_sort(results.begin(), results.end(), [](const JvmProcess& a, const JvmProcess& b) {
        if (a.looksLikeMinecraft != b.looksLikeMinecraft) return a.looksLikeMinecraft;
        return (a.window != nullptr) && (b.window == nullptr);
    });
    return results;
}

bool DeriveJavaPaths(TargetProcess& target) {
    if (target.imagePath.empty()) return false;

    const std::wstring binDir = DirectoryOf(target.imagePath);  // ...\bin
    target.javaHome           = DirectoryOf(binDir);            // ...\ (JDK/JRE root)
    target.javaExe            = binDir + L"\\java.exe";

    // Modern JDK layout puts the HotSpot VM at <home>\bin\server\jvm.dll.
    // The client VM only ever existed on 32-bit, and the old JRE layout
    // (<home>\jre\bin\server) disappeared in JDK 9 - both kept as fallbacks
    // because a mismatched launcher can still land you there.
    const std::wstring candidates[] = {
        binDir + L"\\server\\jvm.dll",
        binDir + L"\\client\\jvm.dll",
        target.javaHome + L"\\jre\\bin\\server\\jvm.dll",
        target.javaHome + L"\\lib\\server\\jvm.dll",
    };

    for (const auto& candidate : candidates) {
        if (FileExists(candidate)) {
            target.jvmDllPath = candidate;
            return true;
        }
    }

    MCOV_LOG_WARN("no jvm.dll found under %S", target.javaHome.c_str());
    return false;
}

bool FindMinecraftProcess(TargetProcess& out, std::string* error,std::uint32_t requiredPid) {
    ScanContext context;
    context.requiredPid=requiredPid;
    ::EnumWindows(&EnumWindowsProc, reinterpret_cast<LPARAM>(&context));

    if (context.bestScore == 0) {
        if (error) {
            *error = "no Minecraft window found. Make sure the game is running and past "
                     "the launcher (the launcher itself is deliberately ignored).";
        }
        return false;
    }

    out = context.best;
    DeriveJavaPaths(out);

    MCOV_LOG_INFO("target: pid=%u hwnd=%p title=\"%S\"",
                  out.pid, out.window, out.windowTitle.c_str());
    MCOV_LOG_INFO("  image : %S", out.imagePath.c_str());
    MCOV_LOG_INFO("  jvm   : %S", out.jvmDllPath.empty() ? L"<not found>" : out.jvmDllPath.c_str());

    // Sanity check that we can actually open the process. If this fails the
    // target is almost certainly running elevated while we are not.
    ProcessHandle probe(out.pid, PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE);
    if (!probe.valid()) {
        if (error) {
            *error = LastErrorText("OpenProcess on the target failed") +
                     " - if Minecraft is running as administrator, this overlay must be too.";
        }
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Window helpers
// ---------------------------------------------------------------------------
bool GetClientRectOnScreen(void* hwnd, int& x, int& y, int& width, int& height) {
    auto window = static_cast<HWND>(hwnd);
    if (!window || !::IsWindow(window)) return false;

    RECT client{};
    if (!::GetClientRect(window, &client)) return false;

    // GetClientRect is window-relative; map its corners into screen space so we
    // cover the rendered area exactly, borders and title bar excluded.
    POINT topLeft{client.left, client.top};
    POINT bottomRight{client.right, client.bottom};
    if (!::ClientToScreen(window, &topLeft) || !::ClientToScreen(window, &bottomRight)) return false;

    x      = topLeft.x;
    y      = topLeft.y;
    width  = bottomRight.x - topLeft.x;
    height = bottomRight.y - topLeft.y;
    return width > 0 && height > 0;
}

bool IsWindowAlive(void* hwnd) {
    auto window = static_cast<HWND>(hwnd);
    return window && ::IsWindow(window) != FALSE;
}

bool IsForegroundWindowOwnedBy(std::uint32_t pid) {
    HWND foreground = ::GetForegroundWindow();
    if (!foreground) return false;
    DWORD owner = 0;
    ::GetWindowThreadProcessId(foreground, &owner);
    return owner == pid;
}

bool ForceForeground(void* hwnd, FocusMode mode) {
    auto window = static_cast<HWND>(hwnd);
    if (!window || !::IsWindow(window)) return false;
    if (mode == FocusMode::None) return false;

    HWND foreground = ::GetForegroundWindow();
    if (foreground == window) return true;

    switch (mode) {
        case FocusMode::None:
            return false;

        case FocusMode::Simple: {
            // The safe option: an ordinary activation request, identical to what
            // happens when the user clicks another window. Never touches the
            // target's input queue. Returns FALSE when the foreground lock
            // refuses it, in which case Windows just flashes our taskbar button.
            const bool ok = ::SetForegroundWindow(window) != FALSE;
            if (!ok) {
                MCOV_LOG_WARN("SetForegroundWindow refused (foreground lock active). "
                              "The menu will draw but cannot receive input; switch focus "
                              "mode to 'Unlock foreground' in Settings.");
            }
            return ok;
        }

        case FocusMode::UnlockForeground: {
            // Clear the foreground lock for the duration of one activation, then
            // put it back exactly as we found it. Minecraft sees a completely
            // ordinary deactivation, so GLFW's cursor-release path runs on its
            // own thread the way it does for alt-tab.
            DWORD previousTimeout = 0;
            const bool queried = ::SystemParametersInfoW(
                SPI_GETFOREGROUNDLOCKTIMEOUT, 0, &previousTimeout, 0) != FALSE;

            if (queried) {
                ::SystemParametersInfoW(SPI_SETFOREGROUNDLOCKTIMEOUT, 0,
                                        reinterpret_cast<void*>(0), SPIF_SENDCHANGE);
            }

            const bool ok = ::SetForegroundWindow(window) != FALSE;

            if (queried) {
                ::SystemParametersInfoW(SPI_SETFOREGROUNDLOCKTIMEOUT, 0,
                                        reinterpret_cast<void*>(static_cast<UINT_PTR>(previousTimeout)),
                                        SPIF_SENDCHANGE);
            }
            if (!ok) MCOV_LOG_WARN("SetForegroundWindow still refused after clearing the lock");
            return ok;
        }

        case FocusMode::AttachInput: {
            // DANGEROUS - retained only because it is the classic technique and
            // some targets tolerate it. Merging input queues makes focus changes
            // reentrant inside the target's message pump; against a GLFW app with
            // the cursor captured this reliably faults in glfw.dll. See the
            // FocusMode comment in process.hpp.
            const DWORD foregroundThread = ::GetWindowThreadProcessId(foreground, nullptr);
            const DWORD selfThread       = ::GetCurrentThreadId();
            const bool  attached         = foregroundThread != selfThread &&
                                           ::AttachThreadInput(foregroundThread, selfThread, TRUE) != FALSE;

            // Deliberately no BringWindowToTop (redundant when topmost) and no
            // SetFocus: with queues merged, SetFocus yanks focus cross-thread and
            // is the single most likely trigger of the GLFW fault.
            const bool ok = ::SetForegroundWindow(window) != FALSE;

            if (attached) ::AttachThreadInput(foregroundThread, selfThread, FALSE);
            return ok;
        }
    }
    return false;
}

std::wstring ExecutableDirectory() {
    wchar_t path[MAX_PATH] = {};
    if (!::GetModuleFileNameW(nullptr, path, MAX_PATH)) return {};
    return DirectoryOf(path);
}

std::wstring FindAttachCapableJava(const TargetProcess& target, std::string* diagnostic) {
    int targetMajor = ReadJavaMajor(target.javaHome);
    if (!targetMajor && std::filesystem::path(target.javaHome).filename() == L"jre")
        targetMajor = ReadJavaMajor(std::filesystem::path(target.javaHome).parent_path());
    if (targetMajor < 17) {
        if (diagnostic) *diagnostic = "cannot identify Java 17+ from the target runtime's release manifest";
        return {};
    }
    struct Candidate {
        std::wstring home;
        const char*  origin;
    };
    std::vector<Candidate> candidates;

    wchar_t buffer[MAX_PATH] = {};
    if (const DWORD n = ::GetEnvironmentVariableW(L"MCOV_JDK_HOME", buffer, MAX_PATH); n > 0 && n < MAX_PATH) {
        candidates.push_back({buffer, "MCOV_JDK_HOME"});
    }
    if (const DWORD n = ::GetEnvironmentVariableW(L"JAVA_HOME", buffer, MAX_PATH); n > 0 && n < MAX_PATH) {
        candidates.push_back({buffer, "JAVA_HOME"});
    }
#ifdef MCOV_BUILD_JDK_HOME
    // Baked in by CMake so a working JDK is available even with no environment
    // set up - this is the one that normally wins on a dev machine.
#define MCOV_WIDEN_INNER(x) L##x
#define MCOV_WIDEN(x) MCOV_WIDEN_INNER(x)
    candidates.push_back({MCOV_WIDEN(MCOV_BUILD_JDK_HOME), "the JDK this overlay was built against"});
#endif
    if (!target.javaHome.empty()) {
        candidates.push_back({target.javaHome, "the game's own runtime"});
    }
    // Discover all installed majors; a build JDK 21 must never win for Java 25.
    for (const auto* root : {L"C:\\Program Files\\Eclipse Adoptium", L"C:\\Program Files\\Java",
                             L"C:\\Program Files\\Microsoft", L"C:\\Program Files\\Zulu"}) {
        std::error_code ec;
        std::filesystem::directory_iterator entries(root, ec), end;
        while (!ec && entries != end) {
            if (entries->is_directory(ec)) candidates.push_back({entries->path().wstring(), "an installed JDK"});
            entries.increment(ec);
        }
    }

    std::string tried;
    for (const auto& candidate : candidates) {
        if (candidate.home.empty()) continue;

        const std::wstring exe = candidate.home + L"\\bin\\java.exe";
        if (!tried.empty()) tried += "; ";
        tried += candidate.origin;

        if (!FileExists(exe)) { tried += " (no java.exe)"; continue; }
        const int candidateMajor = ReadJavaMajor(candidate.home);
        if (candidateMajor != targetMajor) {
            tried += " (Java " + std::to_string(candidateMajor) + ", target " + std::to_string(targetMajor) + ")";
            continue;
        }
        if (!IsX64Java(candidate.home)) { tried += " (not an x64 JDK)"; continue; }

        // attach.dll is shipped by JDKs and omitted from JREs, and its presence
        // tracks the jdk.attach module exactly. Checking for it beats checking
        // for javac.exe, which is only a proxy.
        if (!FileExists(candidate.home + L"\\bin\\attach.dll")) {
            tried += " (JRE - no attach support)";
            continue;
        }

        MCOV_LOG_INFO("attach helper will run on %s: %S", candidate.origin, exe.c_str());
        return exe;
    }

    if (diagnostic) {
        *diagnostic = "no JDK with attach support found. Tried: " + tried +
                      ". Minecraft launchers usually ship a JRE, which cannot attach; "
                      "install a 64-bit JDK " + std::to_string(targetMajor) +
                      " and set MCOV_JDK_HOME or JAVA_HOME.";
    }
    return {};
}

bool LaunchAgentLoader(const TargetProcess& target,
                       const std::wstring&  classDir,
                       const std::wstring&  agentDll,
                       std::uint32_t*       exitCode,
                       std::string*         error,std::stop_token stop) {
    if (!FileExists(agentDll)) {
        if (error) *error = "mcov_agent.dll not found; build the agent target first";
        return false;
    }

    // The helper needs the jdk.attach module, which the target's runtime very
    // often lacks. It does NOT need to be the same JDK as the target: the attach
    // handshake is HotSpot's and works across vendors, so a Temurin 21 helper
    // attaches happily to a Zulu 21 game. Matching the MAJOR version is what
    // matters.
    std::string  discovery;
    std::wstring javaExe = FindAttachCapableJava(target, &discovery);
    if (javaExe.empty()) {
        if (error) *error = discovery;
        return false;
    }

    std::wstring commandLine;
    commandLine += L"\"" + javaExe + L"\"";
    commandLine += L" -cp \"" + classDir + L"\"";
    commandLine += L" AgentLoader ";
    commandLine += std::to_wstring(target.pid);
    commandLine += L" \"" + agentDll + L"\"";

    MCOV_LOG_INFO("launching attach helper: %S", commandLine.c_str());

    // Capture the helper's stdout+stderr. Without this, a failure surfaces as a
    // bare exit code and the actual cause (NoClassDefFoundError, a rejected
    // attach, a bitness mismatch) is thrown away - which is exactly what made
    // the first round of "auto-load failed" reports useless.
    wchar_t tempDir[MAX_PATH] = {};
    wchar_t tempFile[MAX_PATH] = {};
    ::GetTempPathW(MAX_PATH, tempDir);
    ::GetTempFileNameW(tempDir, L"mcov", 0, tempFile);

    SECURITY_ATTRIBUTES inheritable{};
    inheritable.nLength        = sizeof(inheritable);
    inheritable.bInheritHandle = TRUE;

    HANDLE capture = ::CreateFileW(tempFile, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   &inheritable, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);

    STARTUPINFOW        startup{};
    PROCESS_INFORMATION info{};
    startup.cb          = sizeof(startup);
    startup.dwFlags     = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;

    if (capture != INVALID_HANDLE_VALUE) {
        startup.dwFlags   |= STARTF_USESTDHANDLES;
        startup.hStdOutput = capture;
        startup.hStdError  = capture;
        startup.hStdInput  = nullptr;
    }

    // CreateProcessW mutates its command-line argument, so it needs a writable buffer.
    std::vector<wchar_t> mutableCommandLine(commandLine.begin(), commandLine.end());
    mutableCommandLine.push_back(L'\0');

    // bInheritHandles must be TRUE for the child to receive the capture handle.
    const BOOL started = ::CreateProcessW(nullptr, mutableCommandLine.data(), nullptr, nullptr,
                                          capture != INVALID_HANDLE_VALUE ? TRUE : FALSE,
                                          CREATE_NO_WINDOW, nullptr, classDir.c_str(), &startup, &info);
    if (!started) {
        if (error) *error = LastErrorText("CreateProcessW for the attach helper");
        if (capture != INVALID_HANDLE_VALUE) ::CloseHandle(capture);
        ::DeleteFileW(tempFile);
        return false;
    }

    const auto deadline=GetTickCount64()+30000;
    bool interrupted=false;
    while(::WaitForSingleObject(info.hProcess,100)==WAIT_TIMEOUT) {
        if(stop.stop_requested()||GetTickCount64()>=deadline) {
            // Nur unseren eigenen Helper beenden; niemals die Minecraft-JVM.
            interrupted=true; ::TerminateProcess(info.hProcess,ERROR_CANCELLED);
            ::WaitForSingleObject(info.hProcess,1000); break;
        }
    }

    DWORD code = 0;
    ::GetExitCodeProcess(info.hProcess, &code);
    if(interrupted) code=ERROR_CANCELLED;
    if (exitCode) *exitCode = code;

    ::CloseHandle(info.hThread);
    ::CloseHandle(info.hProcess);
    if (capture != INVALID_HANDLE_VALUE) ::CloseHandle(capture);

    // Read back whatever the helper said and mirror it into our log.
    std::string output;
    if (HANDLE readBack = ::CreateFileW(tempFile, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        readBack != INVALID_HANDLE_VALUE) {
        char  chunk[1024];
        DWORD read = 0;
        while (::ReadFile(readBack, chunk, sizeof(chunk), &read, nullptr) && read > 0) {
            output.append(chunk, read);
        }
        ::CloseHandle(readBack);
    }
    ::DeleteFileW(tempFile);

    if (!output.empty()) {
        std::size_t start = 0;
        while (start < output.size()) {
            std::size_t end = output.find('\n', start);
            if (end == std::string::npos) end = output.size();
            std::string line = output.substr(start, end - start);
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
            if (!line.empty()) MCOV_LOG_INFO("  [helper] %s", line.c_str());
            start = end + 1;
        }
    }

    if (code != 0) {
        if (error) {
            *error = "attach helper failed (exit " + std::to_string(code) + ")";
            // Surface the helper's own first complaint, which is the actionable bit.
            const std::size_t marker = output.find("[loader]");
            if (marker != std::string::npos) {
                std::size_t end = output.find('\n', marker);
                if (end == std::string::npos) end = output.size();
                std::string detail = output.substr(marker, end - marker);
                while (!detail.empty() && (detail.back() == '\r')) detail.pop_back();
                *error += ": " + detail;
            } else if (!output.empty()) {
                *error += ": " + output.substr(0, 200);
            }
        }
        return false;
    }
    return true;
}

} // namespace mcov

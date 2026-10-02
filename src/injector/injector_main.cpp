// injector_main.cpp - standalone flat GUI for loading the agent into a JVM.
//
// The overlay can already load the agent on startup; this exists for the cases
// where that is not what you want: picking a specific JVM out of several,
// attaching to an already-running session, seeing exactly why an attach failed,
// or pointing at a different agent DLL.
//
// It performs the same JVMTI attach the overlay does - VirtualMachine.attach()
// via AgentLoader - not code injection. The name "injector" is the familiar one,
// but nothing here writes into another process's memory.
#include "imgui.h"

#include <windows.h>

#include <string>
#include <vector>

#include "../common/flat_app.hpp"
#include "../common/flat_style.hpp"
#include "../common/ipc.hpp"
#include "../common/log.hpp"
#include "../overlay/process.hpp"

using namespace mcov;

namespace {

std::string Narrow(const std::wstring& wide) {
    if (wide.empty()) return {};
    const int size = ::WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()),
                                           nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string out(static_cast<std::size_t>(size), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()),
                          out.data(), size, nullptr, nullptr);
    return out;
}

// Shortens a long path to "...\tail" so it occupies one line instead of three
// and cannot squeeze the rest of the layout off the window.
std::string Elide(const std::string& text, std::size_t maxLen = 58) {
    if (text.size() <= maxLen) return text;
    return "..." + text.substr(text.size() - (maxLen - 3));
}

struct AppState {
    std::vector<JvmProcess>  processes;
    int                      selected = -1;
    std::vector<std::string> log;
    std::wstring             agentDll;
    std::wstring             toolsDir;
    std::wstring             attachJava;
    std::string              attachDiagnostic;

    // Last result, shown as a banner. The log alone was not enough: it lived at
    // the bottom of the window where it could be pushed out of view entirely,
    // so a successful attach looked like nothing had happened.
    std::string status;
    bool        statusIsError = false;

    void Log(std::string line) {
        // Mirror into the file log too, so a failure is still diagnosable after
        // the window is closed.
        MCOV_LOG_INFO("%s", line.c_str());
        log.push_back(std::move(line));
        if (log.size() > 200) log.erase(log.begin());
    }

    // Reader onto the injected agent's shared block, so the injector can show
    // that the agent is actually alive rather than just "the attach returned 0".
    SharedChannel probe;
    std::uint32_t probePid = 0;

    void SyncProbe() {
        if (selected < 0 || selected >= static_cast<int>(processes.size())) return;
        const std::uint32_t pid = processes[selected].pid;
        if (pid == probePid && probe.valid()) return;
        probe.Close();
        probePid = probe.OpenReader(pid, nullptr) ? pid : 0;
    }

    void Refresh() {
        const std::uint32_t previous = (selected >= 0 && selected < static_cast<int>(processes.size()))
                                           ? processes[selected].pid : 0;
        processes = EnumerateJvmProcesses();
        selected  = processes.empty() ? -1 : 0;
        for (int i = 0; i < static_cast<int>(processes.size()); ++i) {
            if (processes[i].pid == previous) { selected = i; break; }
        }
    }
};

const char* AgentStatusText(AgentStatus status) {
    switch (status) {
        case AgentStatus::Booting:           return "booting";
        case AgentStatus::ResolvingMappings: return "resolving mappings";
        case AgentStatus::Running:           return "running";
        case AgentStatus::NoClientInstance:  return "no world loaded";
        case AgentStatus::MappingsFailed:    return "mapping lookup failed";
        case AgentStatus::Detaching:         return "detaching";
        case AgentStatus::WaitingForFrame:   return "waiting for camera frame";
    }
    return "unknown";
}

// Starts the overlay. --no-agent because whatever we are looking at is already
// injected; without it the overlay would attach a second time on startup.
bool LaunchOverlay(const std::wstring& exeDir, std::string* error) {
    const std::wstring exe = exeDir + L"\\mcov_overlay.exe";
    if (::GetFileAttributesW(exe.c_str()) == INVALID_FILE_ATTRIBUTES) {
        if (error) *error = "mcov_overlay.exe not found next to the injector";
        return false;
    }

    std::wstring commandLine = L"\"" + exe + L"\" --no-agent";
    std::vector<wchar_t> mutableCommandLine(commandLine.begin(), commandLine.end());
    mutableCommandLine.push_back(L'\0');

    STARTUPINFOW        startup{};
    PROCESS_INFORMATION info{};
    startup.cb = sizeof(startup);

    if (!::CreateProcessW(nullptr, mutableCommandLine.data(), nullptr, nullptr, FALSE,
                          0, nullptr, exeDir.c_str(), &startup, &info)) {
        if (error) *error = "CreateProcessW for the overlay failed";
        return false;
    }
    ::CloseHandle(info.hThread);
    ::CloseHandle(info.hProcess);
    return true;
}

void DrawTitleBar(FlatApp& app, AppState& state) {
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const float w    = static_cast<float>(app.width());

    draw->AddText(ImVec2(32, 29), style::kText, "mcov  agent injector");

    const char* hint = "drag here to move";
    const ImVec2 hintSize = ImGui::CalcTextSize(hint);
    draw->AddText(ImVec2(w * 0.5f - hintSize.x * 0.5f, 29), style::kMuted, hint);

    // Close button. Drawn on a draw list rather than as an ImGui widget, so it
    // has to be excluded from the caption hit-test by hand - otherwise Windows
    // treats the click as the start of a window drag and it never reaches us.
    const ImVec2 closeMin(w - 56.0f, 26.0f);
    const ImVec2 closeMax(w - 32.0f, 50.0f);
    app.SetNoDragRect(closeMin.x, closeMin.y, closeMax.x, closeMax.y);

    const ImVec2 mouse   = ImGui::GetIO().MousePos;
    const bool   hovered = mouse.x >= closeMin.x && mouse.x <= closeMax.x &&
                           mouse.y >= closeMin.y && mouse.y <= closeMax.y;

    draw->AddRectFilled(closeMin, closeMax,
                        hovered ? style::kHover : style::kPanel, 6.0f);

    // Draw the glyph as two strokes; the bundled font has no reliable ASCII 'X'
    // at this size once Inter is the default.
    const float pad = 8.0f;
    const ImU32 ink = style::kText;
    draw->AddLine(ImVec2(closeMin.x + pad, closeMin.y + pad),
                  ImVec2(closeMax.x - pad, closeMax.y - pad), ink, 1.6f);
    draw->AddLine(ImVec2(closeMax.x - pad, closeMin.y + pad),
                  ImVec2(closeMin.x + pad, closeMax.y - pad), ink, 1.6f);

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) app.RequestClose();
}

void DoInject(AppState& state) {
    if (state.selected < 0 || state.selected >= static_cast<int>(state.processes.size())) return;
    const JvmProcess& target = state.processes[state.selected];

    TargetProcess t;
    t.pid       = target.pid;
    t.window    = target.window;
    t.imagePath = target.imagePath;
    t.javaHome  = target.javaHome;
    t.javaExe   = target.javaHome + L"\\bin\\java.exe";
    DeriveJavaPaths(t);

    state.Log("Attaching to pid " + std::to_string(target.pid) + " ...");

    std::uint32_t exitCode = 0;
    std::string   error;
    if (LaunchAgentLoader(t, state.toolsDir, state.agentDll, &exitCode, &error)) {
        state.Log("OK - Agent_OnAttach returned successfully.");
        state.status        = "Injected into pid " + std::to_string(target.pid);
        state.statusIsError = false;
    } else {
        state.Log("FAILED: " + error);
        state.status        = error;
        state.statusIsError = true;
    }
    state.Refresh();
}

} // namespace

int main() {
    const std::wstring exeDir = ExecutableDirectory();
    mcov::log::Init("injector", exeDir + L"\\mcov_injector.log", /*alsoConsole=*/true);

    AppState state;
    state.agentDll = exeDir + L"\\mcov_agent.dll";
    state.toolsDir = exeDir + L"\\tools";
    state.Refresh();

    // Resolve the attach-capable JDK once so the UI can report it up front
    // rather than only failing at attach time.
    TargetProcess probe;
    if (!state.processes.empty()) {
        probe.javaHome = state.processes[0].javaHome;
        probe.pid      = state.processes[0].pid;
    }
    state.attachJava = FindAttachCapableJava(probe, &state.attachDiagnostic);

    FlatApp app;
    std::string error;
    if (!app.Initialize(L"mcov injector", 720, 560, &error)) {
        MCOV_LOG_ERROR("init failed: %s", error.c_str());
        return 1;
    }

    app.Run([&] {
        const float w = static_cast<float>(app.width());
        const float h = static_cast<float>(app.height());

        // Opaque shell; the transparent margin leaves room for the shadow.
        app.Panel(16.0f, 16.0f, w - 16.0f, h - 16.0f);
        DrawTitleBar(app, state);

        ImGui::SetNextWindowPos(ImVec2(16, static_cast<float>(FlatApp::kTitleBarHeight)));
        ImGui::SetNextWindowSize(ImVec2(w - 32, h - FlatApp::kTitleBarHeight - 16));
        ImGui::Begin("##injector", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings);

        // --- attach runtime -------------------------------------------------
        if (state.attachJava.empty()) {
            style::StyleScope warning;
            warning.Color(ImGuiCol_Text, ImVec4(0.95f, 0.5f, 0.4f, 1.0f));
            ImGui::TextWrapped("No JDK with attach support found.");
            ImGui::TextWrapped("%s", state.attachDiagnostic.c_str());
        } else {
            ImGui::TextDisabled("Attach runtime");
            const std::string jdk = Narrow(state.attachJava);
            ImGui::TextUnformatted(Elide(jdk).c_str());
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", jdk.c_str());
        }
        ImGui::Separator();

        // --- process list ---------------------------------------------------
        ImGui::TextDisabled("Running JVMs");
        ImGui::SameLine(ImGui::GetWindowWidth() - 110.0f);
        if (ImGui::Button("Refresh", ImVec2(90, 0))) state.Refresh();

        {
            style::FlatPanel panel("##procs", ImVec2(0, 182));
            if (panel.visible()) {
                if (state.processes.empty()) {
                    ImGui::TextDisabled("No java.exe / javaw.exe running as this user.");
                }
                for (int i = 0; i < static_cast<int>(state.processes.size()); ++i) {
                    const JvmProcess& p = state.processes[i];

                    char label[320];
                    std::snprintf(label, sizeof(label), "%s  pid %u##p%d",
                                  p.windowTitle.empty() ? "(no window)" : Narrow(p.windowTitle).c_str(),
                                  p.pid, i);

                    if (ImGui::Selectable(label, state.selected == i)) state.selected = i;

                    ImGui::SameLine();
                    if (p.alreadyInjected) {
                        ImGui::TextColored(ImVec4(0.40f, 0.85f, 0.50f, 1.0f), "[injected]");
                    } else if (p.looksLikeMinecraft) {
                        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.00f, 1.0f), "[minecraft]");
                    } else {
                        ImGui::TextDisabled("[jvm]");
                    }
                }
            }
        }

        // --- selection detail ------------------------------------------------
        if (state.selected >= 0 && state.selected < static_cast<int>(state.processes.size())) {
            const JvmProcess& p = state.processes[state.selected];
            const std::string image = Narrow(p.imagePath);
            ImGui::TextDisabled("Runtime");
            ImGui::TextUnformatted(Elide(image).c_str());
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", image.c_str());

            const bool isJdk = ::GetFileAttributesW((p.javaHome + L"\\bin\\attach.dll").c_str())
                               != INVALID_FILE_ATTRIBUTES;
            if (!isJdk) ImGui::TextDisabled("JRE - helper runs on the JDK above (expected).");
        }

        ImGui::Separator();
        ImGui::TextDisabled("Agent");
        const std::string agent = Narrow(state.agentDll);
        ImGui::TextUnformatted(Elide(agent).c_str());
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", agent.c_str());

        const bool canInject = state.selected >= 0 && !state.attachJava.empty();
        ImGui::BeginDisabled(!canInject);
        if (style::AccentButton("Inject agent", ImVec2(-1, 38))) DoInject(state);
        ImGui::EndDisabled();

        // Result banner, right under the button where it cannot be missed.
        if (!state.status.empty()) {
            style::StyleScope banner;
            banner.Color(ImGuiCol_Text,
                                  state.statusIsError ? ImVec4(0.96f, 0.48f, 0.40f, 1.0f)
                                                      : ImVec4(0.40f, 0.88f, 0.52f, 1.0f));
            ImGui::TextWrapped("%s", state.status.c_str());
        }

        // --- live agent state -------------------------------------------------
        // The agent draws nothing by itself: it samples the client and writes to
        // shared memory. Reading that back here is the only way to show, in the
        // injector, that a successful attach actually did something.
        state.SyncProbe();
        if (const SharedBlock* block = state.probe.block()) {
            const auto status    = static_cast<AgentStatus>(block->status.load(std::memory_order_acquire));
            const auto heartbeat = block->heartbeat.load(std::memory_order_acquire);

            ImGui::Separator();
            ImGui::TextDisabled("Agent state");
            ImGui::TextColored(ImVec4(0.40f, 0.88f, 0.52f, 1.0f), "%s", AgentStatusText(status));
            ImGui::SameLine();
            ImGui::TextDisabled("| heartbeat %u", heartbeat);

            WorldSnapshot snapshot{};
            if (TryReadSnapshot(block, snapshot) && snapshot.frameIndex > 0) {
                ImGui::Text("%.1f  %.1f  %.1f    %u entities",
                            snapshot.posX, snapshot.posY, snapshot.posZ, snapshot.entityCount);
            }
        } else if (!state.status.empty() && !state.statusIsError) {
            ImGui::TextDisabled("Agent attached but no shared block yet - give it a moment.");
        }

        // The agent is invisible on its own; the overlay is what draws.
        if (ImGui::Button("Launch overlay", ImVec2(-1, 32))) {
            std::string launchError;
            if (LaunchOverlay(exeDir, &launchError)) {
                state.Log("Overlay started (--no-agent).");
                state.status        = "Overlay launched - press INSERT in Minecraft";
                state.statusIsError = false;
            } else {
                state.Log("FAILED: " + launchError);
                state.status        = launchError;
                state.statusIsError = true;
            }
        }
        ImGui::TextDisabled("Injecting only loads the agent, which draws nothing.\n"
                            "The overlay is the process that renders ESP and HUD.");

        // --- log --------------------------------------------------------------
        // Fixed height: with ImVec2(0,0) this child took whatever was left, and
        // the content above could squeeze it to nothing - which made a
        // successful attach look like a dead button.
        ImGui::TextDisabled("Log");
        {
            style::FlatPanel panel("##log", ImVec2(0, 162));
            if (panel.visible()) {
                for (const std::string& line : state.log) {
                    const bool bad = line.rfind("FAILED", 0) == 0;
                    style::StyleScope lineStyle;
                    if (bad) lineStyle.Color(ImGuiCol_Text, ImVec4(0.95f, 0.5f, 0.4f, 1.0f));
                    ImGui::TextWrapped("%s", line.c_str());
                }
                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f) ImGui::SetScrollHereY(1.0f);
            }
        }

        ImGui::End();
    });

    app.Shutdown();
    mcov::log::Shutdown();
    return 0;
}

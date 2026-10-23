#pragma once
#include "imgui.h"
#include <string>
#include <vector>
#include <cstdint>
#include <array>
#include "anim.h"
namespace mcov::launcher {
// Worker publishes this status; View itself is accessed only by the UI thread.
enum class VerificationStatus { Idle, Verifying, Success, Error };
struct Instance { std::uint32_t pid{}; std::string title,java; bool connected=false,running=false; };
struct View {
    // Presentation only; lifetime follows this launcher, never persisted.
    bool introReady=false,animations=true;
    anim::State motion;
    std::vector<Instance> instances;
    int selected=-1,tab=0;
    bool busy=false,error=false,needsJdk=false;
    bool licensed=false,authBusy=false,authError=false,showLicense=false;
    std::array<char,257> licenseKey{};
    std::string hardwareId;
    std::string authMessage="Enter your license key to unlock the client.";
    std::string status="Start Minecraft and select an instance.",phase,diagnostic,log;
};
struct Actions { bool start=false,refresh=false,close=false,minimize=false,openLogs=false,chooseJdk=false,verifyLicense=false; };
// Darstellung ohne Prozesszugriff. Tests zeichnen genau dieselben Controls.
Actions Render(View& view,ImVec2 size);
}

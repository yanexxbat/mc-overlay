#pragma once
#include "imgui.h"
#include <string>
#include <vector>
#include <cstdint>
namespace mcov::launcher {
struct Instance { std::uint32_t pid{}; std::string title,java; bool connected=false,running=false; };
struct View {
    std::vector<Instance> instances;
    int selected=-1,tab=0;
    bool busy=false,error=false,needsJdk=false;
    std::string status="Minecraft starten und eine Instanz auswaehlen.",phase,diagnostic,log;
};
struct Actions { bool start=false,refresh=false,close=false,minimize=false,openLogs=false,chooseJdk=false; };
// Darstellung ohne Prozesszugriff. Tests zeichnen genau dieselben Controls.
Actions Render(View& view,ImVec2 size);
}

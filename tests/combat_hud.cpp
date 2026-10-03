#include "combat/debug_hud.hpp"
#include "imgui.h"
#include <cmath>
#include <cstdio>

int main() {
    using namespace mcov::combat;
    ImGui::CreateContext();
    auto& io=ImGui::GetIO(); io.IniFilename=nullptr; io.LogFilename=nullptr;
    unsigned char* pixels; int width,height; io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height); io.Fonts->SetTexID(1);
    DebugSnapshot snapshot; snapshot.enabled=true; snapshot.target=Target{}; snapshot.target->id=1;
    snapshot.target->name.fill('W'); snapshot.target->range=2.75; snapshot.fovRadiusPx=300; snapshot.traceCount=kTraceCapacity;
    for(std::size_t i=0;i<kTraceCapacity;++i) snapshot.trace[i]={static_cast<double>(i),{179+static_cast<double>(i),20},{-179+static_cast<double>(i),10}};
    bool valid=true;
    const auto& style=ImGui::GetStyle(); const float alpha=style.Alpha;
    for(const ImVec2 viewport:{ImVec2(1920,1080),ImVec2(480,360),ImVec2(80,80)}) {
        io.DisplaySize=viewport; io.DeltaTime=1.0f/60;
        ImGui::NewFrame(); const auto stack=ImGui::GetBackgroundDrawList()->_ClipRectStack.Size;
        CombatHud::Draw(*ImGui::GetBackgroundDrawList(),snapshot,HudConfig{},viewport);
        valid&=ImGui::GetBackgroundDrawList()->_ClipRectStack.Size==stack;
        ImGui::Render(); const auto* data=ImGui::GetDrawData();
        for(int l=0;l<data->CmdListsCount;++l) for(const auto& vertex:data->CmdLists[l]->VtxBuffer)
            valid&=std::isfinite(vertex.pos.x) && std::isfinite(vertex.pos.y);
    }
    valid&=style.Alpha==alpha;
    ImGui::DestroyContext(); std::printf("%s Combat HUD: fixed trace, clipping, finite geometry, style ownership\n",valid ? "PASS" : "FAIL"); return valid ? 0 : 1;
}

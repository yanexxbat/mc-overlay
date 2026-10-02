// Headless UI checks: exercise ownership and input without starting Minecraft.
#include "common/flat_style.hpp"
#include "imgui_internal.h"  // Stack inspection is restricted to this test.

#include <cstdio>
#include <cstdlib>

using namespace mcov::style;

static void Check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

static void DrawModule(bool earlyReturn) {
    FlatPanel panel("##module", ImVec2(380, 220));
    if (!panel.visible() || earlyReturn) return;
    SectionHeader("MODULES");
    AccentButton("Apply");
    for (int i = 0; i < 25; ++i) ImGui::Text("Scrollable row %d", i);
}

int main() {
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1024, 768);
    io.DeltaTime = 1.0f / 60.0f;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.Fonts->AddFontDefault();
    io.Fonts->Build();
    ApplyFlatTheme(ImGui::GetStyle());
    ImGuiStyle& theme = ImGui::GetStyle();
    Check(theme.WindowRounding == 10 && theme.FrameRounding == 6 &&
          theme.PopupRounding == 8 && theme.GrabRounding == 5 &&
          theme.ScrollbarRounding == 6, "requested rounding");
    Check(ImGui::ColorConvertFloat4ToU32(theme.Colors[ImGuiCol_WindowBg]) == kBackground &&
          ImGui::ColorConvertFloat4ToU32(theme.Colors[ImGuiCol_ChildBg]) == kPanel &&
          ImGui::ColorConvertFloat4ToU32(theme.Colors[ImGuiCol_ButtonHovered]) == kHover &&
          ImGui::ColorConvertFloat4ToU32(theme.Colors[ImGuiCol_CheckMark]) == kAccent &&
          ImGui::ColorConvertFloat4ToU32(theme.Colors[ImGuiCol_Text]) == kText, "exact palette");
    theme.ScaleAllSizes(2);
    ApplyFlatTheme(theme);
    Check(theme.WindowRounding == 10, "theme reapplication resets scaling");

    bool value = false;
    bool disabledValue = false;
    ImVec2 labelHit;
    for (int frame = 0; frame < 6; ++frame) {
        if (frame == 2) {
            io.AddMousePosEvent(labelHit.x, labelHit.y);
            io.AddMouseButtonEvent(0, true);
        }
        if (frame == 3) io.AddMouseButtonEvent(0, false);
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(50, 50));
        ImGui::SetNextWindowSize(ImVec2(800, 660));
        ImGui::Begin("Flat UI", nullptr, ImGuiWindowFlags_NoSavedSettings);
        ImGuiContext& ctx = *ImGui::GetCurrentContext();
        const int colors = ctx.ColorStack.Size;
        const int vars = ctx.StyleVarStack.Size;
        const int windows = ctx.CurrentWindowStack.Size;
        const int groups = ctx.GroupStack.Size;
        const float padding = theme.FramePadding.x;
        const ImVec4 text = theme.Colors[ImGuiCol_Text];

        // Nested modifications of the same slot must unwind after an early return.
        [&] {
            StyleScope outer;
            outer.Color(ImGuiCol_Text, kAccent);
            outer.Var(ImGuiStyleVar_FramePadding, ImVec2(20, 20));
            StyleScope inner;
            inner.Color(ImGuiCol_Text, kMuted);
            inner.Var(ImGuiStyleVar_FramePadding, ImVec2(30, 30));
            return;
        }();
        Check(theme.Colors[ImGuiCol_Text].x == text.x && theme.FramePadding.x == padding,
              "style restored on early return");

        DrawModule(frame == 0);
        Check(ctx.CurrentWindowStack.Size == windows && ctx.GroupStack.Size == groups,
              "panel ends child/group on early return");
        ToggleSwitch("Enable overlay##toggle", &value);
        const ImVec2 min = ImGui::GetItemRectMin();
        labelHit = ImVec2(min.x + 90, min.y + 10); // Click the label, not the track.
        ImGui::BeginDisabled();
        ToggleSwitch("Disabled module##disabled", &disabledValue);
        AccentButton("Disabled action");
        ImGui::EndDisabled();
        Check(!disabledValue, "disabled toggle unchanged");

        const ImVec2 available = ImGui::GetContentRegionAvail();
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        {
            FlatPanel remaining("##remaining", ImVec2(-20, 0));
            Check(ImGui::GetWindowSize().x == available.x - 20 - 32,
                  "negative width follows BeginChild semantics");
        }
        Check(ImGui::GetCursorScreenPos().y > origin.y, "panel advances parent layout");
        // An off-screen child still owns an EndChild even when BeginChild is false.
        ImGui::SetCursorScreenPos(ImVec2(origin.x, 2000));
        DrawModule(false);
        Check(ctx.ColorStack.Size == colors && ctx.StyleVarStack.Size == vars &&
              ctx.CurrentWindowStack.Size == windows && ctx.GroupStack.Size == groups,
              "no stack leaks from clipped panels/widgets");
        ImGui::End();
        ImGui::Render();
        const ImDrawData& data = *ImGui::GetDrawData();
        Check(data.TotalVtxCount > 0, "frame produces draw data");
        for (const ImDrawList* list : data.CmdLists) {
            for (const ImDrawCmd& cmd : list->CmdBuffer) {
                Check(!cmd.UserCallback, "panels need no renderer/shader callbacks");
                Check(cmd.ClipRect.x <= cmd.ClipRect.z && cmd.ClipRect.y <= cmd.ClipRect.w,
                      "valid clipping rectangles");
            }
        }
    }
    Check(value, "toggle label is clickable");
    ImGui::DestroyContext();
    std::puts("Flat UI checks passed: palette, scope unwinding, panels, clipping, input.");
}

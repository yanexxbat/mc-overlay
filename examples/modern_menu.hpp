#pragma once

#include <algorithm>
#include "imgui.h"

// Header-only example for mc-overlay's pinned Dear ImGui 1.91.9b.
// These are illustrative settings, not bindings to the live Minecraft modules.
namespace mcov::menu_example {

enum class Tab { Visuals, Player, World, Settings };

struct MenuState {
    bool open = true;
    Tab tab = Tab::Visuals;
    bool boxes = true, names = true, health = false;
    float distance = 96.0f;
    float color[4] = {0.35f, 0.78f, 0.88f, 1.0f};
    bool playerInfo = true, coordinates = false;
    float hudScale = 1.0f;
    bool waypoints = false, worldInfo = true;
    float waypointDistance = 256.0f;
    bool tooltips = true;
};

// Once after ImGui::CreateContext(), before NewFrame().
// This changes the global style; do not call it in every render frame.
// ScaleAllSizes(dpiScale) can be applied once afterward if needed.
inline void ApplyCustomStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    s = ImGuiStyle{}; // Reapplying never accumulates size/DPI changes.
    ImGui::StyleColorsDark(&s);

    s.Alpha = 1.0f; // Keep text opaque; only surface colors are translucent.
    s.DisabledAlpha = 0.50f;
    s.WindowRounding = 16.0f;
    s.ChildRounding = 12.0f;
    s.FrameRounding = 7.0f;
    s.PopupRounding = 10.0f;
    s.ScrollbarRounding = 6.0f;
    s.GrabRounding = 5.0f;
    s.TabRounding = 7.0f;
    s.WindowBorderSize = 0.0f;
    s.ChildBorderSize = 0.0f;
    s.FrameBorderSize = 0.0f;
    s.PopupBorderSize = 0.0f;
    s.TabBorderSize = 0.0f;
    s.WindowPadding = ImVec2(18, 18);
    s.FramePadding = ImVec2(12, 8);
    s.ItemSpacing = ImVec2(14, 10);
    s.ItemInnerSpacing = ImVec2(8, 6);
    s.ScrollbarSize = 8.0f;
    s.GrabMinSize = 12.0f;
    s.AntiAliasedLines = true;
    s.AntiAliasedFill = true;

    const ImVec4 accent(0.35f, 0.78f, 0.88f, 1.0f);
    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = ImVec4(0.92f, 0.94f, 0.97f, 1.0f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.62f, 0.67f, 0.73f, 1.0f);
    c[ImGuiCol_WindowBg] = ImVec4(0.065f, 0.075f, 0.095f, 0.88f);
    c[ImGuiCol_ChildBg] = ImVec4(0.16f, 0.18f, 0.22f, 0.08f);
    c[ImGuiCol_PopupBg] = ImVec4(0.085f, 0.10f, 0.13f, 0.98f);
    c[ImGuiCol_Border] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.19f, 0.23f, 0.90f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.25f, 0.29f, 1.0f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.23f, 0.30f, 0.35f, 1.0f);
    c[ImGuiCol_Button] = ImVec4(0.16f, 0.19f, 0.23f, 0.90f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.22f, 0.28f, 0.33f, 1.0f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.25f, 0.34f, 0.39f, 1.0f);
    c[ImGuiCol_Header] = ImVec4(0.35f, 0.78f, 0.88f, 0.16f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.35f, 0.78f, 0.88f, 0.24f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.35f, 0.78f, 0.88f, 0.32f);
    c[ImGuiCol_CheckMark] = accent;
    c[ImGuiCol_SliderGrab] = accent;
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.55f, 0.88f, 0.96f, 1.0f);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(0.33f, 0.38f, 0.44f, 0.65f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.43f, 0.50f, 0.57f, 0.85f);
    c[ImGuiCol_ScrollbarGrabActive] = accent;
    c[ImGuiCol_Separator] = ImVec4(0.62f, 0.67f, 0.73f, 0.14f);
    c[ImGuiCol_SeparatorHovered] = accent;
    c[ImGuiCol_SeparatorActive] = accent;
    c[ImGuiCol_TextSelectedBg] = ImVec4(0.35f, 0.78f, 0.88f, 0.28f);
    c[ImGuiCol_TextLink] = accent;
    c[ImGuiCol_NavCursor] = accent;
    c[ImGuiCol_Tab] = c[ImGuiCol_Button];
    c[ImGuiCol_TabHovered] = c[ImGuiCol_HeaderHovered];
    c[ImGuiCol_TabSelected] = c[ImGuiCol_HeaderActive];
    c[ImGuiCol_TabSelectedOverline] = accent;
}

// Between NewFrame() and Render(). Own MenuState in the caller, not per frame.
// Uses only public ImGui APIs and exactly two independently clipped children.
inline void RenderMenu(MenuState& state) {
    if (!state.open) return;

    const ImGuiStyle& style = ImGui::GetStyle();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float scale = style.WindowPadding.x / 18.0f;
    const ImVec2 size(
        std::min(780.0f * scale, viewport->WorkSize.x - 24.0f * scale),
        std::min(540.0f * scale, viewport->WorkSize.y - 24.0f * scale));
    if (size.x < 380.0f * scale || size.y < 260.0f * scale) return;

    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowPos(viewport->GetWorkCenter(), ImGuiCond_Appearing,
                            ImVec2(0.5f, 0.5f));
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse;
    if (!ImGui::Begin("mc-overlay##modern-menu", nullptr, flags)) {
        ImGui::End(); // Also required when Begin() returns false.
        return;
    }

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("mc-overlay");
    const float closeWidth = 72.0f * scale;
    ImGui::SameLine(ImGui::GetWindowWidth() - style.WindowPadding.x - closeWidth);
    if (ImGui::Button("Close##menu", ImVec2(closeWidth, 0))) state.open = false;
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 4.0f * scale));

    constexpr const char* tabs[] = {"Visuals", "Player", "World", "Settings"};
    constexpr const char* descriptions[] = {
        "Entity overlay and appearance.", "Player information and HUD.",
        "World information and waypoints.", "Menu preferences."
    };
    const int previousTab = static_cast<int>(state.tab);
    const float sidebarWidth = size.x < 620.0f * scale ? 128.0f * scale : 164.0f * scale;

    if (ImGui::BeginChild("##sidebar", ImVec2(sidebarWidth, 0),
                          ImGuiChildFlags_AlwaysUseWindowPadding)) {
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
        for (int i = 0; i < IM_ARRAYSIZE(tabs); ++i) {
            const bool selected = state.tab == static_cast<Tab>(i);
            ImGui::PushStyleColor(ImGuiCol_Button, selected ?
                style.Colors[ImGuiCol_Header] : ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, style.Colors[ImGuiCol_HeaderHovered]);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, style.Colors[ImGuiCol_HeaderActive]);
            ImGui::PushStyleColor(ImGuiCol_Text, selected ?
                style.Colors[ImGuiCol_CheckMark] : style.Colors[ImGuiCol_Text]);
            if (ImGui::Button(tabs[i], ImVec2(-1, 40.0f * scale)))
                state.tab = static_cast<Tab>(i);
            ImGui::PopStyleColor(4);
        }
        ImGui::PopStyleVar();
    }
    ImGui::EndChild(); // Always balance BeginChild(), even when clipped.
    ImGui::SameLine();

    if (previousTab != static_cast<int>(state.tab))
        ImGui::SetNextWindowScroll(ImVec2(-1, 0));
    if (ImGui::BeginChild("##content", ImVec2(0, 0),
                          ImGuiChildFlags_AlwaysUseWindowPadding)) {
        const int tab = static_cast<int>(state.tab);
        ImGui::TextUnformatted(tabs[tab]);
        ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
        ImGui::TextWrapped("%s", descriptions[tab]);
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 8.0f * scale));
        ImGui::PushItemWidth(-1); // Labels above full-width inputs avoid clipping.

        switch (state.tab) {
        case Tab::Visuals:
            ImGui::Checkbox("Entity boxes", &state.boxes);
            ImGui::Checkbox("Player names", &state.names);
            ImGui::Checkbox("Health bars", &state.health);
            ImGui::Spacing();
            ImGui::TextUnformatted("Render distance");
            ImGui::SliderFloat("##distance", &state.distance, 16, 256, "%.0f blocks",
                               ImGuiSliderFlags_AlwaysClamp);
            ImGui::TextUnformatted("Overlay color");
            ImGui::ColorEdit4("##color", state.color,
                ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
            break;
        case Tab::Player:
            ImGui::Checkbox("Player information", &state.playerInfo);
            ImGui::Checkbox("Coordinates", &state.coordinates);
            ImGui::Spacing();
            ImGui::TextUnformatted("HUD scale");
            ImGui::SliderFloat("##hud-scale", &state.hudScale, 0.75f, 1.50f, "%.2fx",
                               ImGuiSliderFlags_AlwaysClamp);
            break;
        case Tab::World:
            ImGui::Checkbox("World information", &state.worldInfo);
            ImGui::Checkbox("Waypoints", &state.waypoints);
            ImGui::BeginDisabled(!state.waypoints);
            ImGui::TextUnformatted("Waypoint distance");
            ImGui::SliderFloat("##waypoint-distance", &state.waypointDistance,
                               32, 1024, "%.0f blocks", ImGuiSliderFlags_AlwaysClamp);
            ImGui::EndDisabled();
            break;
        case Tab::Settings:
            ImGui::Checkbox("Show tooltips", &state.tooltips);
            if (state.tooltips && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("Enable contextual hints inside this menu.");
            ImGui::Spacing();
            ImGui::TextUnformatted("Example settings");
            if (ImGui::Button("Reset example values", ImVec2(-1, 0))) {
                const Tab selected = state.tab;
                state = MenuState{};
                state.tab = selected;
            }
            break;
        }
        ImGui::PopItemWidth();
    }
    ImGui::EndChild();
    ImGui::End();
}

} // namespace mcov::menu_example

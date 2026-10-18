#pragma once
#include "compact_style.hpp"

namespace mcov::style {
namespace workspace {
inline constexpr unsigned AccentRGB=0xB3DB8F;
inline constexpr unsigned AccentHoverRGB=0xC4E8A6;
inline constexpr unsigned AccentPressedRGB=0x94BE70;
inline constexpr unsigned BackgroundRGB=0x191C1B;
inline constexpr unsigned SidebarRGB=0x131615;
inline constexpr unsigned TextRGB=0xE9EDE7;
inline constexpr unsigned MutedRGB=0x9CA59F;
}

// Menu and injector share one palette. CompactStyle restores fonts and every
// style field after this scope, so HUD and world rendering remain independent.
class WorkspaceStyle final {
    CompactStyle base_;
public:
    WorkspaceStyle() {
        auto& s=ImGui::GetStyle();
        s.WindowRounding=12; s.FrameRounding=5; s.PopupRounding=8;
        s.Colors[ImGuiCol_WindowBg]=Color(workspace::BackgroundRGB);
        s.Colors[ImGuiCol_PopupBg]=Color(0x232725);
        s.Colors[ImGuiCol_Text]=Color(workspace::TextRGB);
        s.Colors[ImGuiCol_TextDisabled]=Color(workspace::MutedRGB);
        s.Colors[ImGuiCol_Border]=Color(0xFFFFFF,.085f);
        s.Colors[ImGuiCol_Separator]=Color(0xFFFFFF,.075f);
        for(auto color:{ImGuiCol_CheckMark,ImGuiCol_SliderGrab,ImGuiCol_SliderGrabActive,
                        ImGuiCol_NavCursor,ImGuiCol_TextLink}) s.Colors[color]=Color(workspace::AccentRGB);
        for(auto color:{ImGuiCol_ButtonActive,ImGuiCol_Header,ImGuiCol_TabSelected,
                        ImGuiCol_TextSelectedBg}) s.Colors[color]=Color(workspace::AccentRGB,.16f);
    }
    WorkspaceStyle(const WorkspaceStyle&)=delete;
    WorkspaceStyle& operator=(const WorkspaceStyle&)=delete;
};
} // namespace mcov::style

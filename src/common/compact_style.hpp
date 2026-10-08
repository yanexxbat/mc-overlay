#pragma once
#include "flat_style.hpp"

namespace mcov::style {
// Reference-led client palette. Kept local so the launcher and HUD retain theirs.
inline void ApplyCustomStyle(ImGuiStyle& s) {
    s.Alpha=1; s.DisabledAlpha=.48f;
    s.WindowPadding={16,14}; s.FramePadding={7,3}; s.ItemSpacing={9,6};
    s.ItemInnerSpacing={6,4}; s.CellPadding={3,4};
    s.WindowRounding=12; s.ChildRounding=8; s.FrameRounding=4;
    s.PopupRounding=6; s.GrabRounding=3; s.ScrollbarRounding=4; s.TabRounding=5;
    s.WindowBorderSize=s.ChildBorderSize=s.FrameBorderSize=s.TabBorderSize=0;
    s.PopupBorderSize=1; s.TabBarBorderSize=s.TabBarOverlineSize=0;
    s.ScrollbarSize=6; s.GrabMinSize=10;
    s.AntiAliasedFill=s.AntiAliasedLines=true;
    auto* c=s.Colors;
    c[ImGuiCol_Text]=Color(0xECECEF); c[ImGuiCol_TextDisabled]=Color(0xAEAEB8);
    c[ImGuiCol_WindowBg]=Color(0x222229,.88f); c[ImGuiCol_ChildBg]={0,0,0,0};
    c[ImGuiCol_PopupBg]=Color(0x26262D,.98f); c[ImGuiCol_Border]=Color(0xFFFFFF,.08f);
    c[ImGuiCol_BorderShadow]={0,0,0,0};
    c[ImGuiCol_FrameBg]=Color(0xFFFFFF,.045f); c[ImGuiCol_FrameBgHovered]=Color(0xFFFFFF,.085f);
    c[ImGuiCol_FrameBgActive]=Color(0xFFFFFF,.12f);
    c[ImGuiCol_Button]=Color(0xFFFFFF,.045f); c[ImGuiCol_ButtonHovered]=Color(0xFFFFFF,.09f);
    c[ImGuiCol_ButtonActive]=Color(0xEF8B70,.18f);
    c[ImGuiCol_CheckMark]=Color(0xEF8B70); c[ImGuiCol_SliderGrab]=Color(0xEF8B70);
    c[ImGuiCol_SliderGrabActive]=Color(0xFFA388);
    c[ImGuiCol_Header]=Color(0xEF8B70,.12f); c[ImGuiCol_HeaderHovered]=Color(0xFFFFFF,.06f);
    c[ImGuiCol_HeaderActive]=Color(0xEF8B70,.18f);
    c[ImGuiCol_Tab]=Color(0xFFFFFF,.035f); c[ImGuiCol_TabHovered]=Color(0xFFFFFF,.09f);
    c[ImGuiCol_TabSelected]=Color(0xEF8B70,.16f); c[ImGuiCol_TabSelectedOverline]={0,0,0,0};
    c[ImGuiCol_TabDimmed]=c[ImGuiCol_Tab]; c[ImGuiCol_TabDimmedSelected]=c[ImGuiCol_TabSelected];
    c[ImGuiCol_Separator]=Color(0xFFFFFF,.065f);
    c[ImGuiCol_SeparatorHovered]=c[ImGuiCol_SeparatorActive]=Color(0xEF8B70);
    c[ImGuiCol_ScrollbarBg]={0,0,0,0}; c[ImGuiCol_ScrollbarGrab]=Color(0xFFFFFF,.15f);
    c[ImGuiCol_ScrollbarGrabHovered]=Color(0xFFFFFF,.27f); c[ImGuiCol_ScrollbarGrabActive]=Color(0xEF8B70,.65f);
    c[ImGuiCol_TableBorderStrong]=c[ImGuiCol_TableBorderLight]=c[ImGuiCol_Separator];
    c[ImGuiCol_TableRowBg]=c[ImGuiCol_TableRowBgAlt]={0,0,0,0};
    c[ImGuiCol_NavCursor]=Color(0xFFA388); c[ImGuiCol_TextSelectedBg]=Color(0xEF8B70,.28f);
    c[ImGuiCol_TextLink]=Color(0xFFA388);
}
inline void ApplyCustomStyle() { ApplyCustomStyle(ImGui::GetStyle()); }

class CompactStyle final {
    ImGuiStyle saved_=ImGui::GetStyle();
public:
    CompactStyle() { ApplyCustomStyle(); ImGui::PushFont(MenuFont()); }
    ~CompactStyle() { ImGui::PopFont(); ImGui::GetStyle()=saved_; }
    CompactStyle(const CompactStyle&)=delete;
    CompactStyle& operator=(const CompactStyle&)=delete;
};
} // namespace mcov::style

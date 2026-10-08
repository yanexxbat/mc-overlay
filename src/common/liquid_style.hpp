#pragma once
#include "flat_style.hpp"

namespace mcov::style {
// Lokales Theme: HUD, Welt-Effekte und andere ImGui-Fenster behalten ihren Stil.
class LiquidStyle final {
    ImGuiStyle saved_;
public:
    LiquidStyle() : saved_(ImGui::GetStyle()) {
        auto& s = ImGui::GetStyle();
        s.WindowPadding={16,16}; s.FramePadding={10,6}; s.ItemSpacing={10,8};
        s.ItemInnerSpacing={7,5}; s.CellPadding={10,7};
        s.WindowRounding=22; s.ChildRounding=14; s.FrameRounding=8;
        s.PopupRounding=12; s.TabRounding=8; s.GrabRounding=6; s.ScrollbarRounding=8;
        s.WindowBorderSize=0; s.ChildBorderSize=0; s.FrameBorderSize=0; s.PopupBorderSize=1;
        s.ScrollbarSize=8; s.GrabMinSize=13;
        s.TabBarBorderSize=0; s.TabBarOverlineSize=0;
        auto* c=s.Colors;
        c[ImGuiCol_Text]=Color(0xF3F4FF); c[ImGuiCol_TextDisabled]=Color(0xD7DCEC);
        c[ImGuiCol_WindowBg]=Color(0x151A2A,.96f); c[ImGuiCol_ChildBg]=Color(0x101522,.24f);
        c[ImGuiCol_PopupBg]=Color(0x222B42,.99f); c[ImGuiCol_Border]=Color(0xB9CBFF,.16f);
        c[ImGuiCol_BorderShadow]={0,0,0,0};
        c[ImGuiCol_FrameBg]=Color(0xC0CFFF,.09f); c[ImGuiCol_FrameBgHovered]=Color(0xC0CFFF,.17f);
        c[ImGuiCol_FrameBgActive]=Color(0xC0CFFF,.23f);
        c[ImGuiCol_Button]=Color(0xC0CFFF,.10f); c[ImGuiCol_ButtonHovered]=Color(0xC0CFFF,.21f);
        c[ImGuiCol_ButtonActive]=Color(0x818CF8,.50f);
        c[ImGuiCol_CheckMark]=Color(0xB2B9FF); c[ImGuiCol_SliderGrab]=Color(0xB2B9FF);
        c[ImGuiCol_SliderGrabActive]=Color(0xE3E6FF);
        c[ImGuiCol_Header]=Color(0x818CF8,.22f); c[ImGuiCol_HeaderHovered]=Color(0x818CF8,.33f);
        c[ImGuiCol_HeaderActive]=Color(0x818CF8,.45f);
        c[ImGuiCol_Tab]=Color(0xC0CFFF,.06f); c[ImGuiCol_TabHovered]=Color(0xC0CFFF,.20f);
        c[ImGuiCol_TabSelected]=Color(0x818CF8,.32f);
        c[ImGuiCol_TabSelectedOverline]={0,0,0,0}; c[ImGuiCol_TabDimmed]=c[ImGuiCol_Tab];
        c[ImGuiCol_TabDimmedSelected]=c[ImGuiCol_TabSelected];
        c[ImGuiCol_Separator]=Color(0xCBD5FF,.10f);
        c[ImGuiCol_SeparatorHovered]=Color(0xB2B9FF,.45f); c[ImGuiCol_SeparatorActive]=Color(0xB2B9FF);
        c[ImGuiCol_ScrollbarBg]={0,0,0,0}; c[ImGuiCol_ScrollbarGrab]=Color(0xC0CFFF,.22f);
        c[ImGuiCol_ScrollbarGrabHovered]=Color(0xC0CFFF,.40f); c[ImGuiCol_ScrollbarGrabActive]=Color(0xB2B9FF,.60f);
        c[ImGuiCol_TableBorderStrong]=c[ImGuiCol_Separator]; c[ImGuiCol_TableBorderLight]=Color(0xCBD5FF,.05f);
        c[ImGuiCol_TableHeaderBg]=Color(0xC0CFFF,.06f);
        c[ImGuiCol_TableRowBg]={0,0,0,0}; c[ImGuiCol_TableRowBgAlt]=Color(0xC0CFFF,.025f);
        c[ImGuiCol_NavCursor]=Color(0xB2B9FF); c[ImGuiCol_TextSelectedBg]=Color(0x818CF8,.42f);
        ImGui::PushFont(MenuFont());
    }
    ~LiquidStyle() { ImGui::PopFont(); ImGui::GetStyle()=saved_; }
    LiquidStyle(const LiquidStyle&)=delete;
    LiquidStyle& operator=(const LiquidStyle&)=delete;
};
} // namespace mcov::style

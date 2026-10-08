#pragma once
#include "flat_style.hpp"

namespace mcov::style {
// Gemeinsame Palette und RAII für Menü und Launcher; verändert kein HUD dauerhaft.
struct ClassicStyle final {
    ImGuiStyle saved=ImGui::GetStyle();
    ClassicStyle() {
        auto& s=ImGui::GetStyle();
        s.WindowPadding={0,0}; s.WindowRounding=s.ChildRounding=s.FrameRounding=s.TabRounding=0;
        s.PopupRounding=0; s.FramePadding={4,2}; s.ItemSpacing={6,4}; s.ItemInnerSpacing={5,3};
        s.CellPadding={7,3}; s.ScrollbarSize=7; s.ScrollbarRounding=0; s.GrabRounding=0;
        s.WindowBorderSize=s.ChildBorderSize=s.FrameBorderSize=s.TabBorderSize=1;
        auto* c=s.Colors;
        const auto bg=Color(0x242424),field=Color(0x191919),text=Color(0xECECEC),muted=Color(0xB6AFBE),purple=Color(0xAE69D1);
        for(int i=0;i<ImGuiCol_COUNT;++i) c[i]=bg;
        c[ImGuiCol_Text]=text; c[ImGuiCol_TextDisabled]=muted; c[ImGuiCol_WindowBg]=bg;
        c[ImGuiCol_ChildBg]={0,0,0,0}; c[ImGuiCol_PopupBg]=bg; c[ImGuiCol_Border]=Color(0x080808);
        c[ImGuiCol_FrameBg]=c[ImGuiCol_Button]=c[ImGuiCol_Tab]=field;
        c[ImGuiCol_FrameBgHovered]=c[ImGuiCol_ButtonHovered]=c[ImGuiCol_TabHovered]=Color(0x39303F);
        c[ImGuiCol_FrameBgActive]=c[ImGuiCol_ButtonActive]=Color(0x493451);
        c[ImGuiCol_CheckMark]=c[ImGuiCol_SliderGrab]=c[ImGuiCol_HeaderActive]=purple;
        c[ImGuiCol_SliderGrabActive]=text; c[ImGuiCol_Header]=Color(0x2E2733); c[ImGuiCol_HeaderHovered]=Color(0x403247);
        c[ImGuiCol_TabSelected]=bg; c[ImGuiCol_TabSelectedOverline]=purple;
        c[ImGuiCol_Separator]=Color(0x5C3A6E); c[ImGuiCol_TableBorderStrong]=Color(0x080808);
        c[ImGuiCol_TableBorderLight]=Color(0x292929); c[ImGuiCol_TableRowBg]=c[ImGuiCol_TableRowBgAlt]=bg;
        c[ImGuiCol_ScrollbarBg]=field; c[ImGuiCol_ScrollbarGrab]=Color(0x65566F);
        c[ImGuiCol_ScrollbarGrabHovered]=c[ImGuiCol_ScrollbarGrabActive]=purple;
        c[ImGuiCol_NavCursor]=purple; c[ImGuiCol_TextSelectedBg]=Color(0x674177);
        c[ImGuiCol_ModalWindowDimBg]={0,0,0,.55f};
        ImGui::PushFont(style::ClassicMenuFont());
    }
    ~ClassicStyle() { ImGui::PopFont(); ImGui::GetStyle()=saved; }
};
} // namespace mcov::style

#pragma once
#include "imgui.h"

namespace mcov::style::compact {
// Shared drawing tokens for menus and snapshot HUDs; no renderer or font ownership.
inline constexpr unsigned AccentRGB=0x8EDB1A,AccentActiveRGB=0xA6EF42,AccentPressedRGB=0x72B411;
inline constexpr unsigned AccentInkRGB=0x17200B,SelectedTextRGB=0xD4F3B8;
inline constexpr ImU32 Rgba(unsigned rgb,unsigned alpha=255) {
    return IM_COL32((rgb>>16)&255,(rgb>>8)&255,rgb&255,alpha);
}
inline constexpr ImU32 Surface=IM_COL32(34,34,41,224);
inline constexpr ImU32 Text=IM_COL32(236,236,239,255);
inline constexpr ImU32 Muted=IM_COL32(174,174,184,255);
inline constexpr ImU32 Accent=Rgba(AccentRGB);
inline constexpr ImU32 AccentActive=Rgba(AccentActiveRGB);
inline constexpr ImU32 AccentPressed=Rgba(AccentPressedRGB);
inline constexpr ImU32 AccentInk=Rgba(AccentInkRGB);
inline constexpr ImU32 Separator=IM_COL32(255,255,255,17);
inline constexpr ImU32 Track=IM_COL32(255,255,255,12);
inline constexpr ImU32 Pressed=Rgba(AccentRGB,46);
inline constexpr ImU32 Positive=IM_COL32(165,199,173,255);
inline constexpr ImU32 Warning=IM_COL32(232,187,115,255);
inline constexpr ImU32 Danger=IM_COL32(242,128,128,255);
inline constexpr ImU32 ErrorText=IM_COL32(255,166,166,255);
inline constexpr float HudRounding=8;
inline void Panel(ImDrawList& draw,ImVec2 min,ImVec2 max) {
    draw.AddRectFilled(min,max,Surface,HudRounding);
}
inline void Heading(ImDrawList& draw,ImVec2 min,float width,const char* label) {
    draw.AddText({min.x+10,min.y+6},Text,label);
    draw.AddLine({min.x+10,min.y+22},{min.x+width-10,min.y+22},Separator);
}
} // namespace mcov::style::compact

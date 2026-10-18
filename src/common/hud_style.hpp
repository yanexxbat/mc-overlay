#pragma once
#include "imgui.h"
#include <algorithm>

namespace mcov::style::hud {
// In-game surfaces are intentionally separate from the lime-accented controls.
inline constexpr ImU32 Surface=IM_COL32(16,21,26,158);
inline constexpr ImU32 Text=IM_COL32(246,247,249,255),Muted=IM_COL32(169,177,185,255);
inline constexpr ImU32 Accent=Text,AccentActive=Text,Track=IM_COL32(255,255,255,24);
inline constexpr ImU32 Separator=IM_COL32(255,255,255,38),Pressed=IM_COL32(238,242,246,225);
inline constexpr ImU32 Ink=IM_COL32(23,29,34,255),Rail=IM_COL32(245,247,249,245);
inline constexpr ImU32 Positive=IM_COL32(165,199,173,255),Warning=IM_COL32(232,187,115,255),Danger=IM_COL32(242,128,128,255);
inline constexpr float Rounding=4,Inset=10,RailWidth=2;
inline ImFont* Font() { return ImGui::GetIO().FontDefault?ImGui::GetIO().FontDefault:ImGui::GetFont(); }
inline void Label(ImDrawList& draw,ImFont* font,float size,ImVec2 pos,ImU32 color,const char* text,float wrap=0) {
    const unsigned alpha=(color>>IM_COL32_A_SHIFT)&255;
    draw.AddText(font,size,{pos.x+1,pos.y+1},IM_COL32(0,0,0,alpha*180/255),text,nullptr,wrap);
    draw.AddText(font,size,pos,color,text,nullptr,wrap);
}
inline void Label(ImDrawList& draw,ImVec2 pos,ImU32 color,const char* text) {
    Label(draw,ImGui::GetFont(),ImGui::GetFontSize(),pos,color,text);
}
inline void Panel(ImDrawList& draw,ImVec2 min,ImVec2 max) {
    draw.AddRectFilled(min,{max.x-RailWidth,max.y},Surface,Rounding,ImDrawFlags_RoundCornersLeft);
    draw.AddRectFilled({max.x-RailWidth,min.y},max,Rail,1);
}
inline void Heading(ImDrawList& draw,ImVec2 min,float width,const char* label) {
    draw.PushClipRect({min.x+Inset,min.y},{min.x+width-Inset,min.y+24},true);
    Label(draw,{min.x+Inset,min.y+5},Muted,label);
    draw.PopClipRect();
}
inline void Row(ImDrawList& draw,float width,float y,const char* label,const char* value,ImU32 valueColor=Text) {
    const float right=width-Inset;
    const float valueX=std::max(Inset,right-ImGui::CalcTextSize(value).x);
    draw.PushClipRect({Inset,y},{std::max(Inset,valueX-8),y+ImGui::GetFontSize()+1},true);
    Label(draw,{Inset,y},Muted,label);
    draw.PopClipRect();
    draw.PushClipRect({Inset,y},{right,y+ImGui::GetFontSize()+1},true);
    Label(draw,{valueX,y},valueColor,value);
    draw.PopClipRect();
}
} // namespace mcov::style::hud

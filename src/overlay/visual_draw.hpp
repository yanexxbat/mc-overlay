#pragma once

#include "imgui.h"

// Backend-independent, allocation-free helpers. All primitives use the current
// draw-list texture and clip rectangle; no shaders or blend callbacks are added.
namespace mcov::visual {

float Safe(float value, float fallback, float low, float high) noexcept;
ImU32 Color(const float rgba[4], float opacity = 1.f) noexcept;
ImU32 Alpha(ImU32 color, float opacity) noexcept;
ImU32 Mix(ImU32 first, ImU32 last, float fraction) noexcept;
void ShadeGradient(ImDrawList& draw, int firstVertex, ImVec2 start, ImVec2 end,
                   ImU32 first, ImU32 last) noexcept;
void DrawGradientRect(ImDrawList& draw, ImVec2 lo, ImVec2 hi, ImU32 top,
                      ImU32 bottom, float rounding = 0.f);
void DrawGradientPolygon(ImDrawList& draw, const ImVec2* points, int count,
                         ImVec2 gradientStart, ImVec2 gradientEnd,
                         ImU32 top, ImU32 bottom);
void DrawGradientLine(ImDrawList& draw, ImVec2 start, ImVec2 end,
                      ImU32 first, ImU32 last, float thickness = 1.f);
void DrawCornerBox(ImDrawList& draw, ImVec2 lo, ImVec2 hi, ImU32 color,
                   float thickness, float fraction, float rounding);
void DrawBoxOutline(ImDrawList& draw, ImVec2 lo, ImVec2 hi, ImU32 color,
                    float thickness, float rounding, bool corners, float fraction);
void DrawBoxGlow(ImDrawList& draw, ImVec2 lo, ImVec2 hi, ImU32 color,
                 float thickness, float rounding, bool corners, float fraction,
                 float radius, float opacity);
void RenderTextWithOutline(ImDrawList& draw, ImFont& font, float size,
                           ImVec2 at, ImU32 color, const char* text,
                           float outlineOpacity = .75f);

} // namespace mcov::visual

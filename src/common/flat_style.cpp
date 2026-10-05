#include "flat_style.hpp"

#include <algorithm>
#include <windows.h>

#include "data/fonts.h"  // Existing embedded Inter assets; no glass runtime.
#include "log.hpp"

namespace mcov::style {
namespace {
constexpr ImU32 kAccentHover  = IM_COL32(104, 117, 250, 255);
constexpr ImU32 kAccentActive = IM_COL32(71, 82, 196, 255);
constexpr float kShadowMargin = 16.0f;

const char* VisibleLabelEnd(const char* label) {
    const char* end = label;
    while (*end && !(end[0] == '#' && end[1] == '#')) ++end;
    return end;
}
} // namespace

void ApplyFlatTheme(ImGuiStyle& style) {
    style = ImGuiStyle{};
    ImGui::StyleColorsDark(&style);
    style.Alpha             = 1.0f;
    style.WindowRounding    = 10.0f;
    style.ChildRounding     = 10.0f;
    style.FrameRounding     = 6.0f;
    style.PopupRounding     = 8.0f;
    style.GrabRounding      = 5.0f;
    style.ScrollbarRounding = 6.0f;
    style.TabRounding       = 6.0f;
    style.WindowPadding     = ImVec2(16, 16);
    style.FramePadding      = ImVec2(10, 6);
    style.ItemSpacing       = ImVec2(10, 8);
    style.ItemInnerSpacing  = ImVec2(8, 6);
    style.ScrollbarSize     = 10.0f;
    style.GrabMinSize       = 12.0f;
    style.WindowBorderSize  = 0.0f;
    style.ChildBorderSize   = 0.0f;
    style.PopupBorderSize   = 1.0f;
    style.FrameBorderSize   = 0.0f;
    style.TabBorderSize     = 0.0f;
    style.AntiAliasedLines  = true;
    style.AntiAliasedFill   = true;

    const auto color = [](ImU32 rgba) { return ImGui::ColorConvertU32ToFloat4(rgba); };
    ImVec4* c = style.Colors;
    // Start from opaque surfaces for every slot, including table rows.
    for (int i = 0; i < ImGuiCol_COUNT; ++i) c[i] = color(kPanel);
    c[ImGuiCol_Text]         = color(kText);
    c[ImGuiCol_TextDisabled] = color(kMuted);
    c[ImGuiCol_WindowBg]     = color(kBackground);
    c[ImGuiCol_ChildBg]      = color(kPanel);
    c[ImGuiCol_PopupBg]      = color(kPanel);
    c[ImGuiCol_Border]       = color(kBorder);
    c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

    c[ImGuiCol_FrameBg]        = color(kBackground);
    c[ImGuiCol_FrameBgHovered] = color(kHover);
    c[ImGuiCol_FrameBgActive]  = color(kHover);
    c[ImGuiCol_Button]         = color(kPanel);
    c[ImGuiCol_ButtonHovered]  = color(kHover);
    c[ImGuiCol_ButtonActive]   = color(kAccent);
    c[ImGuiCol_Header]         = color(kPanel);
    c[ImGuiCol_HeaderHovered]  = color(kHover);
    c[ImGuiCol_HeaderActive]   = color(kAccent);
    c[ImGuiCol_CheckMark]      = color(kAccent);
    c[ImGuiCol_SliderGrab]     = color(kAccent);
    c[ImGuiCol_SliderGrabActive] = color(kAccentHover);

    c[ImGuiCol_TitleBg]          = color(kBackground);
    c[ImGuiCol_TitleBgActive]    = color(kBackground);
    c[ImGuiCol_TitleBgCollapsed] = color(kBackground);
    c[ImGuiCol_MenuBarBg]        = color(kPanel);
    c[ImGuiCol_ScrollbarBg]          = color(kPanel);
    c[ImGuiCol_ScrollbarGrab]        = color(kBorder);
    c[ImGuiCol_ScrollbarGrabHovered] = color(kMuted);
    c[ImGuiCol_ScrollbarGrabActive]  = color(kAccent);
    c[ImGuiCol_Separator]        = color(kBorder);
    c[ImGuiCol_SeparatorHovered] = color(kAccentHover);
    c[ImGuiCol_SeparatorActive]  = color(kAccent);
    c[ImGuiCol_ResizeGrip]        = color(kBorder);
    c[ImGuiCol_ResizeGripHovered] = color(kAccentHover);
    c[ImGuiCol_ResizeGripActive]  = color(kAccent);

    c[ImGuiCol_Tab]         = color(kBackground);
    c[ImGuiCol_TabHovered]  = color(kHover);
    c[ImGuiCol_TabSelected] = color(kPanel);
    c[ImGuiCol_TabSelectedOverline] = color(kAccent);
    c[ImGuiCol_TabDimmed]   = color(kBackground);
    c[ImGuiCol_TabDimmedSelected] = color(kPanel);
    c[ImGuiCol_TabDimmedSelectedOverline] = color(kMuted);
    c[ImGuiCol_PlotLines]            = color(kAccent);
    c[ImGuiCol_PlotLinesHovered]     = color(kAccentHover);
    c[ImGuiCol_PlotHistogram]        = color(kAccent);
    c[ImGuiCol_PlotHistogramHovered] = color(kAccentHover);
    c[ImGuiCol_TableHeaderBg]     = color(kHover);
    c[ImGuiCol_TableBorderStrong] = color(kBorder);
    c[ImGuiCol_TableBorderLight]  = color(kBorder);
    c[ImGuiCol_TableRowBg]        = color(kPanel);
    c[ImGuiCol_TableRowBgAlt]     = color(kHover);
    c[ImGuiCol_TextLink]          = color(kAccentHover);
    c[ImGuiCol_TextSelectedBg]    = color(kAccentActive);
    c[ImGuiCol_DragDropTarget]    = color(kAccent);
    c[ImGuiCol_NavCursor]         = color(kAccent);
    c[ImGuiCol_NavWindowingHighlight] = color(kAccent);
    c[ImGuiCol_NavWindowingDimBg] = ImVec4(0, 0, 0, 0.55f);
    c[ImGuiCol_ModalWindowDimBg]  = ImVec4(0, 0, 0, 0.55f);
}

void DrawPanelShadow(ImDrawList& draw, ImVec2 min, ImVec2 max, float rounding) {
    if (max.x <= min.x || max.y <= min.y) return;
    struct Layer { float spread; int alpha; };
    constexpr Layer layers[] = {{12, 6}, {9, 9}, {6, 13}, {3, 18}, {1, 24}};
    constexpr float offsetY = 3.0f;
    for (const auto& layer : layers) {
        draw.AddRectFilled(
            ImVec2(min.x - layer.spread, min.y - layer.spread + offsetY),
            ImVec2(max.x + layer.spread, max.y + layer.spread + offsetY),
            IM_COL32(0, 0, 0, layer.alpha), rounding + layer.spread);
    }
}

void DrawFlatPanel(ImDrawList& draw, ImVec2 min, ImVec2 max, float rounding, ImU32 fill) {
    if (max.x <= min.x || max.y <= min.y) return;
    DrawPanelShadow(draw, min, max, rounding);
    draw.AddRectFilled(min, max, fill, rounding);
}

StyleScope::~StyleScope() {
    if (vars_) ImGui::PopStyleVar(vars_);
    if (colors_) ImGui::PopStyleColor(colors_);
}
void StyleScope::Color(ImGuiCol index, ImU32 color) {
    ImGui::PushStyleColor(index, color);
    ++colors_;
}
void StyleScope::Color(ImGuiCol index, const ImVec4& color) {
    ImGui::PushStyleColor(index, color);
    ++colors_;
}
void StyleScope::Var(ImGuiStyleVar index, float value) {
    ImGui::PushStyleVar(index, value);
    ++vars_;
}
void StyleScope::Var(ImGuiStyleVar index, const ImVec2& value) {
    ImGui::PushStyleVar(index, value);
    ++vars_;
}

FlatPanel::FlatPanel(const char* id, ImVec2 size) {
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const ImVec2 extent(std::max(2 * kShadowMargin + 4, size.x > 0 ? size.x : available.x + size.x),
                        std::max(2 * kShadowMargin + 4, size.y > 0 ? size.y : available.y + size.y));
    const ImVec2 min(origin.x + kShadowMargin, origin.y + kShadowMargin);
    const ImVec2 max(origin.x + extent.x - kShadowMargin, origin.y + extent.y - kShadowMargin);

    ImGui::BeginGroup();
    ImGui::Dummy(extent);  // Reserve the whole footprint, including the shadow.
    after_ = ImGui::GetCursorScreenPos();
    DrawFlatPanel(*ImGui::GetWindowDrawList(), min, max);
    ImGui::SetCursorScreenPos(min);
    style_.Var(ImGuiStyleVar_WindowPadding, ImVec2(14, 14));
    style_.Var(ImGuiStyleVar_ChildRounding, 10.0f);
    visible_ = ImGui::BeginChild(id, ImVec2(max.x - min.x, max.y - min.y),
                                ImGuiChildFlags_AlwaysUseWindowPadding,
                                ImGuiWindowFlags_NoBackground);
}
FlatPanel::~FlatPanel() {
    ImGui::EndChild();
    ImGui::SetCursorScreenPos(after_);
    ImGui::EndGroup();
}

void SectionHeader(const char* label) {
    ImGui::Dummy(ImVec2(0, 4));
    StyleScope scope;
    scope.Color(ImGuiCol_Text, kMuted);
    ImGui::TextUnformatted(label);
    ImGui::Separator();
}

bool AccentButton(const char* label, ImVec2 size) {
    StyleScope scope;
    scope.Color(ImGuiCol_Button, kAccent);
    scope.Color(ImGuiCol_ButtonHovered, kAccentHover);
    scope.Color(ImGuiCol_ButtonActive, kAccentActive);
    return ImGui::Button(label, size);
}

bool ToggleSwitch(const char* label, bool* value) {
    ImDrawList& draw = *ImGui::GetWindowDrawList();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float height = ImGui::GetFrameHeight() * 0.82f;
    const float width = height * 1.85f;
    const float radius = height * 0.5f;
    const char* end = VisibleLabelEnd(label);
    const float labelWidth = ImGui::CalcTextSize(label, end).x;
    const float labelOffset = width + ImGui::GetStyle().ItemInnerSpacing.x;

    // Include the label in the hit target and enable keyboard navigation.
    const bool pressed = ImGui::InvisibleButton(label, ImVec2(labelOffset + labelWidth, height),
                                                ImGuiButtonFlags_EnableNav);
    if (pressed) *value = !*value;
    ImGuiStorage* storage = ImGui::GetStateStorage();
    const ImGuiID id = ImGui::GetID(label);
    const float target = *value ? 1.0f : 0.0f;
    float current = storage->GetFloat(id, target);
    current += (target - current) * std::clamp(ImGui::GetIO().DeltaTime * 12.0f, 0.0f, 1.0f);
    storage->SetFloat(id, current);

    const ImU32 track = ImGui::GetColorU32(*value ? ImGuiCol_CheckMark :
                                          (ImGui::IsItemHovered() ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg));
    draw.AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), track, radius);
    draw.AddCircleFilled(ImVec2(pos.x + radius + current * (width - height), pos.y + radius),
                          radius - 2.0f, ImGui::GetColorU32(ImGuiCol_Text));
    draw.AddText(ImVec2(pos.x + labelOffset, pos.y + (height - ImGui::GetTextLineHeight()) * 0.5f),
                 ImGui::GetColorU32(ImGuiCol_Text), label, end);
    if (ImGui::IsItemFocused()) {
        draw.AddRect(ImVec2(pos.x - 2, pos.y - 2), ImVec2(pos.x + width + 2, pos.y + height + 2),
                     ImGui::GetColorU32(ImGuiCol_NavCursor), radius + 2, 0, 1.5f);
    }
    return pressed;
}

bool LoadFonts(float uiSize) {
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;  // Embedded byte vectors outlive the atlas.
    ImFont* medium = io.Fonts->AddFontFromMemoryTTF(
        inter_medium.data(), static_cast<int>(inter_medium.size()), uiSize, &cfg);
    if (!medium) {
        MCOV_LOG_WARN("Inter unavailable; using ImGui's default font");
        return false;
    }
    io.FontDefault = medium;
    io.Fonts->AddFontFromMemoryTTF(inter_semibold.data(), static_cast<int>(inter_semibold.size()),
                                  uiSize * 1.3f, &cfg);
    // Keep the HUD's first two atlas entries and default font unchanged.
    io.Fonts->AddFontFromMemoryTTF(inter_medium.data(), static_cast<int>(inter_medium.size()), 14.f, &cfg);
    io.Fonts->AddFontFromMemoryTTF(inter_semibold.data(), static_cast<int>(inter_semibold.size()), 14.f, &cfg);
    // RCDATA travels with the executable: no runtime font paths or glass renderer.
    const HMODULE module = GetModuleHandleW(nullptr);
    const HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(101), RT_RCDATA);
    if (resource) {
        const HGLOBAL data = LoadResource(module, resource);
        static const ImWchar range[] = {0xf000, 0xf8ff, 0};
        ImFontConfig iconCfg = cfg;
        iconCfg.PixelSnapH = true;
        io.Fonts->AddFontFromMemoryTTF(LockResource(data), static_cast<int>(SizeofResource(module, resource)),
                                     30.f, &iconCfg, range);
    }
    ImFontConfig classic; classic.PixelSnapH=true; classic.SizePixels=13;
    io.Fonts->AddFontDefault(&classic);
    return true;
}

ImFont* MenuFont(bool bold) {
    const auto& fonts = ImGui::GetIO().Fonts->Fonts;
    return fonts.Size >= 4 ? fonts[bold ? 3 : 2] : ImGui::GetFont();
}
ImFont* MenuIconFont() {
    const auto& fonts = ImGui::GetIO().Fonts->Fonts;
    return fonts.Size >= 5 ? fonts[4] : ImGui::GetFont();
}
ImFont* ClassicMenuFont() {
    const auto& fonts=ImGui::GetIO().Fonts->Fonts;
    return fonts.Size>=6?fonts[5]:fonts.back();
}
} // namespace mcov::style

#pragma once

#include "imgui.h"

namespace mcov::style {

inline constexpr ImU32 kBackground = IM_COL32(18, 18, 20, 255);  // #121214
inline constexpr ImU32 kPanel      = IM_COL32(30, 30, 36, 255);  // #1E1E24
inline constexpr ImU32 kHover      = IM_COL32(42, 42, 50, 255);  // #2A2A32
inline constexpr ImU32 kAccent     = IM_COL32(88, 101, 242, 255); // #5865F2
inline constexpr ImU32 kText       = IM_COL32(236, 236, 238, 255); // #ECECEE
inline constexpr ImU32 kMuted      = IM_COL32(150, 150, 163, 255);
inline constexpr ImU32 kBorder     = IM_COL32(53, 53, 63, 255);

// Call once after CreateContext(). This resets the style, so reapplying it is
// idempotent. Apply DPI scaling separately, after this function if needed.
void ApplyFlatTheme(ImGuiStyle& style);
bool LoadFonts(float uiSize = 17.0f);
ImFont* MenuFont(bool bold = false);
ImFont* MenuIconFont();

// Screen-space drawing; renderer independent, no capture, textures or JNI.
// Draw on the parent draw list before child contents to preserve clipping and
// stacking. The panel's opaque fill covers the shadow inside its own bounds.
void DrawPanelShadow(ImDrawList& draw, ImVec2 min, ImVec2 max, float rounding);
void DrawFlatPanel(ImDrawList& draw, ImVec2 min, ImVec2 max,
                   float rounding = 10.0f, ImU32 fill = kPanel);

// A widget owns every push it makes. Reverse-order pops happen on scope exit,
// including early returns. Do not mix manual pops into one of these scopes.
class StyleScope final {
public:
    StyleScope() = default;
    ~StyleScope();
    StyleScope(const StyleScope&) = delete;
    StyleScope& operator=(const StyleScope&) = delete;

    void Color(ImGuiCol index, ImU32 color);
    void Color(ImGuiCol index, const ImVec4& color);
    void Var(ImGuiStyleVar index, float value);
    void Var(ImGuiStyleVar index, const ImVec2& value);

private:
    int colors_ = 0;
    int vars_   = 0;
};

// A rounded module container with padding, independent scrolling and reserved
// space for its shadow. size follows BeginChild(): 0 fills available space,
// negative values subtract from available space. Always ends its child/group.
class FlatPanel final {
public:
    explicit FlatPanel(const char* id, ImVec2 size = ImVec2(0, 0));
    ~FlatPanel();
    FlatPanel(const FlatPanel&) = delete;
    FlatPanel& operator=(const FlatPanel&) = delete;
    bool visible() const { return visible_; }

private:
    StyleScope style_;
    ImVec2 after_;
    bool visible_ = false;
};

void SectionHeader(const char* label);
bool AccentButton(const char* label, ImVec2 size = ImVec2(0, 0));
bool ToggleSwitch(const char* label, bool* value);

} // namespace mcov::style

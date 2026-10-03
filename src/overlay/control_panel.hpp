#pragma once
#include <cstdint>
#include "imgui.h"

namespace mcov::control_panel {

enum class Page { Combat, Visuals, Blocks, Hud, Stats, Settings };
struct State {
    Page page = Page::Combat;
    int theme = 0; // Amber Dark, Graphite, Light, Crimson
    bool navCollapsed = false;
    bool reduceMotion = false;
    bool positioned = false;
    bool dragging = false;
    ImVec2 position{}; // Relative to the current game viewport; retained on reopen.
    ImVec2 dragOffset{};
    Page renderedPage = Page::Combat;
    bool animateNextPage = false;
    float pageAlpha = 1.f;
    float pageStartAlpha = 1.f;
    float pageElapsed = .16f;
};
struct Status {
    bool connected = false;
    const char* agent = "waiting for agent";
    const char* version = "detecting";
    std::uint32_t pid = 0;
};
struct Actions { bool close = false; bool reset = false; };
using Content = void (*)(void*, Page);

// Geometry and controls follow the user's screenshot. Renderer-independent;
// presentation state only. The caller owns all real module settings and actions.
Actions Render(State& state, const Status& status, void* owner, Content content);
const char* ThemeName(int index);
float Scale();
bool MotionAllowed(const State& state);
void Section(const char* label);

// Labels and controls occupy separate columns with stable per-row IDs.
bool BeginRows(const char* id);
bool CheckRow(const char* label, bool* value);
bool ComboRow(const char* label, int* value, const char* const* items, int count);
bool SliderRow(const char* label, float* value, float min, float max, const char* format);
bool SliderDoubleRow(const char* label, double* value, double min, double max, const char* format);
bool SliderIntRow(const char* label, int* value, int min, int max, const char* format);
bool ColorRow(const char* label, float* value);
void ValueRow(const char* label, const char* value);

} // namespace mcov::control_panel

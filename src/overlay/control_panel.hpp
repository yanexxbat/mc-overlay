#pragma once
#include <cstdint>
#include <span>
#include <array>
#include "imgui.h"

namespace mcov::control_panel {

enum class Page { Combat, Visuals, Blocks, Hud, Stats, Settings, Misc };
enum class TileMode { Toggle, Settings, Action };
struct ModuleView {
    const char* name="";
    TileMode mode=TileMode::Settings;
    bool enabled=false;
    char binding[48]{};
};
struct Placement {
    bool positioned=false,dragging=false;
    ImVec2 position{},offset{};
};
struct State {
    Page page = Page::Combat;
    int theme = 0; // Compatibility with existing callers; one compact client theme.
    std::array<int,7> selectedSections{};
    char search[64]{};
    bool enabledOnly = false;
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
    bool settingsOpen=false;
    Page settingsPage=Page::Combat;
    int settingsSection=0;
    Page renderedSettingsPage=Page::Combat;
    int renderedSettingsSection=-1;
    bool settingsWasOpen=false;
    Placement settingsPlacement{};
    char settingsLabel[64]{};
};
struct Status {
    bool connected = false;
    const char* agent = "waiting for agent";
    const char* version = "detecting";
    std::uint32_t pid = 0;
    const char* memory = "";
};
struct Actions { bool close = false; bool reset = false; };
using Content = void (*)(void*, Page);
using Catalog = int (*)(void*,Page,std::span<ModuleView>);
using Activate = void (*)(void*,Page,int);
// Nur das gewaehlte Modul zeichnen; bestehende Einstellungen bleiben erhalten.
class SectionScope final {
public:
    explicit SectionScope(int section);
    ~SectionScope();
    SectionScope(const SectionScope&)=delete;
    SectionScope& operator=(const SectionScope&)=delete;
private:
    int previous_;
};

// Responsive client workspace; controls remain renderer independent.
// presentation state only. The caller owns all real module settings and actions.
Actions Render(State& state, const Status& status, void* owner, Content content,
               Catalog catalog=nullptr,Activate activate=nullptr);
Actions RenderMenu(State& state, const Status& status, void* owner, Content content,
                   Catalog catalog=nullptr,Activate activate=nullptr);
const char* ThemeName(int index);
float Scale();
bool MotionAllowed(const State& state);
void Section(const char* label);
// SectionScope selects the existing controls inside the independent settings pane.
// Ohne Scope bleibt die native Tab-Navigation fuer bestehende Aufrufer erhalten.
int SubTabs(const char* id, const char* const* labels, int count, int selected,
            bool selectNext = false);

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

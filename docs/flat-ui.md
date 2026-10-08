# Bestehende Flat- und HUD-Helfer

Diese API bleibt für bestehende HUD-/Entwickleroberflächen und das globale
Basistheme verfügbar. Sie ist nicht mehr das Menü-/Injector-Design: beide
verwenden jetzt das lokale `LiquidStyle` und den D3D11-Glasrenderer aus
[liquid-ui.md](liquid-ui.md). Das modulare HUD behält sein bisheriges Aussehen.
Bedienung, HUD-Editor und Keybinds stehen in [client-ui.md](client-ui.md).

Die gemeinsame Implementierung liegt in `src/common/flat_style.hpp/.cpp`.
Nach `ImGui::CreateContext()` einmal das Theme anwenden:

```cpp
mcov::style::ApplyFlatTheme(ImGui::GetStyle());
```

Das Theme verwendet deckende Farben: Hintergrund `#121214`, Karten `#1E1E24`,
Hover `#2A2A32`, Akzent `#5865F2`, Text `#ECECEE`. Die Rundungen sind
Window 10, Frame 6, Popup 8, Grab 5 und Scrollbar 6 Pixel.
Es setzt den Style vor jeder Anwendung zurück. Optionale DPI-Skalierung erfolgt
danach einmal mit `ScaleAllSizes()`, damit sie sich nicht aufaddiert.

## Modul-Container

```cpp
#include "common/flat_style.hpp"
#include "common/shared_state.hpp"

void DrawModulePanel(const mcov::WorldSnapshot& snapshot, bool& enabled) {
    mcov::style::FlatPanel panel("##module-panel", ImVec2(360, 220));
    if (!panel.visible()) return;

    mcov::style::SectionHeader("OVERLAY MODULE");
    mcov::style::ToggleSwitch("Enabled##module-enabled", &enabled);
    ImGui::Text("FPS: %d", snapshot.fps);
    ImGui::Text("Entities: %u", snapshot.entityCount);
}
```

`FlatPanel` reserviert 16 Pixel außen für den Schatten und 14 Pixel innen für
Inhalte. Fünf versetzte schwarze `AddRectFilled`-Flächen erzeugen den Schatten;
anschließend deckt die matte Kartenfläche dessen Innenbereich ab. Der Schatten
wird auf der DrawList des Elternfensters vor `BeginChild()` gezeichnet. Das
Child clippt und scrollt die Inhalte unabhängig. `EndChild()` und `EndGroup()`
erfolgen im Destruktor, auch bei einem frühen `return` oder unsichtbaren Child.
Eindeutige IDs sind erforderlich, wenn mehrere Panels im selben Fenster stehen.

Für absolut positionierte HUD-Karten reicht:

```cpp
mcov::style::DrawFlatPanel(*ImGui::GetBackgroundDrawList(),
                          ImVec2(16, 44), ImVec2(248, 164));
```

## Header und Buttons

Globale Abstände, Farben und Rundungen gehören ausschließlich ins Theme.
Custom-Widgets enthalten nur ihre lokalen Abweichungen. `SectionHeader()` und
`AccentButton()` kapseln sie mit `StyleScope`:

```cpp
bool DrawAction(const char* label) {
    mcov::style::StyleScope scope;
    scope.Color(ImGuiCol_Button, mcov::style::kAccent);
    scope.Color(ImGuiCol_ButtonHovered, mcov::style::kHover);
    scope.Var(ImGuiStyleVar_FrameRounding, 6.0f);
    return ImGui::Button(label); // Alle Pops erfolgen beim Verlassen des Scopes.
}
```

Innerhalb eines `StyleScope` keine manuellen Pops verwenden. Verschachtelte
Scopes stellen auch mehrfache Änderungen desselben Style-Slots korrekt zurück.
Die zentralen Helfer sind nicht kopierbar. Für einzelne farbige Textzeilen reicht
`TextColored()`, für mehrere Widgets ein gemeinsamer Scope. Sichtbarer Text und
stabile Widget-ID lassen sich mit `Label##id` trennen. Interaktive Custom-Widgets
nutzen ImGui-Items mit Navigation statt selbstständiger globaler Mausabfragen.

## Rendering und JNI

Der aktuelle Projektstand rendert mit Win32/D3D11. Theme, Panels, Schatten und
Widgets verwenden ausschließlich öffentliche ImGui-APIs und funktionieren auch
mit `imgui_impl_opengl3` in einem OpenGL-3.3-Kontext. Die Graphics-Backend-Migration
ist kein Bestandteil dieses UI-Umbaus.

JNI/JVMTI bleibt vollständig in `mcov_agent.dll`. Der Sampler veröffentlicht
POD-Snapshots über Shared Memory. `Overlay::RunFrame()` übernimmt einen fertigen
Snapshot mit `TryReadSnapshot()` und übergibt ihn als `const WorldSnapshot&` an
die Renderer. Die Flat-Helfer importieren weder JNI noch Agent-Header und fügen
keine JNI-Abfragen hinzu. Der separate Liquid-Glass-Menürenderer baut und
verwendet inzwischen Desktop Duplication, Blur und Refraction; seine
GPU-spezifische Implementierung ist keine portable OpenGL-Variante dieser Flat-
API. Die originalen Demo-Widgets werden nicht übernommen. Der transparente
Framebuffer außerhalb der UI bleibt für das Overlay erforderlich.

## Prüfung

`-DMCOV_BUILD_UI_CHECKS=ON` aktiviert `mcov_ui_check` und den CTest `flat_ui`.
Er prüft die Palette, Style-Stack-Rücksetzung, frühe Returns, geclippte und
scrollbare Panels sowie die Aktivierung eines Toggles über dessen Label.
Der Check läuft ohne Fenster und ohne Minecraft/JVM.

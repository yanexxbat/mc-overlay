# Modernes ImGui-Menue als integrierbares Beispiel

`examples/modern_menu.hpp` ist ein vollstaendiges Header-only-Beispiel fuer die
im Projekt gepinnte Dear-ImGui-Version 1.91.9b. Es liefert `ApplyCustomStyle()`
und `RenderMenu(MenuState&)` im Namespace `mcov::menu_example`. Die vier Tabs
Visuals, Player, World und Settings verwenden eigene Beispielwerte. Diese Werte
muessen bei einer produktiven Integration mit den vorhandenen Modulsettings
verbunden werden. Der bestehende Client und Launcher werden nicht umgestellt.

```cpp
#include "../examples/modern_menu.hpp" // Von src/ aus; Pfad ggf. anpassen.

// Nach ImGui::CreateContext(), vor dem ersten NewFrame():
mcov::menu_example::ApplyCustomStyle();
ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
// Optional: ImGui::GetStyle().ScaleAllSizes(dpiScale); genau einmal.

// Als Member/ausserhalb der Render-Schleife anlegen:
mcov::menu_example::MenuState menu;

// Je Frame zwischen NewFrame() und Render():
mcov::menu_example::RenderMenu(menu);
```

`menu.open` kann durch die vorhandene Menue-Taste umgeschaltet werden. Der
Close-Button setzt es auf `false`. Das Fenster laesst sich ueber freie
Hintergrundflaechen ziehen, sofern `ConfigWindowsMoveFromTitleBarOnly` deaktiviert
ist (ImGui-Standard). Das Beispiel skaliert mit den Style-Abstaenden und reduziert
seine Groesse auf den Viewport. Unter 380 x 260 UI-Pixeln verfuegbarer Menueflaeche
wird es nicht gezeichnet.

Das globale Theme veraendert auch andere ImGui-Oberflaechen. Fuer einen Austausch
des vorhandenen lokalen `LiquidStyle` die Beispielpalette deshalb ebenfalls in
einen lokalen Style-Scope uebernehmen. `ApplyCustomStyle()` nicht jeden Frame
aufrufen; Fonts, Texturen und Backend-Initialisierung gehoeren weiterhin zum
vorhandenen Overlay-Lifecycle.

Die Root-Flaeche verwendet Alpha 0.88, die Child-Flaechen nur eine leichte
Toenung. Text bleibt bei Alpha 1.0. Das ist performante Transluzenz ohne
Backdrop-Aufnahme oder Blur-Pass. Echte Hintergrundunschaerfe benoetigt den
bereits vorhandenen D3D11-Glasrenderer. Die Overlay-Ausgabe muss wie bisher
mit transparentem Clear und Alpha-Compositing konfiguriert sein.

Visuelle Richtung: Operate, anthrazitfarbene Flaechen, dezentes Cyan, 16/12/7
Pixel fuer Fenster/Child/Frames, zwei unabhaengig scrollende Bereiche und
runde Navigationstasten mit sichtbarem Hover-, Active- und Tastaturfokuszustand.
Dieses Beispiel ist kein neues globales Designsystem fuer den Client.

# Kompaktes, transparentes Client- und Injector-Theme

Die visuelle Referenz ist `design/reference-translucent-menu.png`, der vom Benutzer
bereitgestellte Screenshot. Er ersetzt das erste Cyan-Beispiel und das bisherige
Kachel-Menue des Clients. Ziel ist eine breite, ruhige anthrazitfarbene Oberflaeche
mit schmaler Sidebar, oberen Modul-Tabs, kleinen Reglern und gedaempftem Korallorange.
Echte mcov-Module bleiben erhalten; Bezeichnungen/Funktionen aus dem fremden
Referenzprogramm werden nicht als neue Features uebernommen.

Das produktive Menue liegt in `src/overlay/control_panel.cpp`: `RenderMenu()` wird
ueber den bestehenden kompatiblen `Render()`-Einstieg von `Overlay::DrawMenu()`
aufgerufen. `src/common/compact_style.hpp` stellt `ApplyCustomStyle()` bereit.
`CompactStyle` wendet das Theme lokal an und stellt danach Style und Font wieder her.
Der Injector verwendet dasselbe freigegebene Theme und denselben lokalen
`CompactStyle`; die fruehere Lavender-/Liquid-Glass-Darstellung ist abgeloest.

## Layout und Bedienung

- Root: 1040 x 520 Pixel, auf den Viewport minus 24 Pixel begrenzt und beim Ziehen
  im Viewport gehalten. Rahmenlos mit 12 Pixel Rundung und Alpha 0.88.
- Sidebar: 150 Pixel, unter 620 Pixel Fensterbreite 112 Pixel. Sie enthaelt
  sieben Kategorien, eine Suche und einen leisen Verbindungsstatus.
- Die 76 vorhandenen Module/Abschnitte sind direkt ueber obere Tabs erreichbar.
  Ueberlaufende Tabs scrollen horizontal mit den nativen ImGui-Pfeilen.
- Die Suche filtert die Modulnamen der Kategorie ohne Gross-/Kleinschreibung.
  Ein leerer Trefferzustand bietet Clear search. Kategorienwechsel leert die Suche.
- Genau zwei Child-Bereiche: Navigation und Content. Die Tabs stehen oberhalb des
  Content-Scrollbereichs und bleiben sichtbar. Jede Kategorie behaelt ihren
  gewaehlten Abschnitt; ein Modul-/Kategorienwechsel setzt Content-Scroll zurueck.
- Ab 660 Pixel Content-Breite und bei mehr als zwei Toggle-Modulen stehen links
  Detailregler und rechts Quick toggles im Verhaeltnis 60/40
  fuer andere Module derselben Kategorie. Bei schmaleren Fenstern bleiben alle
  Abschnitte ueber Tabs und der aktuelle Enable-Schalter erreichbar.
- Labels und Werte haben eigene Tabellenspalten. Checkboxen und Farbfelder sitzen
  rechts; Labels schalten Checkboxen ebenfalls. Keybinds stehen vor der Checkbox.
  Native Slider unterstuetzen Maus, Tastatur und Ctrl-Klick zur Zahleneingabe.
- Unter 420 Pixel Content-Breite steht in Keybinds jeder Modulname in einer
  eigenen Zeile ueber dem Tastenfeld und Toggle/Hold-Modus. Tastenfelder bleiben
  innerhalb der verfuegbaren Breite; Widget-IDs bleiben beim Layoutwechsel stabil.
- Header ziehen, Center, kategorienbezogenes Reset und Close funktionieren weiter.
  Enable und Run action im Header sind an die vorhandenen Module gebunden.
  HUD-Editor, Keybinds, Friend-Listen und Minecraft/JNI-Logik bleiben erhalten.

## Injector

Der Launcher oeffnet mit 640 x 640 Pixeln und 12 Pixel Aussenabstand.
Die Start/Diagnose-Sidebar ist 142 Pixel breit, unter 620 Pixel Viewport-Breite
112 Pixel. Kleine selbst gezeichnete Icons und 14-Pixel-Inter entsprechen dem
Client. Die echte Instanzliste zeigt PID/Java/Client-Status und den vollstaendigen
Fenstertitel im Tooltip. Status, JDK-Auswahl und Diagnose scrollen; der
34-Pixel-Startbutton und Schliessen-Footer bleiben fest erreichbar, auch bei
480 x 380. Aktiver Starttext ist fast schwarz, deaktivierter Text weiss mit dem
gemeinsamen DisabledAlpha. Native Titelleisten-Drag, Minimieren/Schliessen,
asynchroner Start, Instanzauswahl, JDK, Logs und Packaging bleiben erhalten.
Weitere Bedienung und Laufzeitdetails: [launcher.md](launcher.md).

## Material und Leistung

Anthrazit `#222229` mit Alpha 0.88 wird direkt ueber die transparente
Overlay-Ausgabe komponiert. Die Sidebar ist leicht dunkler. Text `#ECECEF`,
Sekundaertext `#AEAEB8`, Akzent `#EF8B70`, aktiv `#FFA388`. Kleine Controls haben
4 Pixel Rundung; Child-Style 8, Popups 6 Pixel. Fenster- und Frame-Border sind 0.

Client und Injector zeichnen gewoehnliche ImGui-Geometrie und benoetigen weder
eine Desktop-Aufnahme noch Blur oder Refraction. Der bestehende Glasrenderer bleibt
im Client inaktiv. `FlatApp` hat LiquidGlass-Member, Initialisierung, BeginFrame,
Rendern und Shutdown entfernt. Der native Injector loescht RGBA auf 0 und verwendet
DWM-Margins −1 sowie eine Fensterregion mit 24 Pixel Eckdurchmesser passend zum
12-Pixel-Radius. Der erste Frame wird vor `ShowWindow` praesentiert.
Die eingestellte Capture-Affinity des Clients gilt auch bei offenem Menue.
Settings > Overlay zeigt Translucent dark / 88% opacity und Not required an.
Material, Lizenz und historische Capture-Tests des weiterhin vorhandenen
Glasrenderers stehen in [liquid-ui.md](liquid-ui.md).

## Verifikation

Der native D3D11/WARP-Check prueft alle 76 Modul-Tabs, echte Toggle-Aktionen,
Tastaturbedienung, Suche, Style-/Font-/Window-Stack, Drag, kompakte Darstellung,
Slider inklusive sichtbarer Ctrl-Eingabe, Keybinds, HUD und bestehende Renderer.
Offscreen-Vorschauen mit synthetischem Hintergrund beweisen die ImGui-Darstellung
und Alpha-Komposition; sie sind keine Live-Aufnahme aus Minecraft.
Aktuelle Ansichten:
`.impeccable/review/compact-client/{combat-desktop,aim-desktop,aim-compact,visuals-desktop,keybinds-desktop,keybinds-compact,keybinds-compact-bottom}.png`.

Die aktuellen Injector-Checks bestehen fuer Auswahl, Start, Verhinderung doppelter
Starts, beide Sidebar-Tabs, feste Footer und Titel-Minimieren/Schliessen. Der native
versteckte HWND-/Swapchain-Check prueft Panel-Alpha, transparente Aussenraender,
lesbare Controls, Resize und Fensterregion. Alle neun Bilder unter
`.impeccable/review/compact-injector/` sind freigegeben. Die `launcher-native-*`
Dateien sind versteckte Swapchain-Ausgaben vor OS-Clipping; sie beweisen keine
Live-Desktop-Komposition oder Minecraft-Optik. `mcov`, `mcov_injector`, native/UI-
Checks und das portable ZIP mit exakten Payloads bestehen.
